#include "engine.hpp"

#include <array>
#include <cstdlib>
#include <fstream>
#include <sstream>

// Include all the subsystem headers
#include "shaders/shader_compiler.hpp"
#include "input/input_manager.hpp"
#include "resources/resource_manager.hpp"
#include "scene/scene_loader.hpp"
#include "rendering/vulkan_init.hpp"
#include "rendering/vulkan_resources.hpp"
#include "rendering/vulkan_renderer.hpp"
#include "ui/UIComponents.hpp"

namespace impgine {

// External function to set ResourceManager globals
extern void setResourceManagerGlobals(VkCommandPool commandPool, VkQueue graphicsQueue);

Engine::Engine() {
    // Load project configuration
    project = Project::loadProject("example/project.imp");

    // Compile shaders before initializing Vulkan
    ShaderCompiler::compileAllShaders(project);

    window = std::make_unique<Window>(project.windowWidth, project.windowHeight, project.windowTitle);
    window->setUserPointer(this);
    window->setFramebufferSizeCallback(framebufferResizeCallback);
    glfwSetMouseButtonCallback(window->getGLFWWindow(), mouseButtonCallback);
    glfwSetCharCallback(window->getGLFWWindow(), characterCallback);
    glfwSetKeyCallback(window->getGLFWWindow(), keyCallback);

    initVulkan();
}

Engine::~Engine() {
    cleanup();
}

void Engine::run() {
    std::cout << "Welcome to Impgine!\n";
    loadScene(project.getFullPath(project.lastScene));
    std::cout << "Project: " << project.projectName << " loaded" << std::endl;
    std::cout << "ECSRegistry currently has " << ECSRegistry::getRegistry().getEntities().size() << " entities" << std::endl;

    for (const auto& entity : ECSRegistry::getRegistry().getEntities()) {
        std::cout << "Entity: " << entity <<  ", have components: " << std::endl;

        for (const auto& component : ECSRegistry::getRegistry().getComponents(entity)) {
            std::cout << "Component: " << component.first.name() << std::endl;
        }
    }

    mainLoop();
}

void Engine::initVulkan() {
    // Use Vulkan init functions
    createInstance(instance, validationLayers, enableValidationLayers);
    setupDebugMessenger(instance, debugMessenger, enableValidationLayers);
    createSurface(instance, surface, window.get());
    pickPhysicalDevice(instance, physicalDevice, surface, deviceExtensions, msaaSamples, needsPortabilitySubset);
    createLogicalDevice(physicalDevice, device, graphicsQueue, presentQueue, surface,
                       deviceExtensions, validationLayers, enableValidationLayers, needsPortabilitySubset);

    createECSRegistry();

    // Start with cursor visible for UI interaction
    window->setCursorInputMode(GLFW_CURSOR_NORMAL);

    swapChain = std::make_unique<SwapChain>(device, physicalDevice, surface, *window);

    // Create command pool using vulkan_resources
    createCommandPool(device, physicalDevice, surface, commandPool);

    // Create subsystems
    inputManager = new InputManager(window.get(), &camera);
    resourceManager = new ResourceManager(device, physicalDevice);

    // Configure fallback texture path
    resourceManager->setFallbackTexturePath(project.getEnginePath() + "/Textures/texture.jpg");

    // Set global resource manager context
    setResourceManagerGlobals(commandPool, graphicsQueue);

    // Defer content loading to loadScene
    createColorResources(device, physicalDevice, swapChain->getSwapChainImageFormat(),
                        swapChain->getSwapChainExtent(), msaaSamples,
                        colorImage, colorImageMemory, colorImageView);
    createDepthResources(device, physicalDevice, swapChain->getSwapChainExtent(),
                        msaaSamples, depthImage, depthImageMemory, depthImageView);
    createRenderPass(device, physicalDevice, swapChain->getSwapChainImageFormat(),
                    msaaSamples, renderPass);
    std::vector<VkImageView> imageViews;
    for (size_t i = 0; i < swapChain->imageCount(); i++) {
        imageViews.push_back(swapChain->getImageView(i));
    }
    createFramebuffers(device, renderPass, swapChain->getSwapChainExtent(),
                      imageViews, colorImageView, depthImageView,
                      swapChainFramebuffers);

    // Pipeline will be created after scene resources are ready
}

void Engine::createECSRegistry() {
    auto& reg = ECSRegistry::getRegistry();
    (void)reg;
}

void Engine::mainLoop() {
    auto lastTime = std::chrono::high_resolution_clock::now();
    
    while (!window->shouldClose()) {
        auto currentTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;
        
        window->pollEvents();
        inputManager->handleUIInput(uiRenderer.get(), uiWindows);
        inputManager->processInput(deltaTime);
        inputManager->handleMouseMovement();
        drawFrame();
    }
    vkDeviceWaitIdle(device);
}

void Engine::cleanupSwapChain() {
    impgine::cleanupSwapChain(device, colorImageView, colorImage, colorImageMemory,
                              depthImageView, depthImage, depthImageMemory,
                              swapChainFramebuffers, pipeline.get(), renderPass, swapChain.get());

    // Reset unique_ptrs after cleanup (they will call destructors)
    pipeline.reset();
    swapChain.reset();
}

void Engine::initializeUnityLayout() {
    uiWindows.clear();

    // Get actual framebuffer size, not project config size
    int ww = 0, wh = 0;
    window->getFramebufferSize(&ww, &wh);

    // Scene view (central viewport) - this is where Vulkan renders
    // Make it invisible since we render the 3D scene directly in this area
    UIWindow sceneView;
    sceneView.title = "Scene";
    sceneView.type = UIWindowType::SceneView;
    sceneView.dockPosition = DockPosition::Center;
    sceneView.isVisible = false;  // Don't draw UI for center viewport
    uiWindows.push_back(sceneView);

    // Hierarchy (left panel) - scene tree
    UIWindow hierarchy;
    hierarchy.title = "Hierarchy";
    hierarchy.type = UIWindowType::Hierarchy;
    hierarchy.dockPosition = DockPosition::Left;
    hierarchy.isVisible = true;
    uiWindows.push_back(hierarchy);

    // Inspector (right panel) - object properties
    UIWindow inspector;
    inspector.title = "Inspector";
    inspector.type = UIWindowType::Inspector;
    inspector.dockPosition = DockPosition::Right;
    inspector.isVisible = true;
    uiWindows.push_back(inspector);

    // Assets (bottom panel) - asset browser
    UIWindow assets;
    assets.title = "Assets";
    assets.type = UIWindowType::Assets;
    assets.dockPosition = DockPosition::Bottom;
    assets.isVisible = true;
    uiWindows.push_back(assets);

    // Compute layout immediately after creating windows
    if (uiRenderer) {
        uiRenderer->getLayout().setWindowSize(ww, wh);
        uiRenderer->getLayout().computeLayout(uiWindows);
    }

    // Add example UI components to Inspector window (for demonstration)
    {
        // Find the inspector window (index 2 based on creation order)
        if (uiWindows.size() > 2) {
            auto& inspectorWindow = uiWindows[2];

            // Add a button
            auto button = std::make_shared<UIButton>();
            button->label = "Test Button";
            button->position = glm::vec2(10.0f, 200.0f);
            button->size = glm::vec2(150.0f, 30.0f);
            button->onClick = []() {
                std::cout << "Button clicked!" << std::endl;
            };
            inspectorWindow.addComponent(button);

            // Add an input field
            auto input = std::make_shared<UIInputField>();
            input->placeholder = "Enter name...";
            input->position = glm::vec2(10.0f, 240.0f);
            input->size = glm::vec2(150.0f, 25.0f);
            inspectorWindow.addComponent(input);

            // Add a checkbox
            auto checkbox = std::make_shared<UICheckbox>();
            checkbox->label = "Enable Feature";
            checkbox->position = glm::vec2(10.0f, 280.0f);
            checkbox->onToggle = [](bool checked) {
                std::cout << "Checkbox " << (checked ? "checked" : "unchecked") << std::endl;
            };
            inspectorWindow.addComponent(checkbox);

            // Add a slider
            auto slider = std::make_shared<UISlider>();
            slider->position = glm::vec2(10.0f, 320.0f);
            slider->size = glm::vec2(180.0f, 20.0f);
            slider->value = 0.75f;
            slider->onValueChanged = [](float value) {
                std::cout << "Slider value: " << value << std::endl;
            };
            inspectorWindow.addComponent(slider);

            // Add a label
            auto label = std::make_shared<UILabel>();
            label->text = "UI Components Demo";
            label->position = glm::vec2(10.0f, 170.0f);
            label->textColor = glm::vec4(0.3f, 0.7f, 1.0f, 1.0f);
            inspectorWindow.addComponent(label);
        }
    }

    // Print UI layout information
    std::cout << "\n=== UI Layout Initialized ===" << std::endl;
    std::cout << "Actual framebuffer size: " << ww << "x" << wh << std::endl;
    if (uiRenderer) {
        glm::vec4 vp = uiRenderer->getLayout().getViewportRect();
        std::cout << "Viewport: pos=(" << vp.x << "," << vp.y << ") size=(" << vp.z << "," << vp.w << ")" << std::endl;
    }
    for (size_t i = 0; i < uiWindows.size(); i++) {
        const auto& w = uiWindows[i];
        if (w.isVisible) {
            std::cout << "Window " << i << " (" << w.title << "): "
                      << "pos=(" << w.position.x << "," << w.position.y << ") "
                      << "size=(" << w.size.x << "," << w.size.y << ")" << std::endl;
        }
    }
    std::cout << "============================\n" << std::endl;
    }

void Engine::cleanup() {
    // Destroy UI resources before destroying device
    if (uiRenderer) {
        uiRenderer.reset();
    }
    cleanupSwapChain();

    // Cleanup all mesh resources via ResourceManager
    if (resourceManager) {
        for (auto& [path, meshResources] : resourceManager->getMeshCache()) {
            vkDestroySampler(device, meshResources.textureSampler, nullptr);
            vkDestroyImageView(device, meshResources.textureImageView, nullptr);
            vkDestroyImage(device, meshResources.textureImage, nullptr);
            vkFreeMemory(device, meshResources.textureImageMemory, nullptr);
            vkDestroyBuffer(device, meshResources.indexBuffer, nullptr);
            vkFreeMemory(device, meshResources.indexBufferMemory, nullptr);
            vkDestroyBuffer(device, meshResources.vertexBuffer, nullptr);
            vkFreeMemory(device, meshResources.vertexBufferMemory, nullptr);
        }
    }

    vkDestroyDescriptorPool(device, descriptorPool, nullptr);

    for (size_t i = 0; i < uniformBuffers.size(); i++) {
        vkDestroyBuffer(device, uniformBuffers[i], nullptr);
        vkFreeMemory(device, uniformBuffersMemory[i], nullptr);
    }

    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

        vkDestroyCommandPool(device, commandPool, nullptr);

        vkDestroyDevice(device, nullptr);

    if (enableValidationLayers) {
        DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
    }

        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);

    // Clean up subsystems
    delete inputManager;
    delete resourceManager;
}

void Engine::loadScene(const std::string& scenePath) {
    // Use SceneLoader to load scene data into ECS
    SceneLoader::loadScene(scenePath, project);

    // Load GPU resources for all entities with MeshRenderer3D components
    auto& reg = ECSRegistry::getRegistry();
    for (const auto& entity : reg.getEntities()) {
        const auto& components = reg.getComponents(entity);
        if (components.find(std::type_index(typeid(MeshRenderer3D))) != components.end()) {
            auto& meshRenderer = reg.getComponent<MeshRenderer3D>(entity);
            std::string modelPath = meshRenderer.mesh->modelPath;
            std::string texturePath = meshRenderer.texture ? meshRenderer.texture->texturePath : "";

            // Load mesh resources using ResourceManager
            resourceManager->loadMeshResources(modelPath, texturePath);
        }
    }

    // Create uniform buffers and descriptor sets
    uint32_t imageCount = static_cast<uint32_t>(swapChain->imageCount());
    createUniformBuffers(device, physicalDevice, imageCount,
                        uniformBuffers, uniformBuffersMemory);
    createDescriptorSetLayout(device, descriptorSetLayout);

    uint32_t meshCount = static_cast<uint32_t>(resourceManager->getMeshCache().size());
    createDescriptorPool(device, meshCount, imageCount, descriptorPool);
    createDescriptorSets(device, descriptorPool, descriptorSetLayout, uniformBuffers,
                        imageCount, &resourceManager->getMeshCache());

    // Create pipeline layout now that descriptor set layout exists
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(PushConstantData);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) !=
        VK_SUCCESS) {
        throw std::runtime_error("failed to create pipeline layout!");
    }

    // Create graphics pipeline
    PipelineConfigInfo pipelineConfig{};
    Pipeline::defaultPipelineConfigInfo(pipelineConfig);
    pipelineConfig.renderPass = renderPass;
    pipelineConfig.pipelineLayout = pipelineLayout;
    pipelineConfig.multisampleInfo.rasterizationSamples = msaaSamples;

    std::string vertShader = project.getShadersPath() + "/vert.spv";
    std::string fragShader = project.getShadersPath() + "/frag.spv";
    pipeline = std::make_unique<Pipeline>(device, vertShader, fragShader, pipelineConfig);

    createCommandBuffers(device, commandPool, SwapChain::MAX_FRAMES_IN_FLIGHT, commandBuffers);

    // Initialize UI renderer and a default window
    {
        std::string uiVert = project.getShadersPath() + "/ui.vert.spv";
        std::string uiFrag = project.getShadersPath() + "/ui.frag.spv";
        uiRenderer = std::make_unique<UIRenderer>(device, physicalDevice, renderPass, pipelineLayout, commandPool, graphicsQueue, *swapChain, msaaSamples, uiVert, uiFrag);

        // Load Arial font and create descriptor sets
        std::string fontPath = project.getEnginePath() + "/TTF/arial.ttf";
        uiRenderer->getTextRenderer()->loadFont(fontPath, 48);

        // Create descriptor sets after font texture is loaded
        uiRenderer->createDescriptorSets();

        // Initialize Unity-like layout
        initializeUnityLayout();
    }
}

void Engine::drawFrame() {
    // Wait for the previous frame
    VkFence inFlightFence = swapChain->getInFlightFence(currentFrame);
    vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkSemaphore imageAvailableSemaphore = swapChain->getImageAvailableSemaphore(currentFrame);
    auto result = swapChain->acquireNextImage(&imageIndex, currentFrame, imageAvailableSemaphore);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapChain();
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    vkResetFences(device, 1, &inFlightFence);

    // Update camera projection and view matrices
    camera.setPerspectiveProjection(
        glm::radians(45.0f),
        swapChain->getSwapChainExtent().width / (float)swapChain->getSwapChainExtent().height,
        0.1f,
        100.0f
    );
    camera.updateViewMatrix();

    // Update uniform buffer - use imageIndex for correct buffer
    UniformBufferObject ubo{};
    ubo.view = camera.getView();
    ubo.proj = camera.getProjection();
    impgine::updateUniformBuffer(device, uniformBuffersMemory[imageIndex], &ubo, sizeof(ubo));

    // Record command buffer
    vkResetCommandBuffer(commandBuffers[currentFrame], 0);
    auto& registry = ECSRegistry::getRegistry();
    impgine::recordCommandBuffer(commandBuffers[currentFrame], imageIndex, renderPass,
                                 swapChainFramebuffers, swapChain->getSwapChainExtent(),
                                 pipeline.get(), pipelineLayout,
                                 &resourceManager->getMeshCache(), &registry,
                                 uiRenderer.get(), window.get(), &uiWindows);

    // Submit command buffer
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {imageAvailableSemaphore};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

    VkSemaphore signalSemaphores[] = {swapChain->getRenderFinishedSemaphore(imageIndex)};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFence) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit draw command buffer!");
    }

    // Present frame
    result = swapChain->presentFrame(presentQueue, &imageIndex, currentFrame);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
        framebufferResized = false;
        recreateSwapChain();
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("failed to present swap chain image!");
    }

    currentFrame = (currentFrame + 1) % SwapChain::MAX_FRAMES_IN_FLIGHT;
}

void Engine::recreateSwapChain() {
    std::string vertShader = project.getShadersPath() + "/vert.spv";
    std::string fragShader = project.getShadersPath() + "/frag.spv";
    std::string uiVert = project.getShadersPath() + "/ui.vert.spv";
    std::string uiFrag = project.getShadersPath() + "/ui.frag.spv";
    std::string fontPath = project.getEnginePath() + "/TTF/arial.ttf";

    // Release ownership from unique_ptrs so impgine::recreateSwapChain can delete them
    SwapChain* swapChainPtr = swapChain.release();
    Pipeline* pipelinePtr = pipeline.release();
    UIRenderer* uiRendererPtr = uiRenderer.release();

    impgine::recreateSwapChain(device, physicalDevice, surface, *window, swapChainPtr,
                               colorImage, colorImageMemory, colorImageView,
                               depthImage, depthImageMemory, depthImageView,
                               renderPass, swapChainFramebuffers,
                               uniformBuffers, uniformBuffersMemory,
                               descriptorPool, descriptorSetLayout,
                               &resourceManager->getMeshCache(), msaaSamples,
                               &pipelinePtr, pipelineLayout, vertShader, fragShader,
                               &uiRendererPtr, commandPool, graphicsQueue,
                               uiVert, uiFrag, fontPath);

    // Take ownership of new pointers
    swapChain.reset(swapChainPtr);
    pipeline.reset(pipelinePtr);
    uiRenderer.reset(uiRendererPtr);
}

void Engine::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    (void)width;
    (void)height;
    auto app = reinterpret_cast<Engine*>(glfwGetWindowUserPointer(window));
    app->framebufferResized = true;
}

void Engine::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    auto app = reinterpret_cast<Engine*>(glfwGetWindowUserPointer(window));
    InputManager::mouseButtonCallback(window, button, action, mods,
                                     app->uiRenderer.get(), app->uiWindows);
}

void Engine::characterCallback(GLFWwindow* window, unsigned int codepoint) {
    auto app = reinterpret_cast<Engine*>(glfwGetWindowUserPointer(window));
    InputManager::characterCallback(window, codepoint, app->uiWindows);
}

void Engine::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    auto app = reinterpret_cast<Engine*>(glfwGetWindowUserPointer(window));
    InputManager::keyCallback(window, key, scancode, action, mods, app->uiWindows);
}

}  // namespace impgine
