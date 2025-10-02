#include "outline_renderer.hpp"
#include "vulkan_resources.hpp"
#include "../backend/swap_chain.hpp"
#include "../backend/pipeline.hpp"
#include "../ECS/ECSRegistry.hpp"
#include "../ECS/components.hpp"
#include "../backend/buffers.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>
#include <array>
#include <unordered_map>
#include <fstream>

namespace impgine {

// Helper function to read SPIR-V file
static std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file: " + filename);
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

// Helper function to create shader module
static VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("failed to create shader module!");
    }

    return shaderModule;
}

// Forward declarations
struct MeshGPUResources {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;
    VkImage textureImage;
    VkDeviceMemory textureImageMemory;
    VkImageView textureImageView;
    VkSampler textureSampler;
    uint32_t mipLevels;
    std::vector<VkDescriptorSet> descriptorSets;
};

struct UniformBufferObject {
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

struct PushConstantData {
    alignas(16) glm::mat4 model;
};

static VkRenderPass createMaskRenderPass(VkDevice device) {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = VK_FORMAT_R8_UNORM;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = VK_FORMAT_D32_SFLOAT;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthRef{};
    depthRef.attachment = 1;
    depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    VkRenderPass renderPass;
    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create outline mask render pass!");
    }

    return renderPass;
}

static void createMaskImage(
    VkDevice device,
    VkPhysicalDevice physicalDevice,
    VkExtent2D extent,
    VkImage& image,
    VkDeviceMemory& memory,
    VkImageView& imageView) {

    createImage(device, physicalDevice, extent.width, extent.height, 1,
                VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R8_UNORM,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                image, memory);

    imageView = createImageView(device, image, VK_FORMAT_R8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 1);
}

static void createMaskDepthImage(
    VkDevice device,
    VkPhysicalDevice physicalDevice,
    VkExtent2D extent,
    VkImage& image,
    VkDeviceMemory& memory,
    VkImageView& imageView) {

    createImage(device, physicalDevice, extent.width, extent.height, 1,
                VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_D32_SFLOAT,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                image, memory);

    imageView = createImageView(device, image, VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT, 1);
}

void initOutlineResources(
    VkDevice device,
    VkPhysicalDevice physicalDevice,
    VkExtent2D extent,
    VkDescriptorSetLayout mainDescriptorSetLayout,
    uint32_t maxFramesInFlight,
    OutlineResources& resources) {

    if (resources.initialized) {
        return;
    }

    // Create mask render pass
    resources.maskRenderPass = createMaskRenderPass(device);

    // Create mask image and depth buffer
    createMaskImage(device, physicalDevice, extent,
                    resources.maskImage, resources.maskImageMemory, resources.maskImageView);
    createMaskDepthImage(device, physicalDevice, extent,
                         resources.maskDepthImage, resources.maskDepthMemory, resources.maskDepthView);

    // Create mask sampler
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (vkCreateSampler(device, &samplerInfo, nullptr, &resources.maskSampler) != VK_SUCCESS) {
        throw std::runtime_error("failed to create mask sampler!");
    }

    // Create framebuffer
    std::array<VkImageView, 2> attachments = {resources.maskImageView, resources.maskDepthView};

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = resources.maskRenderPass;
    framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    framebufferInfo.pAttachments = attachments.data();
    framebufferInfo.width = extent.width;
    framebufferInfo.height = extent.height;
    framebufferInfo.layers = 1;

    if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &resources.maskFramebuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to create outline mask framebuffer!");
    }

    // Create outline pipeline layout (uses main descriptor layout for UBO)
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(PushConstantData);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &mainDescriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &resources.outlinePipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create outline pipeline layout!");
    }

    // Load outline shaders
    std::vector<char> outlineVertCode = readFile("example/Engine/Shaders/outline.vert.spv");
    std::vector<char> outlineFragCode = readFile("example/Engine/Shaders/outline.frag.spv");

    VkShaderModule vertShaderModule = createShaderModule(device, outlineVertCode);
    VkShaderModule fragShaderModule = createShaderModule(device, outlineFragCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    // Vertex input
    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Viewport and scissor
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(extent.width);
    viewport.height = static_cast<float>(extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = extent;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    // Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Depth stencil
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    // Color blending
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // Create outline pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = resources.outlinePipelineLayout;
    pipelineInfo.renderPass = resources.maskRenderPass;
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &resources.outlinePipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create outline pipeline!");
    }

    vkDestroyShaderModule(device, fragShaderModule, nullptr);
    vkDestroyShaderModule(device, vertShaderModule, nullptr);

    // Create composite descriptor layout
    VkDescriptorSetLayoutBinding samplerBinding{};
    samplerBinding.binding = 0;
    samplerBinding.descriptorCount = 1;
    samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &samplerBinding;

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &resources.compositeDescriptorLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create composite descriptor set layout!");
    }

    // Create composite descriptor pool
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = maxFramesInFlight;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = maxFramesInFlight;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &resources.compositeDescriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create composite descriptor pool!");
    }

    // Allocate descriptor sets
    std::vector<VkDescriptorSetLayout> layouts(maxFramesInFlight, resources.compositeDescriptorLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = resources.compositeDescriptorPool;
    allocInfo.descriptorSetCount = maxFramesInFlight;
    allocInfo.pSetLayouts = layouts.data();

    resources.compositeDescriptorSets.resize(maxFramesInFlight);
    if (vkAllocateDescriptorSets(device, &allocInfo, resources.compositeDescriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate composite descriptor sets!");
    }

    // Update descriptor sets
    for (size_t i = 0; i < maxFramesInFlight; i++) {
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = resources.maskImageView;
        imageInfo.sampler = resources.maskSampler;

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = resources.compositeDescriptorSets[i];
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
    }

    // Create composite pipeline layout
    VkPushConstantRange compositePushConstant{};
    compositePushConstant.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    compositePushConstant.offset = 0;
    compositePushConstant.size = sizeof(OutlinePushConstants);

    VkPipelineLayoutCreateInfo compositeLayoutInfo{};
    compositeLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    compositeLayoutInfo.setLayoutCount = 1;
    compositeLayoutInfo.pSetLayouts = &resources.compositeDescriptorLayout;
    compositeLayoutInfo.pushConstantRangeCount = 1;
    compositeLayoutInfo.pPushConstantRanges = &compositePushConstant;

    if (vkCreatePipelineLayout(device, &compositeLayoutInfo, nullptr, &resources.compositePipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create composite pipeline layout!");
    }

    // Load composite shaders (fullscreen quad with edge detection)
    std::vector<char> compositeVertCode = readFile("example/Engine/Shaders/outline_composite.vert.spv");
    std::vector<char> compositeFragCode = readFile("example/Engine/Shaders/outline_composite.frag.spv");

    VkShaderModule compositeVertModule = createShaderModule(device, compositeVertCode);
    VkShaderModule compositeFragModule = createShaderModule(device, compositeFragCode);

    VkPipelineShaderStageCreateInfo compositeVertStage{};
    compositeVertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    compositeVertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    compositeVertStage.module = compositeVertModule;
    compositeVertStage.pName = "main";

    VkPipelineShaderStageCreateInfo compositeFragStage{};
    compositeFragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    compositeFragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    compositeFragStage.module = compositeFragModule;
    compositeFragStage.pName = "main";

    VkPipelineShaderStageCreateInfo compositeShaderStages[] = {compositeVertStage, compositeFragStage};

    // Vertex input (none - fullscreen triangle generated in shader)
    VkPipelineVertexInputStateCreateInfo compositeVertexInput{};
    compositeVertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    compositeVertexInput.vertexBindingDescriptionCount = 0;
    compositeVertexInput.vertexAttributeDescriptionCount = 0;

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo compositeInputAssembly{};
    compositeInputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    compositeInputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    compositeInputAssembly.primitiveRestartEnable = VK_FALSE;

    // Viewport and scissor (dynamic)
    VkViewport compositeViewport{};
    compositeViewport.x = 0.0f;
    compositeViewport.y = 0.0f;
    compositeViewport.width = static_cast<float>(extent.width);
    compositeViewport.height = static_cast<float>(extent.height);
    compositeViewport.minDepth = 0.0f;
    compositeViewport.maxDepth = 1.0f;

    VkRect2D compositeScissor{};
    compositeScissor.offset = {0, 0};
    compositeScissor.extent = extent;

    VkPipelineViewportStateCreateInfo compositeViewportState{};
    compositeViewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    compositeViewportState.viewportCount = 1;
    compositeViewportState.pViewports = &compositeViewport;
    compositeViewportState.scissorCount = 1;
    compositeViewportState.pScissors = &compositeScissor;

    // Rasterizer
    VkPipelineRasterizationStateCreateInfo compositeRasterizer{};
    compositeRasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    compositeRasterizer.depthClampEnable = VK_FALSE;
    compositeRasterizer.rasterizerDiscardEnable = VK_FALSE;
    compositeRasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    compositeRasterizer.lineWidth = 1.0f;
    compositeRasterizer.cullMode = VK_CULL_MODE_NONE;
    compositeRasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    compositeRasterizer.depthBiasEnable = VK_FALSE;

    // Multisampling
    VkPipelineMultisampleStateCreateInfo compositeMultisampling{};
    compositeMultisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    compositeMultisampling.sampleShadingEnable = VK_FALSE;
    compositeMultisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // No depth testing
    VkPipelineDepthStencilStateCreateInfo compositeDepthStencil{};
    compositeDepthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    compositeDepthStencil.depthTestEnable = VK_FALSE;
    compositeDepthStencil.depthWriteEnable = VK_FALSE;

    // Color blending (alpha blending for outline)
    VkPipelineColorBlendAttachmentState compositeColorBlendAttachment{};
    compositeColorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    compositeColorBlendAttachment.blendEnable = VK_TRUE;
    compositeColorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    compositeColorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    compositeColorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    compositeColorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    compositeColorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    compositeColorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo compositeColorBlending{};
    compositeColorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    compositeColorBlending.logicOpEnable = VK_FALSE;
    compositeColorBlending.attachmentCount = 1;
    compositeColorBlending.pAttachments = &compositeColorBlendAttachment;

    // Composite pipeline will be created separately after render pass is available
    vkDestroyShaderModule(device, compositeFragModule, nullptr);
    vkDestroyShaderModule(device, compositeVertModule, nullptr);

    resources.initialized = true;
}

void createOutlineCompositePipeline(
    VkDevice device,
    VkRenderPass renderPass,
    VkExtent2D extent,
    VkSampleCountFlagBits msaaSamples,
    OutlineResources& resources) {

    if (!resources.initialized) {
        return;
    }

    // Load composite shaders
    std::vector<char> compositeVertCode = readFile("example/Engine/Shaders/outline_composite.vert.spv");
    std::vector<char> compositeFragCode = readFile("example/Engine/Shaders/outline_composite.frag.spv");

    VkShaderModule compositeVertModule = createShaderModule(device, compositeVertCode);
    VkShaderModule compositeFragModule = createShaderModule(device, compositeFragCode);

    VkPipelineShaderStageCreateInfo compositeVertStage{};
    compositeVertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    compositeVertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    compositeVertStage.module = compositeVertModule;
    compositeVertStage.pName = "main";

    VkPipelineShaderStageCreateInfo compositeFragStage{};
    compositeFragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    compositeFragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    compositeFragStage.module = compositeFragModule;
    compositeFragStage.pName = "main";

    VkPipelineShaderStageCreateInfo compositeShaderStages[] = {compositeVertStage, compositeFragStage};

    // Vertex input (none - fullscreen triangle generated in shader)
    VkPipelineVertexInputStateCreateInfo compositeVertexInput{};
    compositeVertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    compositeVertexInput.vertexBindingDescriptionCount = 0;
    compositeVertexInput.vertexAttributeDescriptionCount = 0;

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo compositeInputAssembly{};
    compositeInputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    compositeInputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    compositeInputAssembly.primitiveRestartEnable = VK_FALSE;

    // Viewport and scissor
    VkViewport compositeViewport{};
    compositeViewport.x = 0.0f;
    compositeViewport.y = 0.0f;
    compositeViewport.width = static_cast<float>(extent.width);
    compositeViewport.height = static_cast<float>(extent.height);
    compositeViewport.minDepth = 0.0f;
    compositeViewport.maxDepth = 1.0f;

    VkRect2D compositeScissor{};
    compositeScissor.offset = {0, 0};
    compositeScissor.extent = extent;

    VkPipelineViewportStateCreateInfo compositeViewportState{};
    compositeViewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    compositeViewportState.viewportCount = 1;
    compositeViewportState.pViewports = &compositeViewport;
    compositeViewportState.scissorCount = 1;
    compositeViewportState.pScissors = &compositeScissor;

    // Rasterizer
    VkPipelineRasterizationStateCreateInfo compositeRasterizer{};
    compositeRasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    compositeRasterizer.depthClampEnable = VK_FALSE;
    compositeRasterizer.rasterizerDiscardEnable = VK_FALSE;
    compositeRasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    compositeRasterizer.lineWidth = 1.0f;
    compositeRasterizer.cullMode = VK_CULL_MODE_NONE;
    compositeRasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    compositeRasterizer.depthBiasEnable = VK_FALSE;

    // Multisampling (match main render pass)
    VkPipelineMultisampleStateCreateInfo compositeMultisampling{};
    compositeMultisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    compositeMultisampling.sampleShadingEnable = VK_FALSE;
    compositeMultisampling.rasterizationSamples = msaaSamples;

    // No depth testing
    VkPipelineDepthStencilStateCreateInfo compositeDepthStencil{};
    compositeDepthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    compositeDepthStencil.depthTestEnable = VK_FALSE;
    compositeDepthStencil.depthWriteEnable = VK_FALSE;

    // Color blending (alpha blending for outline)
    VkPipelineColorBlendAttachmentState compositeColorBlendAttachment{};
    compositeColorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    compositeColorBlendAttachment.blendEnable = VK_TRUE;
    compositeColorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    compositeColorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    compositeColorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    compositeColorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    compositeColorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    compositeColorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo compositeColorBlending{};
    compositeColorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    compositeColorBlending.logicOpEnable = VK_FALSE;
    compositeColorBlending.attachmentCount = 1;
    compositeColorBlending.pAttachments = &compositeColorBlendAttachment;

    // Create composite pipeline
    VkGraphicsPipelineCreateInfo compositePipelineInfo{};
    compositePipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    compositePipelineInfo.stageCount = 2;
    compositePipelineInfo.pStages = compositeShaderStages;
    compositePipelineInfo.pVertexInputState = &compositeVertexInput;
    compositePipelineInfo.pInputAssemblyState = &compositeInputAssembly;
    compositePipelineInfo.pViewportState = &compositeViewportState;
    compositePipelineInfo.pRasterizationState = &compositeRasterizer;
    compositePipelineInfo.pMultisampleState = &compositeMultisampling;
    compositePipelineInfo.pDepthStencilState = &compositeDepthStencil;
    compositePipelineInfo.pColorBlendState = &compositeColorBlending;
    compositePipelineInfo.layout = resources.compositePipelineLayout;
    compositePipelineInfo.renderPass = renderPass;
    compositePipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &compositePipelineInfo, nullptr, &resources.compositePipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create composite outline pipeline!");
    }

    vkDestroyShaderModule(device, compositeFragModule, nullptr);
    vkDestroyShaderModule(device, compositeVertModule, nullptr);
}

void cleanupOutlineResources(VkDevice device, OutlineResources& resources) {
    if (!resources.initialized) {
        return;
    }

    vkDestroyPipeline(device, resources.compositePipeline, nullptr);
    vkDestroyPipelineLayout(device, resources.compositePipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, resources.compositeDescriptorLayout, nullptr);
    vkDestroyDescriptorPool(device, resources.compositeDescriptorPool, nullptr);

    vkDestroyPipeline(device, resources.outlinePipeline, nullptr);
    vkDestroyPipelineLayout(device, resources.outlinePipelineLayout, nullptr);

    vkDestroyFramebuffer(device, resources.maskFramebuffer, nullptr);
    vkDestroyRenderPass(device, resources.maskRenderPass, nullptr);

    vkDestroySampler(device, resources.maskSampler, nullptr);
    vkDestroyImageView(device, resources.maskImageView, nullptr);
    vkDestroyImage(device, resources.maskImage, nullptr);
    vkFreeMemory(device, resources.maskImageMemory, nullptr);

    vkDestroyImageView(device, resources.maskDepthView, nullptr);
    vkDestroyImage(device, resources.maskDepthImage, nullptr);
    vkFreeMemory(device, resources.maskDepthMemory, nullptr);

    resources.initialized = false;
}

void recreateOutlineResources(
    VkDevice device,
    VkPhysicalDevice physicalDevice,
    VkExtent2D newExtent,
    OutlineResources& resources) {

    // Recreate mask image and framebuffer
    vkDestroyFramebuffer(device, resources.maskFramebuffer, nullptr);
    vkDestroyImageView(device, resources.maskImageView, nullptr);
    vkDestroyImage(device, resources.maskImage, nullptr);
    vkFreeMemory(device, resources.maskImageMemory, nullptr);
    vkDestroyImageView(device, resources.maskDepthView, nullptr);
    vkDestroyImage(device, resources.maskDepthImage, nullptr);
    vkFreeMemory(device, resources.maskDepthMemory, nullptr);

    createMaskImage(device, physicalDevice, newExtent,
                    resources.maskImage, resources.maskImageMemory, resources.maskImageView);
    createMaskDepthImage(device, physicalDevice, newExtent,
                         resources.maskDepthImage, resources.maskDepthMemory, resources.maskDepthView);

    std::array<VkImageView, 2> attachments = {resources.maskImageView, resources.maskDepthView};

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = resources.maskRenderPass;
    framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    framebufferInfo.pAttachments = attachments.data();
    framebufferInfo.width = newExtent.width;
    framebufferInfo.height = newExtent.height;
    framebufferInfo.layers = 1;

    if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &resources.maskFramebuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to recreate outline mask framebuffer!");
    }

    // Update descriptor sets with new image view
    for (size_t i = 0; i < resources.compositeDescriptorSets.size(); i++) {
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = resources.maskImageView;
        imageInfo.sampler = resources.maskSampler;

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = resources.compositeDescriptorSets[i];
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
    }
}

void renderOutlineMask(
    VkCommandBuffer commandBuffer,
    const OutlineResources& resources,
    VkExtent2D extent,
    void* meshCachePtr,
    void* ecsRegistryPtr,
    uint32_t selectedEntity,
    uint32_t imageIndex,
    const VkViewport* viewport,
    const VkRect2D* scissor) {

    auto& meshCache = *static_cast<std::unordered_map<std::string, MeshGPUResources>*>(meshCachePtr);
    ECSRegistry* registry = static_cast<ECSRegistry*>(ecsRegistryPtr);

    // Begin mask render pass
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = resources.maskRenderPass;
    renderPassInfo.framebuffer = resources.maskFramebuffer;
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = extent;

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Set viewport and scissor if provided
    if (viewport) {
        vkCmdSetViewport(commandBuffer, 0, 1, viewport);
    } else {
        VkViewport defaultViewport{};
        defaultViewport.x = 0.0f;
        defaultViewport.y = 0.0f;
        defaultViewport.width = static_cast<float>(extent.width);
        defaultViewport.height = static_cast<float>(extent.height);
        defaultViewport.minDepth = 0.0f;
        defaultViewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &defaultViewport);
    }

    if (scissor) {
        vkCmdSetScissor(commandBuffer, 0, 1, scissor);
    } else {
        VkRect2D defaultScissor{};
        defaultScissor.offset = {0, 0};
        defaultScissor.extent = extent;
        vkCmdSetScissor(commandBuffer, 0, 1, &defaultScissor);
    }

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, resources.outlinePipeline);

    // Render the selected entity
    try {
        auto& meshRenderer = registry->getComponent<impgine::MeshRenderer>(selectedEntity);

        auto meshIt = meshCache.find(meshRenderer.mesh->modelPath);
        if (meshIt != meshCache.end()) {
            auto& meshResources = meshIt->second;

            // Bind descriptor set (for UBO)
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    resources.outlinePipelineLayout, 0, 1,
                                    &meshResources.descriptorSets[imageIndex], 0, nullptr);

            // Bind vertex and index buffers
            VkBuffer vertexBuffers[] = {meshResources.vertexBuffer};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
            vkCmdBindIndexBuffer(commandBuffer, meshResources.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

            // Get world transform matrix (handles hierarchy automatically)
            glm::mat4 model = impgine::Transform::getWorldMatrix(selectedEntity);

            PushConstantData pushConstants{};
            pushConstants.model = model;

            vkCmdPushConstants(commandBuffer, resources.outlinePipelineLayout,
                               VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstantData), &pushConstants);

            vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(meshResources.indices.size()), 1, 0, 0, 0);
        }
    } catch (const std::runtime_error&) {
        // Entity doesn't have required components, skip outline rendering
    }

    vkCmdEndRenderPass(commandBuffer);
}

void compositeOutline(
    VkCommandBuffer commandBuffer,
    const OutlineResources& resources,
    VkExtent2D extent,
    uint32_t imageIndex,
    const VkViewport* viewport,
    const VkRect2D* scissor) {

    if (!resources.initialized || resources.compositePipeline == VK_NULL_HANDLE) {
        return;
    }

    // Set viewport and scissor if provided (to match the 3D scene viewport)
    if (viewport) {
        vkCmdSetViewport(commandBuffer, 0, 1, viewport);
    }
    if (scissor) {
        vkCmdSetScissor(commandBuffer, 0, 1, scissor);
    }

    // Bind composite pipeline
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, resources.compositePipeline);

    // Bind descriptor set (mask texture)
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                           resources.compositePipelineLayout, 0, 1,
                           &resources.compositeDescriptorSets[imageIndex], 0, nullptr);

    // Push constants (outline color and texel size)
    OutlinePushConstants pushConstants{};
    pushConstants.outlineColor = resources.outlineColor;
    pushConstants.texelSize = glm::vec2(1.0f / extent.width, 1.0f / extent.height);

    vkCmdPushConstants(commandBuffer, resources.compositePipelineLayout,
                      VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(OutlinePushConstants), &pushConstants);

    // Draw fullscreen triangle
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);
}

} // namespace impgine
