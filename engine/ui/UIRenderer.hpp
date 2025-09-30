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
#include "TextRenderer.hpp"

namespace impgine {

    struct UIVertex {
        glm::vec2 pos;    // NDC space
        glm::vec4 color;  // RGBA
        glm::vec2 uv;     // Texture coordinates

        static VkVertexInputBindingDescription getBindingDescription() {
            VkVertexInputBindingDescription binding{};
            binding.binding = 0;
            binding.stride = sizeof(UIVertex);
            binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            return binding;
        }

        static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions() {
            std::array<VkVertexInputAttributeDescription, 3> attrs{};
            attrs[0].binding = 0; attrs[0].location = 0; attrs[0].format = VK_FORMAT_R32G32_SFLOAT; attrs[0].offset = offsetof(UIVertex, pos);
            attrs[1].binding = 0; attrs[1].location = 1; attrs[1].format = VK_FORMAT_R32G32B32A32_SFLOAT; attrs[1].offset = offsetof(UIVertex, color);
            attrs[2].binding = 0; attrs[2].location = 2; attrs[2].format = VK_FORMAT_R32G32_SFLOAT; attrs[2].offset = offsetof(UIVertex, uv);
            return attrs;
        }
    };

    class UIRenderer {
    public:
        UIRenderer(VkDevice device, VkPhysicalDevice physicalDevice, VkRenderPass renderPass, VkPipelineLayout pipelineLayout, VkCommandPool commandPool, VkQueue graphicsQueue, SwapChain& swapChain, VkSampleCountFlagBits samples, const std::string& vertSpvPath, const std::string& fragSpvPath);
        ~UIRenderer();

        UIRenderer(const UIRenderer&) = delete;
        UIRenderer& operator=(const UIRenderer&) = delete;

        void record(VkCommandBuffer cmd, const Window& window, std::vector<UIWindow>& windows, uint32_t imageIndex);

        // Get the layout manager
        UILayout& getLayout() { return layout; }

        // Text rendering
        TextRenderer* getTextRenderer() { return textRenderer.get(); }
        void renderText(const std::string& text, glm::vec2 position, float scale, glm::vec4 color, std::vector<UIVertex>& verts);
        void createDescriptorSets();

    private:
        void createVertexBuffer(VkDeviceSize size);
        void createDescriptorSetLayout();
        void createDescriptorPool();

        VkDevice device;
        VkPhysicalDevice physicalDevice;
        std::unique_ptr<Pipeline> pipeline;
        VkBuffer vertexBuffer { VK_NULL_HANDLE };
        VkDeviceMemory vertexMemory { VK_NULL_HANDLE };
        VkPipelineLayout pipelineLayout;
        VkPipelineLayout uiPipelineLayout { VK_NULL_HANDLE };
        VkDescriptorSetLayout descriptorSetLayout { VK_NULL_HANDLE };
        VkDescriptorPool descriptorPool { VK_NULL_HANDLE };
        std::vector<VkDescriptorSet> descriptorSets;
        SwapChain& swapChain;
        VkSampleCountFlagBits samples;
        std::string vertPath;
        std::string fragPath;

        UILayout layout;
        std::unique_ptr<TextRenderer> textRenderer;
    };

} // namespace impgine


