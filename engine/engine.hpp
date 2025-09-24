#pragma once

#include <vulkan/vulkan.h>

#include <cstring>
#include <iostream>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <vector>

#include "backend/buffers.hpp"
#include "backend/pipeline.hpp"
#include "backend/swap_chain.hpp"
#include "backend/window.hpp"
#include "camera.hpp"

namespace impgine {

    struct QueueFamilyIndices {
        std::optional < uint32_t > graphicsFamily;
        std::optional < uint32_t > presentFamily;

        bool isComplete() {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };

    class Engine {
        public: static constexpr int WIDTH = 800;
        static constexpr int HEIGHT = 600;

        Engine();
        ~Engine();

        // Delete copy constructor and assignment operator
        Engine(const Engine & ) = delete;
        Engine & operator = (const Engine & ) = delete;

        void run();

        private: void initVulkan();
        void mainLoop();
        void cleanup();

        // Vulkan initialization functions
        void createInstance();
        bool checkValidationLayerSupport();
        void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT & createInfo);
        void setupDebugMessenger();
        void createSurface();
        void pickPhysicalDevice();
        void createLogicalDevice();
        void createCommandPool();
        void createCommandBuffers();

        // Helper functions
        bool isDeviceSuitable(VkPhysicalDevice device);
        bool checkDeviceExtensionSupport(VkPhysicalDevice device);
        QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
        std::vector <
        const char * > getRequiredExtensions();

        // Drawing
        void drawFrame();
        void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
        void recreateSwapChain();

        // Callback functions
        static void framebufferResizeCallback(GLFWwindow * window, int width, int height);
        static VKAPI_ATTR VkBool32 VKAPI_CALL
        debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
            VkDebugUtilsMessageTypeFlagsEXT messageType,
            const VkDebugUtilsMessengerCallbackDataEXT * pCallbackData, void * pUserData);

        // Member variables
        std::unique_ptr < Window > window;
        std::unique_ptr < Pipeline > pipeline;
        std::unique_ptr < SwapChain > swapChain;
        Camera camera;

        // Validation layers
        const std::vector <
            const char * > validationLayers = {
                "VK_LAYER_KHRONOS_validation"
            };

        const std::vector <
            const char * > deviceExtensions = {
                VK_KHR_SWAPCHAIN_EXTENSION_NAME
            };

        #ifdef NDEBUG
        const bool enableValidationLayers = false;
        #else
        const bool enableValidationLayers = true;
        #endif

        // Vulkan objects
        VkInstance instance;
        VkDebugUtilsMessengerEXT debugMessenger;
        VkSurfaceKHR surface;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkDevice device;
        VkQueue graphicsQueue;
        VkQueue presentQueue;
        VkCommandPool commandPool;
        std::vector < VkCommandBuffer > commandBuffers;
        VkPipelineLayout pipelineLayout;

        uint32_t currentFrame = 0;
        bool framebufferResized = false;
        bool needsPortabilitySubset = false;
    };

    // Helper functions (in global namespace within impgine)
    VkResult CreateDebugUtilsMessengerEXT(VkInstance instance,
        const VkDebugUtilsMessengerCreateInfoEXT * pCreateInfo,
            const VkAllocationCallbacks * pAllocator,
                VkDebugUtilsMessengerEXT * pDebugMessenger);
    void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger,
        const VkAllocationCallbacks * pAllocator);

} // namespace impgine