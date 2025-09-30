#include "UIRenderer.hpp"

#include <cstring>
#include <iostream>

namespace impgine {

    UIRenderer::UIRenderer(VkDevice device, VkPhysicalDevice physicalDevice, VkRenderPass renderPass, VkPipelineLayout pipelineLayout, VkCommandPool commandPool, VkQueue graphicsQueue, SwapChain& swapChain, VkSampleCountFlagBits samples, const std::string& vertSpvPath, const std::string& fragSpvPath)
        : device(device), physicalDevice(physicalDevice), pipelineLayout(pipelineLayout), swapChain(swapChain), samples(samples), vertPath(vertSpvPath), fragPath(fragSpvPath),
          layout(swapChain.getSwapChainExtent().width, swapChain.getSwapChainExtent().height) {

        textRenderer = std::make_unique<TextRenderer>(device, physicalDevice, commandPool, graphicsQueue);

        // Create UI-specific descriptor set layout and pipeline layout
        createDescriptorSetLayout();
        createDescriptorPool();

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 0;
        pipelineLayoutInfo.pPushConstantRanges = nullptr;

        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &uiPipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create UI pipeline layout");
        }

        PipelineConfigInfo cfg{};
        Pipeline::defaultPipelineConfigInfo(cfg);
        Pipeline::enableAlphaBlending(cfg);
        cfg.renderPass = renderPass;
        cfg.pipelineLayout = uiPipelineLayout;
        cfg.multisampleInfo.rasterizationSamples = samples;
        cfg.depthStencilInfo.depthTestEnable = VK_FALSE;
        cfg.depthStencilInfo.depthWriteEnable = VK_FALSE;

        // Provide UI vertex format
        cfg.bindingDescriptions = { UIVertex::getBindingDescription() };
        auto attr = UIVertex::getAttributeDescriptions();
        cfg.attributeDescriptions.assign(attr.begin(), attr.end());

        // UI shaders (path relative to project config)
        pipeline = std::make_unique<Pipeline>(device, vertPath, fragPath, cfg);

        createVertexBuffer(1024 * sizeof(UIVertex));
    }

    UIRenderer::~UIRenderer() {
        if (descriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        }
        if (descriptorSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
        }
        if (uiPipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device, uiPipelineLayout, nullptr);
        }
        if (vertexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, vertexBuffer, nullptr);
        }
        if (vertexMemory != VK_NULL_HANDLE) {
            vkFreeMemory(device, vertexMemory, nullptr);
        }
    }

    void UIRenderer::createVertexBuffer(VkDeviceSize size) {
        VkBufferCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        info.size = size;
        info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vkCreateBuffer(device, &info, nullptr, &vertexBuffer);

        VkMemoryRequirements req{};
        vkGetBufferMemoryRequirements(device, vertexBuffer, &req);

        VkMemoryAllocateInfo alloc{};
        alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc.allocationSize = req.size;
        // Host visible for simplicity
        // Find memory type  - using a basic local function (engine already has one, but not accessible here)
        // We'll choose the first HOST_VISIBLE | HOST_COHERENT type.
        VkPhysicalDeviceMemoryProperties props{};
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &props);
        uint32_t typeIndex = 0;
        for (uint32_t i = 0; i < props.memoryTypeCount; ++i) {
            if ((req.memoryTypeBits & (1u << i)) &&
                (props.memoryTypes[i].propertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) == (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
                typeIndex = i; break;
            }
        }
        alloc.memoryTypeIndex = typeIndex;
        vkAllocateMemory(device, &alloc, nullptr, &vertexMemory);
        vkBindBufferMemory(device, vertexBuffer, vertexMemory, 0);
    }

    void UIRenderer::record(VkCommandBuffer cmd, const Window& window, std::vector<UIWindow>& windows, uint32_t imageIndex) {
        // Update layout dimensions if window was resized
        int ww = 0, wh = 0;
        window.getFramebufferSize(&ww, &wh);
        layout.setWindowSize(ww, wh);

        // Compute layout for docked windows
        layout.computeLayout(windows);

        // Build quads for each window with title bars and borders
        std::vector<UIVertex> verts;
        verts.reserve(windows.size() * 30);  // More vertices for detailed UI

        auto toNDC = [&](glm::vec2 px) {
            float nx = (px.x / static_cast<float>(swapChain.getSwapChainExtent().width)) * 2.0f - 1.0f;
            float ny = (px.y / static_cast<float>(swapChain.getSwapChainExtent().height)) * 2.0f - 1.0f;
            return glm::vec2(nx, -ny);
        };

        auto addQuad = [&](glm::vec2 p, glm::vec2 s, glm::vec4 col) {
            glm::vec2 p0 = toNDC(p);
            glm::vec2 p1 = toNDC({ p.x + s.x, p.y });
            glm::vec2 p2 = toNDC({ p.x + s.x, p.y + s.y });
            glm::vec2 p3 = toNDC({ p.x, p.y + s.y });

            verts.push_back({ p0, col, {0.0f, 0.0f} });
            verts.push_back({ p1, col, {0.0f, 0.0f} });
            verts.push_back({ p2, col, {0.0f, 0.0f} });
            verts.push_back({ p0, col, {0.0f, 0.0f} });
            verts.push_back({ p2, col, {0.0f, 0.0f} });
            verts.push_back({ p3, col, {0.0f, 0.0f} });
        };

        for (const auto& w : windows) {
            if (!w.isVisible) continue;

            glm::vec2 p = w.position;
            glm::vec2 s = w.size;

            // Draw border
            if (w.borderWidth > 0.0f) {
                addQuad(p, { s.x, w.borderWidth }, w.borderColor);  // Top
                addQuad({ p.x, p.y + s.y - w.borderWidth }, { s.x, w.borderWidth }, w.borderColor);  // Bottom
                addQuad(p, { w.borderWidth, s.y }, w.borderColor);  // Left
                addQuad({ p.x + s.x - w.borderWidth, p.y }, { w.borderWidth, s.y }, w.borderColor);  // Right
            }

            // Draw title bar
            glm::vec2 titleBarPos = { p.x + w.borderWidth, p.y + w.borderWidth };
            glm::vec2 titleBarSize = { s.x - 2.0f * w.borderWidth, w.titleBarHeight };
            addQuad(titleBarPos, titleBarSize, w.titleBarColor);

            // Draw content area background
            glm::vec2 contentPos = { p.x + w.borderWidth, p.y + w.borderWidth + w.titleBarHeight };
            glm::vec2 contentSize = { s.x - 2.0f * w.borderWidth, s.y - w.titleBarHeight - 2.0f * w.borderWidth };
            addQuad(contentPos, contentSize, w.backgroundColor);
        }

        // Render "Hello World" text at top-left corner
        renderText("Hello World", {10, 30}, 1.0f, {1.0f, 1.0f, 1.0f, 1.0f}, verts);

        if (verts.empty()) return;

        // Upload to vertex buffer
        void* data = nullptr;
        vkMapMemory(device, vertexMemory, 0, verts.size() * sizeof(UIVertex), 0, &data);
        std::memcpy(data, verts.data(), verts.size() * sizeof(UIVertex));
        vkUnmapMemory(device, vertexMemory);

        pipeline->bind(cmd);

        // Bind descriptor set for font texture
        if (!descriptorSets.empty() && imageIndex < descriptorSets.size()) {
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, uiPipelineLayout, 0, 1, &descriptorSets[imageIndex], 0, nullptr);
        }

        // Set viewport/scissor to full framebuffer
        VkViewport vp{}; vp.x = 0; vp.y = 0; vp.width = static_cast<float>(swapChain.getSwapChainExtent().width); vp.height = static_cast<float>(swapChain.getSwapChainExtent().height); vp.minDepth = 0.0f; vp.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &vp);
        VkRect2D sc{}; sc.offset = {0,0}; sc.extent = swapChain.getSwapChainExtent();
        vkCmdSetScissor(cmd, 0, 1, &sc);

        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer, &offset);
        vkCmdDraw(cmd, static_cast<uint32_t>(verts.size()), 1, 0, 0);
    }

    void UIRenderer::renderText(const std::string& text, glm::vec2 position, float scale, glm::vec4 color, std::vector<UIVertex>& verts) {
        if (!textRenderer) return;

        auto toNDC = [&](glm::vec2 px) {
            float nx = (px.x / static_cast<float>(swapChain.getSwapChainExtent().width)) * 2.0f - 1.0f;
            float ny = (px.y / static_cast<float>(swapChain.getSwapChainExtent().height)) * 2.0f - 1.0f;
            return glm::vec2(nx, -ny);
        };

        float xpos = position.x;
        float ypos = position.y;

        for (char c : text) {
            const Character* ch = textRenderer->getCharacter(c);
            if (!ch) continue;

            float w = ch->size.x * scale;
            float h = ch->size.y * scale;
            float x = xpos + ch->bearing.x * scale;
            float y = ypos - (ch->size.y - ch->bearing.y) * scale;

            glm::vec2 p0 = toNDC({ x, y });
            glm::vec2 p1 = toNDC({ x + w, y });
            glm::vec2 p2 = toNDC({ x + w, y + h });
            glm::vec2 p3 = toNDC({ x, y + h });

            verts.push_back({ p0, color, { ch->texCoordMin.x, ch->texCoordMax.y } });
            verts.push_back({ p1, color, { ch->texCoordMax.x, ch->texCoordMax.y } });
            verts.push_back({ p2, color, { ch->texCoordMax.x, ch->texCoordMin.y } });
            verts.push_back({ p0, color, { ch->texCoordMin.x, ch->texCoordMax.y } });
            verts.push_back({ p2, color, { ch->texCoordMax.x, ch->texCoordMin.y } });
            verts.push_back({ p3, color, { ch->texCoordMin.x, ch->texCoordMin.y } });

            xpos += (ch->advance >> 6) * scale;
        }
    }

    void UIRenderer::createDescriptorSetLayout() {
        VkDescriptorSetLayoutBinding samplerLayoutBinding{};
        samplerLayoutBinding.binding = 0;
        samplerLayoutBinding.descriptorCount = 1;
        samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerLayoutBinding.pImmutableSamplers = nullptr;
        samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &samplerLayoutBinding;

        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create UI descriptor set layout");
        }
    }

    void UIRenderer::createDescriptorPool() {
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSize.descriptorCount = static_cast<uint32_t>(swapChain.imageCount());

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = static_cast<uint32_t>(swapChain.imageCount());

        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create UI descriptor pool");
        }
    }

    void UIRenderer::createDescriptorSets() {
        if (!textRenderer) return;

        std::vector<VkDescriptorSetLayout> layouts(swapChain.imageCount(), descriptorSetLayout);
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(swapChain.imageCount());
        allocInfo.pSetLayouts = layouts.data();

        descriptorSets.resize(swapChain.imageCount());
        if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate UI descriptor sets");
        }

        for (size_t i = 0; i < swapChain.imageCount(); i++) {
            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = textRenderer->getTextureView();
            imageInfo.sampler = textRenderer->getSampler();

            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = descriptorSets[i];
            descriptorWrite.dstBinding = 0;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pImageInfo = &imageInfo;

            vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
        }
    }

} // namespace impgine


