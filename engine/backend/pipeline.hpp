#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

#include <array>
#include <string>
#include <vector>

namespace impgine {

    struct Vertex {
        glm::vec3 pos;
        glm::vec3 color;
        glm::vec2 texCoord;

        bool operator==(const Vertex& other) const {
            return pos == other.pos && color == other.color && texCoord == other.texCoord;
        }

        static VkVertexInputBindingDescription getBindingDescription() {
            VkVertexInputBindingDescription bindingDescription{};
            bindingDescription.binding = 0;
            bindingDescription.stride = sizeof(Vertex);
            bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            return bindingDescription;
        }

        static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions() {
            std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};
            
            attributeDescriptions[0].binding = 0;
            attributeDescriptions[0].location = 0;
            attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[0].offset = offsetof(Vertex, pos);

            attributeDescriptions[1].binding = 0;
            attributeDescriptions[1].location = 1;
            attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[1].offset = offsetof(Vertex, color);

            attributeDescriptions[2].binding = 0;
            attributeDescriptions[2].location = 2;
            attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
            attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

            return attributeDescriptions;
        }
    };

    // External declaration of vertex data
    extern const std::vector<Vertex> vertices;
    extern const std::vector<uint16_t> indices;

    struct PipelineConfigInfo {
        PipelineConfigInfo() =
            default;
        PipelineConfigInfo(const PipelineConfigInfo & ) = delete;
        PipelineConfigInfo & operator = (const PipelineConfigInfo & ) = delete;

        std::vector < VkVertexInputBindingDescription > bindingDescriptions {};
        std::vector < VkVertexInputAttributeDescription > attributeDescriptions {};
        VkPipelineViewportStateCreateInfo viewportInfo;
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo;
        VkPipelineRasterizationStateCreateInfo rasterizationInfo;
        VkPipelineMultisampleStateCreateInfo multisampleInfo;
        VkPipelineColorBlendAttachmentState colorBlendAttachment;
        VkPipelineColorBlendStateCreateInfo colorBlendInfo;
        VkPipelineDepthStencilStateCreateInfo depthStencilInfo;
        std::vector < VkDynamicState > dynamicStateEnables;
        VkPipelineDynamicStateCreateInfo dynamicStateInfo;
        VkPipelineLayout pipelineLayout = nullptr;
        VkRenderPass renderPass = nullptr;
        uint32_t subpass = 0;
    };

    class Pipeline {
        public: Pipeline(VkDevice device,
            const std::string & vertFilepath,
                const std::string & fragFilepath,
                    const PipelineConfigInfo & configInfo);
        ~Pipeline();

        Pipeline(const Pipeline & ) = delete;
        Pipeline & operator = (const Pipeline & ) = delete;

        void bind(VkCommandBuffer commandBuffer);

        static void defaultPipelineConfigInfo(PipelineConfigInfo & configInfo);
        static void enableAlphaBlending(PipelineConfigInfo & configInfo);

        private: static std::vector < char > readFile(const std::string & filepath);

        void createGraphicsPipeline(const std::string & vertFilepath,
            const std::string & fragFilepath,
                const PipelineConfigInfo & configInfo);

        void createShaderModule(const std::vector < char > & code, VkShaderModule * shaderModule);

        VkDevice device;
        VkPipeline graphicsPipeline;
        VkShaderModule vertShaderModule;
        VkShaderModule fragShaderModule;
    };

} // namespace impgine

namespace std {
    template<> struct hash<glm::vec2> {
        size_t operator()(glm::vec2 const& vertex) const {
            return ((hash<float>()(vertex.x) ^
                   (hash<float>()(vertex.y) << 1)) >> 1);
        }
    };

    template<> struct hash<glm::vec3> {
        size_t operator()(glm::vec3 const& vertex) const {
            return ((hash<float>()(vertex.x) ^
                   (hash<float>()(vertex.y) << 1)) >> 1) ^
                   (hash<float>()(vertex.z) << 1);
        }
    };

    template<> struct hash<impgine::Vertex> {
        size_t operator()(impgine::Vertex const& vertex) const {
            return ((hash<glm::vec3>()(vertex.pos) ^
                   (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
                   (hash<glm::vec2>()(vertex.texCoord) << 1);
        }
    };
}