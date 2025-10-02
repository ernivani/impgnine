#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>

namespace impgine {

struct OutlineResources {
    // Offscreen render target for mask
    VkImage maskImage = VK_NULL_HANDLE;
    VkDeviceMemory maskImageMemory = VK_NULL_HANDLE;
    VkImageView maskImageView = VK_NULL_HANDLE;
    VkSampler maskSampler = VK_NULL_HANDLE;

    // Depth buffer for mask rendering
    VkImage maskDepthImage = VK_NULL_HANDLE;
    VkDeviceMemory maskDepthMemory = VK_NULL_HANDLE;
    VkImageView maskDepthView = VK_NULL_HANDLE;

    // Mask framebuffer and render pass
    VkRenderPass maskRenderPass = VK_NULL_HANDLE;
    VkFramebuffer maskFramebuffer = VK_NULL_HANDLE;

    // Outline pipeline for rendering entity to mask
    VkPipeline outlinePipeline = VK_NULL_HANDLE;
    VkPipelineLayout outlinePipelineLayout = VK_NULL_HANDLE;

    // Composite pipeline for applying outline effect
    VkPipeline compositePipeline = VK_NULL_HANDLE;
    VkPipelineLayout compositePipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout compositeDescriptorLayout = VK_NULL_HANDLE;
    VkDescriptorPool compositeDescriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> compositeDescriptorSets;

    // Outline parameters
    glm::vec4 outlineColor = glm::vec4(1.0f, 0.5f, 0.0f, 2.0f); // Orange, width=2

    bool initialized = false;
};

struct OutlinePushConstants {
    glm::vec4 outlineColor;
    glm::vec2 texelSize;
};

// Initialize outline rendering resources
void initOutlineResources(
    VkDevice device,
    VkPhysicalDevice physicalDevice,
    VkExtent2D extent,
    VkDescriptorSetLayout mainDescriptorSetLayout,
    uint32_t maxFramesInFlight,
    OutlineResources& resources);

// Create composite pipeline (must be called after main render pass is created)
void createOutlineCompositePipeline(
    VkDevice device,
    VkRenderPass renderPass,
    VkExtent2D extent,
    VkSampleCountFlagBits msaaSamples,
    OutlineResources& resources);

// Cleanup outline resources
void cleanupOutlineResources(VkDevice device, OutlineResources& resources);

// Recreate outline resources (on swapchain resize)
void recreateOutlineResources(
    VkDevice device,
    VkPhysicalDevice physicalDevice,
    VkExtent2D newExtent,
    OutlineResources& resources);

// Render selected entity to mask buffer
void renderOutlineMask(
    VkCommandBuffer commandBuffer,
    const OutlineResources& resources,
    VkExtent2D extent,
    void* meshCachePtr,
    void* ecsRegistryPtr,
    uint32_t selectedEntity,
    uint32_t imageIndex);

// Composite outline onto the main render target
void compositeOutline(
    VkCommandBuffer commandBuffer,
    const OutlineResources& resources,
    VkExtent2D extent,
    uint32_t imageIndex);

} // namespace impgine
