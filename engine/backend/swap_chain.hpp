#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <memory>

namespace impgine {

class Window;

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

class SwapChain {
public:
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

    SwapChain(VkDevice device, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, Window& window);
    ~SwapChain();

    // Delete copy constructor and assignment operator
    SwapChain(const SwapChain&) = delete;
    SwapChain& operator=(const SwapChain&) = delete;

    VkFramebuffer getFrameBuffer(int index) const { return swapChainFramebuffers[index]; }
    VkRenderPass getRenderPass() const { return renderPass; }
    VkImageView getImageView(int index) const { return swapChainImageViews[index]; }
    size_t imageCount() const { return swapChainImages.size(); }
    VkFormat getSwapChainImageFormat() const { return swapChainImageFormat; }
    VkExtent2D getSwapChainExtent() const { return swapChainExtent; }
    uint32_t getWidth() const { return swapChainExtent.width; }
    uint32_t getHeight() const { return swapChainExtent.height; }

    float extentAspectRatio() const {
        return static_cast<float>(swapChainExtent.width) / static_cast<float>(swapChainExtent.height);
    }

    VkFormat findDepthFormat();
    VkResult acquireNextImage(uint32_t* imageIndex);
    VkResult presentFrame(VkQueue presentQueue, uint32_t* imageIndex);
    
    VkSemaphore getImageAvailableSemaphore(uint32_t imageIndex) const { return imageAvailableSemaphores[imageIndex]; }
    VkSemaphore getRenderFinishedSemaphore(uint32_t imageIndex) const { return renderFinishedSemaphores[imageIndex]; }
    VkFence getInFlightFence() const { return inFlightFences[currentFrame]; }

    bool compareSwapFormats(const SwapChain& swapChain) const {
        return swapChain.swapChainDepthFormat == swapChainDepthFormat &&
               swapChain.swapChainImageFormat == swapChainImageFormat;
    }

    void recreateSwapChain();
    void cleanupSwapChain();

    static SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);

private:
    void init();
    void createSwapChain();
    void createImageViews();
    void createDepthResources();
    void createRenderPass();
    void createFramebuffers();
    void createSyncObjects();

    // Helper functions
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

    VkDevice device;
    VkPhysicalDevice physicalDevice;
    VkSurfaceKHR surface;
    Window& windowRef;

    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkFormat swapChainDepthFormat;
    VkExtent2D swapChainExtent;

    std::vector<VkImageView> swapChainImageViews;
    std::vector<VkImage> depthImages;
    std::vector<VkDeviceMemory> depthImageMemorys;
    std::vector<VkImageView> depthImageViews;
    std::vector<VkFramebuffer> swapChainFramebuffers;

    VkRenderPass renderPass;

    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    std::vector<VkFence> imagesInFlight;
    size_t currentFrame = 0;
};

} // namespace impgine
