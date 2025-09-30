#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <string>

#include "../backend/pipeline.hpp"
#include "../backend/swap_chain.hpp"
#include "../backend/buffers.hpp"
#include "../backend/window.hpp"
#include "UIWindow.hpp"
#include "UILayout.hpp"

namespace impgine {

    struct UIVertex {
        glm::vec2 pos;    // pixel space
        glm::vec4 color;  // RGBA

        static VkVertexInputBindingDescription getBindingDescription() {
            VkVertexInputBindingDescription binding{};
            binding.binding = 0;
            binding.stride = sizeof(UIVertex);
            binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            return binding;
        }

        static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() {
            std::array<VkVertexInputAttributeDescription, 2> attrs{};
            attrs[0].binding = 0; attrs[0].location = 0; attrs[0].format = VK_FORMAT_R32G32_SFLOAT; attrs[0].offset = offsetof(UIVertex, pos);
            attrs[1].binding = 0; attrs[1].location = 1; attrs[1].format = VK_FORMAT_R32G32B32A32_SFLOAT; attrs[1].offset = offsetof(UIVertex, color);
            return attrs;
        }
    };

    class UIRenderer {
    public:
        UIRenderer(VkDevice device, VkPhysicalDevice physicalDevice, VkRenderPass renderPass, VkPipelineLayout pipelineLayout, SwapChain& swapChain, VkSampleCountFlagBits samples, const std::string& vertSpvPath, const std::string& fragSpvPath);
        ~UIRenderer();

        UIRenderer(const UIRenderer&) = delete;
        UIRenderer& operator=(const UIRenderer&) = delete;

        void record(VkCommandBuffer cmd, const Window& window, std::vector<UIWindow>& windows);

        // Get the layout manager
        UILayout& getLayout() { return layout; }

    private:
        void createVertexBuffer(VkDeviceSize size);

        VkDevice device;
        VkPhysicalDevice physicalDevice;
        std::unique_ptr<Pipeline> pipeline;
        VkBuffer vertexBuffer { VK_NULL_HANDLE };
        VkDeviceMemory vertexMemory { VK_NULL_HANDLE };
        VkPipelineLayout pipelineLayout;
        SwapChain& swapChain;
        VkSampleCountFlagBits samples;
        std::string vertPath;
        std::string fragPath;

        UILayout layout;
    };

} // namespace impgine


