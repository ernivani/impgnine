#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <cstdint>

namespace impgine {

// Forward declarations
struct Vertex;

// Buffer creation and management
void createBuffer(
    VkDevice device,
    VkPhysicalDevice physicalDevice,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkBuffer& buffer,
    VkDeviceMemory& bufferMemory);

void createImage(
    VkDevice device,
    VkPhysicalDevice physicalDevice,
    uint32_t width,
    uint32_t height,
    uint32_t mipLevels,
    VkSampleCountFlagBits numSamples,
    VkFormat format,
    VkImageTiling tiling,
    VkImageUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkImage& image,
    VkDeviceMemory& imageMemory);

VkImageView createImageView(
    VkDevice device,
    VkImage image,
    VkFormat format,
    VkImageAspectFlags aspectFlags,
    uint32_t mipLevels);

// Memory management
uint32_t findMemoryType(
    VkPhysicalDevice physicalDevice,
    uint32_t typeFilter,
    VkMemoryPropertyFlags properties);

// Command buffer helpers
VkCommandBuffer beginSingleTimeCommands(VkDevice device, VkCommandPool commandPool);

void endSingleTimeCommands(
    VkDevice device,
    VkCommandPool commandPool,
    VkQueue graphicsQueue,
    VkCommandBuffer commandBuffer);

void copyBuffer(
    VkDevice device,
    VkCommandPool commandPool,
    VkQueue graphicsQueue,
    VkBuffer srcBuffer,
    VkBuffer dstBuffer,
    VkDeviceSize size);

void transitionImageLayout(
    VkDevice device,
    VkCommandPool commandPool,
    VkQueue graphicsQueue,
    VkImage image,
    VkFormat format,
    VkImageLayout oldLayout,
    VkImageLayout newLayout,
    uint32_t mipLevels);

void copyBufferToImage(
    VkDevice device,
    VkCommandPool commandPool,
    VkQueue graphicsQueue,
    VkBuffer buffer,
    VkImage image,
    uint32_t width,
    uint32_t height);

// Command pool
void createCommandPool(
    VkDevice device,
    VkPhysicalDevice physicalDevice,
    VkSurfaceKHR surface,
    VkCommandPool& commandPool);

// Uniform buffers
void createUniformBuffers(
    VkDevice device,
    VkPhysicalDevice physicalDevice,
    uint32_t imageCount,
    std::vector<VkBuffer>& uniformBuffers,
    std::vector<VkDeviceMemory>& uniformBuffersMemory);

// Descriptor sets
void createDescriptorSetLayout(
    VkDevice device,
    VkDescriptorSetLayout& descriptorSetLayout);

void createDescriptorPool(
    VkDevice device,
    uint32_t meshCount,
    uint32_t imageCount,
    VkDescriptorPool& descriptorPool);

void createDescriptorSets(
    VkDevice device,
    VkDescriptorPool descriptorPool,
    VkDescriptorSetLayout descriptorSetLayout,
    const std::vector<VkBuffer>& uniformBuffers,
    uint32_t imageCount,
    void* meshCachePtr);  // Will be cast to std::unordered_map<std::string, MeshGPUResources>*

void createDescriptorSetsForSprites(
    VkDevice device,
    VkDescriptorPool descriptorPool,
    VkDescriptorSetLayout descriptorSetLayout,
    const std::vector<VkBuffer>& uniformBuffers,
    uint32_t imageCount,
    void* spriteCachePtr);  // Will be cast to std::unordered_map<std::string, SpriteGPUResources>*

// MSAA and depth resources
void createColorResources(
    VkDevice device,
    VkPhysicalDevice physicalDevice,
    VkFormat swapChainImageFormat,
    VkExtent2D swapChainExtent,
    VkSampleCountFlagBits msaaSamples,
    VkImage& colorImage,
    VkDeviceMemory& colorImageMemory,
    VkImageView& colorImageView);

void createDepthResources(
    VkDevice device,
    VkPhysicalDevice physicalDevice,
    VkExtent2D swapChainExtent,
    VkSampleCountFlagBits msaaSamples,
    VkImage& depthImage,
    VkDeviceMemory& depthImageMemory,
    VkImageView& depthImageView);

// Render pass and framebuffers
void createRenderPass(
    VkDevice device,
    VkPhysicalDevice physicalDevice,
    VkFormat swapChainImageFormat,
    VkSampleCountFlagBits msaaSamples,
    VkRenderPass& renderPass);

void createFramebuffers(
    VkDevice device,
    VkRenderPass renderPass,
    VkExtent2D swapChainExtent,
    const std::vector<VkImageView>& swapChainImageViews,
    VkImageView colorImageView,
    VkImageView depthImageView,
    std::vector<VkFramebuffer>& swapChainFramebuffers);

// Format helpers
VkFormat findSupportedFormat(
    VkPhysicalDevice physicalDevice,
    const std::vector<VkFormat>& candidates,
    VkImageTiling tiling,
    VkFormatFeatureFlags features);

VkFormat findDepthFormat(VkPhysicalDevice physicalDevice);

bool hasStencilComponent(VkFormat format);

VkSampleCountFlagBits getMaxUsableSampleCount(VkPhysicalDevice physicalDevice);

// Mipmap generation
void generateMipmaps(
    VkDevice device,
    VkPhysicalDevice physicalDevice,
    VkCommandPool commandPool,
    VkQueue graphicsQueue,
    VkImage image,
    VkFormat imageFormat,
    int32_t texWidth,
    int32_t texHeight,
    uint32_t mipLevels);

// Uniform buffer update
void updateUniformBuffer(
    VkDevice device,
    VkDeviceMemory uniformBufferMemory,
    const void* uboData,
    size_t uboSize);

} // namespace impgine
