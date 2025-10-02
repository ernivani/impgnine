#pragma once

#include <vulkan/vulkan.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

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
#include "ECS/ECSRegistry.hpp"
#include "ECS/components.hpp"
#include "project.hpp"
#include "ui/UIRenderer.hpp"
#include "ui/UIWindow.hpp"

namespace impgine {

    // Forward declarations
    class InputManager;
    class ResourceManager;

    struct UniformBufferObject {
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 proj;
    };

    struct PushConstantData {
        alignas(16) glm::mat4 model;
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
        void loadScene(const std::string& scenePath);
        void mainLoop();
        void cleanup();
        void cleanupSwapChain();

        // ECS
        void createECSRegistry();

        // Callback functions
        static void framebufferResizeCallback(GLFWwindow * window, int width, int height);
        static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
        static void characterCallback(GLFWwindow* window, unsigned int codepoint);
        static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
        static void windowCloseCallback(GLFWwindow* window);

        // Drawing
        void drawFrame();
        void recreateSwapChain();

        // Helper to initialize Unity-like UI layout
        void initializeUnityLayout();
        void rebuildInspectorUI();

        // Accessors for input and save handlers
        public: const ProjectSettings& getProject() const { return project; }
        public: ProjectSettings& getProject() { return project; }
        public: Camera& getCamera() { return camera; }

        // Scene state tracking
        public: void markSceneModified() { sceneModified = true; }
        public: bool isSceneModified() const { return sceneModified; }
        public: void saveCurrentScene();
        private: void showUnsavedChangesDialog();
        private: void showLoadingModal(bool show, float progress = 0.0f);
        private: void updateLoadingProgress(float progress);
        private: void initializeLoadingScreen();
        private: void renderLoadingFrame(float progress);

        // Member variables
        public: ProjectSettings project;
        std::unique_ptr<Window> window;
        std::unique_ptr<Pipeline> pipeline;
        std::unique_ptr<SwapChain> swapChain;
        public: Camera camera;
        friend class InputManager;

        // Subsystems
        InputManager* inputManager = nullptr;
        ResourceManager* resourceManager = nullptr;

        // Validation layers
        const std::vector<const char*> validationLayers = {
            "VK_LAYER_KHRONOS_validation"
        };

        const std::vector<const char*> deviceExtensions = {
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
        std::vector<VkBuffer> uniformBuffers;
        std::vector<VkDeviceMemory> uniformBuffersMemory;
        VkDescriptorSetLayout descriptorSetLayout;
        VkDescriptorPool descriptorPool;
        std::vector<VkDescriptorSet> descriptorSets;
        VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;
        VkImage colorImage;
        VkDeviceMemory colorImageMemory;
        VkImageView colorImageView;
        VkImage depthImage;
        VkDeviceMemory depthImageMemory;
        VkImageView depthImageView;
        VkRenderPass renderPass;
        std::vector<VkFramebuffer> swapChainFramebuffers;
        std::vector<VkCommandBuffer> commandBuffers;
        VkPipelineLayout pipelineLayout;

        uint32_t currentFrame = 0;
        bool framebufferResized = false;
        bool needsPortabilitySubset = false;

        // UI
        std::unique_ptr<UIRenderer> uiRenderer;
        std::vector<UIWindow> uiWindows;
        glm::vec2 lastInspectorSize { 0.0f, 0.0f };

        // Scene state
        bool sceneModified = false;
        std::string currentScenePath;
        bool isLoadingScene = false;
    };

} // namespace impgine