#include "UIRenderer.hpp"

#include <cstring>

namespace impgine {

    UIRenderer::UIRenderer(VkDevice device, VkPhysicalDevice physicalDevice, VkRenderPass renderPass, VkPipelineLayout pipelineLayout, SwapChain& swapChain, VkSampleCountFlagBits samples, const std::string& vertSpvPath, const std::string& fragSpvPath)
        : device(device), physicalDevice(physicalDevice), pipelineLayout(pipelineLayout), swapChain(swapChain), samples(samples), vertPath(vertSpvPath), fragPath(fragSpvPath),
          layout(swapChain.getSwapChainExtent().width, swapChain.getSwapChainExtent().height) {

        PipelineConfigInfo cfg{};
        Pipeline::defaultPipelineConfigInfo(cfg);
        Pipeline::enableAlphaBlending(cfg);
        cfg.renderPass = renderPass;
        cfg.pipelineLayout = pipelineLayout;
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

    void UIRenderer::record(VkCommandBuffer cmd, const Window& window, std::vector<UIWindow>& windows) {
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
            // Convert screen coordinates to NDC
            // Screen: (0,0) at top-left, (ww,wh) at bottom-right
            // NDC: (-1,-1) at top-left, (1,1) at bottom-right
            float nx = (px.x / static_cast<float>(ww)) * 2.0f - 1.0f;
            float ny = (px.y / static_cast<float>(wh)) * 2.0f - 1.0f;
            return glm::vec2(nx, ny);
        };

        auto addQuad = [&](glm::vec2 p, glm::vec2 s, glm::vec4 col) {
            glm::vec2 p0 = toNDC(p);
            glm::vec2 p1 = toNDC({ p.x + s.x, p.y });
            glm::vec2 p2 = toNDC({ p.x + s.x, p.y + s.y });
            glm::vec2 p3 = toNDC({ p.x, p.y + s.y });

            verts.push_back({ p0, col });
            verts.push_back({ p1, col });
            verts.push_back({ p2, col });
            verts.push_back({ p0, col });
            verts.push_back({ p2, col });
            verts.push_back({ p3, col });
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

        if (verts.empty()) return;

        // Upload to vertex buffer
        void* data = nullptr;
        vkMapMemory(device, vertexMemory, 0, verts.size() * sizeof(UIVertex), 0, &data);
        std::memcpy(data, verts.data(), verts.size() * sizeof(UIVertex));
        vkUnmapMemory(device, vertexMemory);

        pipeline->bind(cmd);

        // Set viewport/scissor to full framebuffer
        VkViewport vp{}; vp.x = 0; vp.y = 0; vp.width = static_cast<float>(swapChain.getSwapChainExtent().width); vp.height = static_cast<float>(swapChain.getSwapChainExtent().height); vp.minDepth = 0.0f; vp.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &vp);
        VkRect2D sc{}; sc.offset = {0,0}; sc.extent = swapChain.getSwapChainExtent();
        vkCmdSetScissor(cmd, 0, 1, &sc);

        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer, &offset);
        vkCmdDraw(cmd, static_cast<uint32_t>(verts.size()), 1, 0, 0);
    }

} // namespace impgine


