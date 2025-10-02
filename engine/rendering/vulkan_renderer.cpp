#include "vulkan_renderer.hpp"
#include "vulkan_resources.hpp"
#include "outline_renderer.hpp"
#include "../backend/swap_chain.hpp"
#include "../backend/pipeline.hpp"
#include "../backend/window.hpp"
#include "../backend/buffers.hpp"
#include "../ui/UIRenderer.hpp"
#include "../ui/UIWindow.hpp"
#include "../ECS/ECSRegistry.hpp"
#include "../ECS/components.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <stdexcept>
#include <unordered_map>
#include <memory>

namespace impgine {

// Forward declare structures needed
struct UniformBufferObject {
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

struct PushConstantData {
    alignas(16) glm::mat4 model;
};

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

void cleanupSwapChain(
    VkDevice device,
    VkImageView colorImageView,
    VkImage colorImage,
    VkDeviceMemory colorImageMemory,
    VkImageView depthImageView,
    VkImage depthImage,
    VkDeviceMemory depthImageMemory,
    const std::vector<VkFramebuffer>& swapChainFramebuffers,
    void* pipelinePtr,
    VkRenderPass renderPass,
    SwapChain* swapChain) {

    vkDestroyImageView(device, colorImageView, nullptr);
    vkDestroyImage(device, colorImage, nullptr);
    vkFreeMemory(device, colorImageMemory, nullptr);

    vkDestroyImageView(device, depthImageView, nullptr);
    vkDestroyImage(device, depthImage, nullptr);
    vkFreeMemory(device, depthImageMemory, nullptr);

    for (auto framebuffer : swapChainFramebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }

    // Pipeline and swapChain are managed by unique_ptr in Engine
    // Don't delete them here - just clean up Vulkan resources
    (void)pipelinePtr;  // Pipeline destructor is called by unique_ptr
    (void)swapChain;    // SwapChain destructor is called by unique_ptr

    vkDestroyRenderPass(device, renderPass, nullptr);
}

void recordCommandBuffer(
    VkCommandBuffer commandBuffer,
    uint32_t imageIndex,
    VkRenderPass renderPass,
    const std::vector<VkFramebuffer>& swapChainFramebuffers,
    VkExtent2D swapChainExtent,
    void* pipelinePtr,
    VkPipelineLayout pipelineLayout,
    void* meshCachePtr,
    void* ecsRegistryPtr,
    void* uiRendererPtr,
    void* windowPtr,
    void* uiWindowsPtr,
    void* outlineResourcesPtr,
    uint32_t selectedEntity) {

    Pipeline* pipeline = static_cast<Pipeline*>(pipelinePtr);
    auto& meshCache = *static_cast<std::unordered_map<std::string, MeshGPUResources>*>(meshCachePtr);
    ECSRegistry* registry = static_cast<ECSRegistry*>(ecsRegistryPtr);
    UIRenderer* uiRenderer = static_cast<UIRenderer*>(uiRendererPtr);
    Window* window = static_cast<Window*>(windowPtr);
    auto& uiWindows = *static_cast<std::vector<UIWindow>*>(uiWindowsPtr);
    OutlineResources* outlineResources = static_cast<OutlineResources*>(outlineResourcesPtr);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    // Add memory barrier to ensure previous frame is complete
    VkMemoryBarrier memoryBarrier{};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memoryBarrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    memoryBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0,
        1, &memoryBarrier,
        0, nullptr,
        0, nullptr
    );

    // Compute viewport/scissor for 3D scene (needed for both outline and main rendering)
    VkViewport sceneViewport{};
    VkRect2D sceneScissor{};

    if (uiRenderer) {
        // Ensure layout is up to date before using it (use framebuffer size for DPI correctness)
        int ww = 0, wh = 0;
        window->getFramebufferSize(&ww, &wh);
        uiRenderer->getLayout().setWindowSize(ww, wh);
        uiRenderer->getLayout().computeLayout(uiWindows);

        glm::vec4 rect = uiRenderer->getLayout().getViewportRect();
        sceneViewport.x = rect.x;
        sceneViewport.y = rect.y;
        sceneViewport.width = rect.z;
        sceneViewport.height = rect.w;
        sceneViewport.minDepth = 0.0f;
        sceneViewport.maxDepth = 1.0f;

        sceneScissor.offset = { static_cast<int32_t>(rect.x), static_cast<int32_t>(rect.y) };
        sceneScissor.extent = { static_cast<uint32_t>(rect.z), static_cast<uint32_t>(rect.w) };
    } else {
        sceneViewport.x = 0.0f;
        sceneViewport.y = 0.0f;
        sceneViewport.width = static_cast<float>(swapChainExtent.width);
        sceneViewport.height = static_cast<float>(swapChainExtent.height);
        sceneViewport.minDepth = 0.0f;
        sceneViewport.maxDepth = 1.0f;

        sceneScissor.offset = {0, 0};
        sceneScissor.extent = swapChainExtent;
    }

    // Render outline mask if entity is selected (INVALID_ENTITY is max uint32)
    if (selectedEntity != INVALID_ENTITY && outlineResources && outlineResources->initialized) {
        renderOutlineMask(commandBuffer, *outlineResources, swapChainExtent,
                         meshCachePtr, ecsRegistryPtr, selectedEntity, imageIndex,
                         &sceneViewport, &sceneScissor);
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapChainExtent;

    std::array<VkClearValue, 3> clearValues{};
    clearValues[0].color = {{0.01f, 0.01f, 0.01f, 1.0f}};  // Color attachment (MSAA)
    clearValues[1].depthStencil = {1.0f, 0};                // Depth attachment
    clearValues[2].color = {{0.01f, 0.01f, 0.01f, 1.0f}};  // Resolve attachment
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Set viewport/scissor to UI center viewport so 3D scene renders only in middle
    vkCmdSetViewport(commandBuffer, 0, 1, &sceneViewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &sceneScissor);

    pipeline->bind(commandBuffer);

    const auto entityList = registry->getEntities();
    for (const Entity e : entityList) {
        // Skip inactive entities
        if (!registry->isActiveInHierarchy(e)) {
            continue;
        }

        auto &meshRenderer = registry->getComponent<impgine::MeshRenderer>(e);

        // Get the mesh GPU resources
        auto meshIt = meshCache.find(meshRenderer.mesh->modelPath);
        if (meshIt == meshCache.end()) {
            continue; // Skip if mesh not loaded
        }
        auto& meshResources = meshIt->second;

        // Bind descriptor set for this mesh (includes its texture)
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &meshResources.descriptorSets[imageIndex], 0, nullptr);

        // Bind vertex and index buffers for this entity's mesh
        VkBuffer vertexBuffers[] = {meshResources.vertexBuffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffer, meshResources.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

        // Get world transform matrix (handles hierarchy automatically)
        glm::mat4 model = impgine::Transform::getWorldMatrix(e);

        // Push the model matrix via push constants
        PushConstantData pushData{};
        pushData.model = model;
        vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstantData), &pushData);

        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(meshResources.indices.size()), 1, 0, 0, 0);
    }

    // Composite outline if entity is selected (INVALID_ENTITY is max uint32)
    if (selectedEntity != INVALID_ENTITY && outlineResources && outlineResources->initialized) {
        compositeOutline(commandBuffer, *outlineResources, swapChainExtent, imageIndex,
                        &sceneViewport, &sceneScissor);
    }

    // Draw UI on top
    if (uiRenderer) {
        uiRenderer->record(commandBuffer, *window, uiWindows, imageIndex);
    }

    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }
}

void drawFrame(
    VkDevice device,
    VkQueue graphicsQueue,
    VkQueue presentQueue,
    SwapChain& swapChain,
    const std::vector<VkCommandBuffer>& commandBuffers,
    uint32_t& currentFrame,
    bool& framebufferResized,
    void* enginePtr) {

    // Wait for the previous frame to finish
    VkFence inFlightFence = swapChain.getInFlightFence(currentFrame);
    vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    // Use frame-based semaphore for acquire
    VkSemaphore imageAvailableSemaphore = swapChain.getImageAvailableSemaphore(currentFrame);
    auto result = swapChain.acquireNextImage(&imageIndex, currentFrame, imageAvailableSemaphore);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        // Call recreateSwapChain via engine pointer
        // This is a simplified version - in practice, you'd need a callback
        (void)enginePtr;  // Placeholder - actual implementation needs proper callback
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    // Only reset the fence if we are submitting work
    vkResetFences(device, 1, &inFlightFence);

    // Note: updateUniformBuffer would be called here in the Engine class

    // Reset and record command buffer
    vkResetCommandBuffer(commandBuffers[currentFrame], 0);
    // Note: recordCommandBuffer would be called here with proper parameters

    // Submit command buffer
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {imageAvailableSemaphore};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

    VkSemaphore signalSemaphores[] = {swapChain.getRenderFinishedSemaphore(imageIndex)};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFence) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit draw command buffer!");
    }

    // Present frame
    result = swapChain.presentFrame(presentQueue, &imageIndex, currentFrame);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
        framebufferResized = false;
        // Call recreateSwapChain via engine pointer
        // This is a simplified version - in practice, you'd need a callback
        (void)enginePtr;  // Placeholder
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("failed to present swap chain image!");
    }

    currentFrame = (currentFrame + 1) % SwapChain::MAX_FRAMES_IN_FLIGHT;
}

void createCommandBuffers(
    VkDevice device,
    VkCommandPool commandPool,
    uint32_t maxFramesInFlight,
    std::vector<VkCommandBuffer>& commandBuffers) {

    commandBuffers.resize(maxFramesInFlight);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

    if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }
}

void recreateSwapChain(
    VkDevice device,
    VkPhysicalDevice physicalDevice,
    VkSurfaceKHR surface,
    Window& window,
    SwapChain*& swapChain,
    VkImage& colorImage,
    VkDeviceMemory& colorImageMemory,
    VkImageView& colorImageView,
    VkImage& depthImage,
    VkDeviceMemory& depthImageMemory,
    VkImageView& depthImageView,
    VkRenderPass& renderPass,
    std::vector<VkFramebuffer>& swapChainFramebuffers,
    std::vector<VkBuffer>& uniformBuffers,
    std::vector<VkDeviceMemory>& uniformBuffersMemory,
    VkDescriptorPool& descriptorPool,
    VkDescriptorSetLayout descriptorSetLayout,
    void* meshCachePtr,
    VkSampleCountFlagBits msaaSamples,
    void* pipelinePtr,
    VkPipelineLayout pipelineLayout,
    const std::string& vertShaderPath,
    const std::string& fragShaderPath,
    void* uiRendererPtr,
    VkCommandPool commandPool,
    VkQueue graphicsQueue,
    const std::string& uiVertShaderPath,
    const std::string& uiFragShaderPath,
    const std::string& fontPath) {

    int width = 0, height = 0;
    window.getFramebufferSize(&width, &height);
    while (width == 0 || height == 0) {
        window.getFramebufferSize(&width, &height);
        window.waitEvents();
    }

    vkDeviceWaitIdle(device);

    // Clean up old uniform buffers and descriptor pool
    for (size_t i = 0; i < uniformBuffers.size(); i++) {
        vkDestroyBuffer(device, uniformBuffers[i], nullptr);
        vkFreeMemory(device, uniformBuffersMemory[i], nullptr);
    }

    vkDestroyDescriptorPool(device, descriptorPool, nullptr);

    // Clean up Vulkan resources but don't delete the managed objects
    cleanupSwapChain(device, colorImageView, colorImage, colorImageMemory,
                     depthImageView, depthImage, depthImageMemory,
                     swapChainFramebuffers, pipelinePtr, renderPass, swapChain);

    // Delete old managed objects now that Vulkan resources are cleaned up
    delete swapChain;
    Pipeline* pipeline = *static_cast<Pipeline**>(pipelinePtr);
    delete pipeline;

    swapChain = new SwapChain(device, physicalDevice, surface, window);

    createColorResources(device, physicalDevice, swapChain->getSwapChainImageFormat(),
                        swapChain->getSwapChainExtent(), msaaSamples,
                        colorImage, colorImageMemory, colorImageView);

    createDepthResources(device, physicalDevice, swapChain->getSwapChainExtent(),
                        msaaSamples, depthImage, depthImageMemory, depthImageView);

    createRenderPass(device, physicalDevice, swapChain->getSwapChainImageFormat(),
                    msaaSamples, renderPass);

    std::vector<VkImageView> imageViews;
    for (size_t i = 0; i < swapChain->imageCount(); i++) {
        imageViews.push_back(swapChain->getImageView(i));
    }
    createFramebuffers(device, renderPass, swapChain->getSwapChainExtent(),
                      imageViews, colorImageView, depthImageView, swapChainFramebuffers);

    // Recreate uniform buffers and descriptor resources for new swap chain
    createUniformBuffers(device, physicalDevice, swapChain->imageCount(),
                        uniformBuffers, uniformBuffersMemory);

    auto& meshCache = *static_cast<std::unordered_map<std::string, MeshGPUResources>*>(meshCachePtr);
    createDescriptorPool(device, static_cast<uint32_t>(meshCache.size()),
                        swapChain->imageCount(), descriptorPool);

    createDescriptorSets(device, descriptorPool, descriptorSetLayout, uniformBuffers,
                        swapChain->imageCount(), meshCachePtr);

    // Recreate pipeline since it depends on render pass
    // Note: pipeline was already deleted in cleanupSwapChain, so create a new one
    PipelineConfigInfo pipelineConfig{};
    Pipeline::defaultPipelineConfigInfo(pipelineConfig);
    pipelineConfig.renderPass = renderPass;
    pipelineConfig.pipelineLayout = pipelineLayout;
    pipelineConfig.multisampleInfo.rasterizationSamples = msaaSamples;

    pipeline = new Pipeline(device, vertShaderPath, fragShaderPath, pipelineConfig);
    *static_cast<Pipeline**>(pipelinePtr) = pipeline;

    // Recreate UI renderer with new render pass and extent
    UIRenderer* uiRenderer = *static_cast<UIRenderer**>(uiRendererPtr);
    if (uiRenderer) {
        delete uiRenderer;
        uiRenderer = new UIRenderer(device, physicalDevice, renderPass, pipelineLayout,
                                   commandPool, graphicsQueue, *swapChain, msaaSamples,
                                   uiVertShaderPath, uiFragShaderPath);

        // Reload font and create descriptor sets
        uiRenderer->getTextRenderer()->loadFont(fontPath, 48);
        uiRenderer->createDescriptorSets();

        *static_cast<UIRenderer**>(uiRendererPtr) = uiRenderer;
    }
}

void framebufferResizeCallback(void* windowPtr, int width, int height, void* enginePtr) {
    (void)width;
    (void)height;
    (void)windowPtr;

    // In the actual implementation, this would set framebufferResized = true on the Engine
    // For now, this is a placeholder that shows the structure
    struct Engine {
        bool framebufferResized;
    };

    Engine* app = static_cast<Engine*>(enginePtr);
    app->framebufferResized = true;
}

} // namespace impgine
