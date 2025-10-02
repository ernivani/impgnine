#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <cstdint>

namespace impgine {

// Forward declarations
class SwapChain;
class Window;

// Drawing and rendering functions
void drawFrame(
    VkDevice device,
    VkQueue graphicsQueue,
    VkQueue presentQueue,
    SwapChain& swapChain,
    const std::vector<VkCommandBuffer>& commandBuffers,
    uint32_t& currentFrame,
    bool& framebufferResized,
    void* enginePtr);  // Will be cast to Engine* for recreateSwapChain callback

void recordCommandBuffer(
    VkCommandBuffer commandBuffer,
    uint32_t imageIndex,
    VkRenderPass renderPass,
    const std::vector<VkFramebuffer>& swapChainFramebuffers,
    VkExtent2D swapChainExtent,
    void* pipelinePtr,  // Will be cast to Pipeline*
    VkPipelineLayout pipelineLayout,
    void* meshCachePtr,  // Will be cast to std::unordered_map<std::string, MeshGPUResources>*
    void* ecsRegistryPtr,  // Will be cast to ECSRegistry*
    void* uiRendererPtr,  // Will be cast to UIRenderer*
    void* windowPtr,  // Will be cast to Window*
    void* uiWindowsPtr);  // Will be cast to std::vector<UIWindow>*

void createCommandBuffers(
    VkDevice device,
    VkCommandPool commandPool,
    uint32_t maxFramesInFlight,
    std::vector<VkCommandBuffer>& commandBuffers);

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
    void* meshCachePtr,  // Will be cast to std::unordered_map<std::string, MeshGPUResources>*
    VkSampleCountFlagBits msaaSamples,
    void* pipelinePtr,  // Will be cast to Pipeline*&
    VkPipelineLayout pipelineLayout,
    const std::string& vertShaderPath,
    const std::string& fragShaderPath,
    void* uiRendererPtr,  // Will be cast to UIRenderer*&
    VkCommandPool commandPool,
    VkQueue graphicsQueue,
    const std::string& uiVertShaderPath,
    const std::string& uiFragShaderPath,
    const std::string& fontPath);

void cleanupSwapChain(
    VkDevice device,
    VkImageView colorImageView,
    VkImage colorImage,
    VkDeviceMemory colorImageMemory,
    VkImageView depthImageView,
    VkImage depthImage,
    VkDeviceMemory depthImageMemory,
    const std::vector<VkFramebuffer>& swapChainFramebuffers,
    void* pipelinePtr,  // Will be cast to Pipeline*
    VkRenderPass renderPass,
    SwapChain* swapChain);

// Callback functions
void framebufferResizeCallback(void* windowPtr, int width, int height, void* enginePtr);

} // namespace impgine
