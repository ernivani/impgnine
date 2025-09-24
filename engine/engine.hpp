#pragma once

#include <vulkan/vulkan.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

#include "../external/stb_image.h"

#include <chrono>
#include <cstring>
#include <iostream>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#include "backend/buffers.hpp"
#include "backend/pipeline.hpp"
#include "backend/swap_chain.hpp"
#include "backend/window.hpp"
#include "camera.hpp"

namespace impgine {

    struct UniformBufferObject {
        alignas(16) glm::mat4 model;
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 proj;
    };

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
        
        static const std::string MODEL_PATH;
        static const std::string TEXTURE_PATH;

        Engine();
        ~Engine();

        // Delete copy constructor and assignment operator
        Engine(const Engine & ) = delete;
        Engine & operator = (const Engine & ) = delete;

        void run();

        private: void initVulkan();
        void mainLoop();
        void cleanup();
        void cleanupSwapChain();

        // Vulkan initialization functions
        void createInstance();
        bool checkValidationLayerSupport();
        void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT & createInfo);
        void setupDebugMessenger();
        void createSurface();
        void pickPhysicalDevice();
        void createLogicalDevice();
        void createCommandPool();
        void createVertexBuffer();
        void createIndexBuffer();
        void createUniformBuffers();
        void createDescriptorSetLayout();
        void createDescriptorPool();
        void createDescriptorSets();
        void createTextureImage();
        void createTextureImageView();
        void createTextureSampler();
        void createColorResources();
        void createDepthResources();
        void createRenderPass();
        void createFramebuffers();
        void createCommandBuffers();
        void loadModel();
        void generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);
        void updateUniformBuffer(uint32_t currentImage);

        // Helper functions
        bool isDeviceSuitable(VkPhysicalDevice device);
        bool checkDeviceExtensionSupport(VkPhysicalDevice device);
        QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
        uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
        void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
        void createImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
        VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);
        VkCommandBuffer beginSingleTimeCommands();
        void endSingleTimeCommands(VkCommandBuffer commandBuffer);
        void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
        void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels);
        void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
        VkFormat findDepthFormat();
        VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
        bool hasStencilComponent(VkFormat format);
        VkSampleCountFlagBits getMaxUsableSampleCount();
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
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        VkBuffer vertexBuffer;
        VkDeviceMemory vertexBufferMemory;
        VkBuffer indexBuffer;
        VkDeviceMemory indexBufferMemory;
        std::vector<VkBuffer> uniformBuffers;
        std::vector<VkDeviceMemory> uniformBuffersMemory;
        VkDescriptorSetLayout descriptorSetLayout;
        VkDescriptorPool descriptorPool;
        std::vector<VkDescriptorSet> descriptorSets;
        uint32_t mipLevels;
        VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;
        VkImage colorImage;
        VkDeviceMemory colorImageMemory;
        VkImageView colorImageView;
        VkImage textureImage;
        VkDeviceMemory textureImageMemory;
        VkImageView textureImageView;
        VkSampler textureSampler;
        VkImage depthImage;
        VkDeviceMemory depthImageMemory;
        VkImageView depthImageView;
        VkRenderPass renderPass;
        std::vector<VkFramebuffer> swapChainFramebuffers;
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