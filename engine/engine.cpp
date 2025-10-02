#include "engine.hpp"

#include <array>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>

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
    glfwSetWindowCloseCallback(window->getGLFWWindow(), windowCloseCallback);

    // Set up camera modification callback
    camera.setOnModifiedCallback([this]() {
        markSceneModified();
    });

    initVulkan();
}

Engine::~Engine() {
    cleanup();
}

void Engine::run() {
    std::cout << "Welcome to Impgine!\n";

    // Initialize basic UI renderer first to show loading screen
    initializeLoadingScreen();

    // Show initial loading screen
    showLoadingModal(true, 0.0f);
    renderLoadingFrame(0.0f);

    // Now load the scene with progress updates
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
    inputManager = new InputManager(window.get(), &camera, this);
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

        // Rebuild hierarchy if needed (after input processing but before rendering)
        if (needsHierarchyRebuild) {
            rebuildHierarchyUI();
            needsHierarchyRebuild = false;
        }

        drawFrame();
    }
    vkDeviceWaitIdle(device);
}

void Engine::cleanupSwapChain() {
    cleanupOutlineResources(device, outlineResources);

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
    // Remove demo elements; inspector is built dynamically

    // Populate Hierarchy window with tree nodes for entities
    rebuildHierarchyUI();
    // Build inspector UI for initial selection (if any)
    rebuildInspectorUI();

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

void Engine::rebuildInspectorUI() {
    if (!uiRenderer) return;

    // Ensure there is an inspector window
    if (uiWindows.size() <= 2) return;
    auto& inspectorWindow = uiWindows[2];
    if (inspectorWindow.type != UIWindowType::Inspector) return;

    // Clear existing components
    inspectorWindow.components.clear();

    const Entity selected = uiRenderer->getSelectedEntity();
    if (selected == INVALID_ENTITY) {
        // Show placeholder text
        auto label = std::make_shared<UILabel>();
        label->text = "No selection";
        label->position = glm::vec2(10.0f, 10.0f);
        label->textColor = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);
        inspectorWindow.addComponent(label);
        return;
    }

    auto& registry = ECSRegistry::getRegistry();

    float y = 10.0f;

    // Header: Active checkbox and Entity name
    {
        // Active checkbox
        bool isActive = true;
        try {
            const auto& active = registry.getComponent<Active>(selected);
            isActive = active.isActiveSelf;
        } catch (...) {}

        auto activeCheckbox = std::make_shared<UICheckbox>();
        activeCheckbox->isChecked = isActive;
        activeCheckbox->label = "";  // No label, just checkbox
        activeCheckbox->position = glm::vec2(10.0f, y + 6.0f);
        activeCheckbox->size = glm::vec2(20.0f, 20.0f);
        activeCheckbox->boxSize = 16.0f;
        activeCheckbox->boxColor = glm::vec4(0.18f, 0.18f, 0.2f, 1.0f);
        activeCheckbox->checkColor = glm::vec4(0.3f, 0.7f, 0.3f, 1.0f);
        activeCheckbox->onToggle = [this, selected](bool checked) {
            auto& reg = ECSRegistry::getRegistry();
            reg.setActive(selected, checked);
            markSceneModified();
        };
        inspectorWindow.addComponent(activeCheckbox);

        // Entity name (editable)
        std::string name = "Entity " + std::to_string(selected);
        try {
            const auto& tag = registry.getComponent<Tag>(selected);
            if (!tag.tag.empty()) name = tag.tag;
        } catch (...) {}

        auto nameField = std::make_shared<UIInputField>();
        nameField->text = name;
        nameField->position = glm::vec2(35.0f, y);
        nameField->size = glm::vec2(inspectorWindow.getContentSize().x - 45.0f, 32.0f);
        nameField->backgroundColor = glm::vec4(0.18f, 0.18f, 0.2f, 1.0f);
        nameField->focusedColor = glm::vec4(0.22f, 0.22f, 0.25f, 1.0f);
        nameField->hoverColor = glm::vec4(0.2f, 0.2f, 0.22f, 1.0f);
        nameField->textColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        nameField->borderWidth = 1.0f;
        nameField->padding = 8.0f;
        nameField->onCommit = [this, selected](const std::string& newName) {
            auto& reg = ECSRegistry::getRegistry();
            try {
                auto& tag = reg.getComponent<Tag>(selected);
                tag.tag = newName;
                markSceneModified();
                needsHierarchyRebuild = true;  // Rebuild hierarchy to show new name
            } catch (...) {
                // Entity doesn't have a tag component, add one
                reg.addComponent<Tag>(selected, Tag{newName});
                markSceneModified();
                needsHierarchyRebuild = true;
            }
        };
        inspectorWindow.addComponent(nameField);
        y += 40.0f;
    }

    // Transform section with better styling
    auto transformTitle = std::make_shared<UILabel>();
    transformTitle->text = "Transform";
    transformTitle->position = glm::vec2(10.0f, y);
    transformTitle->textColor = glm::vec4(0.9f, 0.9f, 0.9f, 1.0f);
    inspectorWindow.addComponent(transformTitle);
    y += 30.0f;

    // Display transform as editable X/Y/Z fields similar to Unity
    try {
        const auto& transform = registry.getComponent<Transform>(selected);

        auto makeLabel = [&](const char* text, float ypos) {
            auto lbl = std::make_shared<UILabel>();
            lbl->text = text;
            lbl->position = glm::vec2(20.0f, ypos + 5.0f);
            lbl->textColor = glm::vec4(0.85f, 0.85f, 0.85f, 1.0f);
            inspectorWindow.addComponent(lbl);
        };

        // Calculate responsive layout for transform fields
        float contentWidth = inspectorWindow.getContentSize().x;
        float labelWidth = 80.0f;
        float axisLabelWidth = 15.0f;
        float fieldSpacing = 5.0f;
        float availableWidth = contentWidth - labelWidth - 40.0f; // 40 for padding
        float fieldWidth = (availableWidth - (2 * axisLabelWidth) - (2 * fieldSpacing)) / 3.0f;
        fieldWidth = glm::max(fieldWidth, 50.0f); // Minimum field width

        auto makeAxisLabel = [&](const char* text, float xpos, float ypos) {
            auto lbl = std::make_shared<UILabel>();
            lbl->text = text;
            lbl->position = glm::vec2(xpos, ypos + 8.0f);
            lbl->textColor = glm::vec4(0.7f, 0.7f, 0.7f, 1.0f);
            inspectorWindow.addComponent(lbl);
        };

        auto makeField = [&](float xpos, float ypos, float value, std::function<void(float)> setter) {
            auto field = std::make_shared<UIInputField>();
            field->position = glm::vec2(xpos, ypos);
            field->size = glm::vec2(fieldWidth, 28.0f);
            field->backgroundColor = glm::vec4(0.18f, 0.18f, 0.2f, 1.0f);
            field->focusedColor = glm::vec4(0.22f, 0.22f, 0.25f, 1.0f);
            field->hoverColor = glm::vec4(0.2f, 0.2f, 0.22f, 1.0f);
            field->borderWidth = 1.0f;
            field->padding = 6.0f;

            // Format number without trailing zeros
            std::string txt = std::to_string(value);
            // Remove trailing zeros after decimal point
            size_t dotPos = txt.find('.');
            if (dotPos != std::string::npos) {
                size_t lastNonZero = txt.find_last_not_of('0');
                if (lastNonZero != std::string::npos && lastNonZero > dotPos) {
                    txt = txt.substr(0, lastNonZero + 1);
                } else if (lastNonZero == dotPos) {
                    txt = txt.substr(0, dotPos); // Remove decimal point if no fractional part
                }
            }
            field->text = txt;
            field->onCommit = [this, setter](const std::string& s){
                try {
                    setter(std::stof(s));
                    markSceneModified();
                } catch (...) {}
            };
            inspectorWindow.addComponent(field);
        };

        // Row: Position
        {
            float rowY = y; makeLabel("Position", rowY); y += 38.0f;
            float xStart = labelWidth;
            makeAxisLabel("X", xStart, rowY);
            makeField(xStart + axisLabelWidth, rowY, transform.position.x, [selected](float v){ auto& r = ECSRegistry::getRegistry(); auto& t = r.getComponent<Transform>(selected); t.position.x = v; });
            float xOffset2 = xStart + axisLabelWidth + fieldWidth + fieldSpacing;
            makeAxisLabel("Y", xOffset2, rowY);
            makeField(xOffset2 + axisLabelWidth, rowY, transform.position.y, [selected](float v){ auto& r = ECSRegistry::getRegistry(); auto& t = r.getComponent<Transform>(selected); t.position.y = v; });
            float xOffset3 = xOffset2 + axisLabelWidth + fieldWidth + fieldSpacing;
            makeAxisLabel("Z", xOffset3, rowY);
            makeField(xOffset3 + axisLabelWidth, rowY, transform.position.z, [selected](float v){ auto& r = ECSRegistry::getRegistry(); auto& t = r.getComponent<Transform>(selected); t.position.z = v; });
        }

        // Row: Rotation
        {
            float rowY = y; makeLabel("Rotation", rowY); y += 38.0f;
            float xStart = labelWidth;
            makeAxisLabel("X", xStart, rowY);
            makeField(xStart + axisLabelWidth, rowY, transform.rotation.x, [selected](float v){ auto& r = ECSRegistry::getRegistry(); auto& t = r.getComponent<Transform>(selected); t.rotation.x = v; });
            float xOffset2 = xStart + axisLabelWidth + fieldWidth + fieldSpacing;
            makeAxisLabel("Y", xOffset2, rowY);
            makeField(xOffset2 + axisLabelWidth, rowY, transform.rotation.y, [selected](float v){ auto& r = ECSRegistry::getRegistry(); auto& t = r.getComponent<Transform>(selected); t.rotation.y = v; });
            float xOffset3 = xOffset2 + axisLabelWidth + fieldWidth + fieldSpacing;
            makeAxisLabel("Z", xOffset3, rowY);
            makeField(xOffset3 + axisLabelWidth, rowY, transform.rotation.z, [selected](float v){ auto& r = ECSRegistry::getRegistry(); auto& t = r.getComponent<Transform>(selected); t.rotation.z = v; });
        }

        // Row: Scale
        {
            float rowY = y; makeLabel("Scale", rowY); y += 38.0f;
            float xStart = labelWidth;
            makeAxisLabel("X", xStart, rowY);
            makeField(xStart + axisLabelWidth, rowY, transform.scale.x, [selected](float v){ auto& r = ECSRegistry::getRegistry(); auto& t = r.getComponent<Transform>(selected); t.scale.x = v; });
            float xOffset2 = xStart + axisLabelWidth + fieldWidth + fieldSpacing;
            makeAxisLabel("Y", xOffset2, rowY);
            makeField(xOffset2 + axisLabelWidth, rowY, transform.scale.y, [selected](float v){ auto& r = ECSRegistry::getRegistry(); auto& t = r.getComponent<Transform>(selected); t.scale.y = v; });
            float xOffset3 = xOffset2 + axisLabelWidth + fieldWidth + fieldSpacing;
            makeAxisLabel("Z", xOffset3, rowY);
            makeField(xOffset3 + axisLabelWidth, rowY, transform.scale.z, [selected](float v){ auto& r = ECSRegistry::getRegistry(); auto& t = r.getComponent<Transform>(selected); t.scale.z = v; });
        }
    } catch (...) {
        auto msg = std::make_shared<UILabel>();
        msg->text = "Transform: (missing)";
        msg->position = glm::vec2(20.0f, y);
        msg->textColor = glm::vec4(0.8f, 0.4f, 0.4f, 1.0f);
        inspectorWindow.addComponent(msg);
        y += 25.0f;
    }

    y += 10.0f;

    // MeshRenderer section
    try {
        const auto& meshRenderer = registry.getComponent<MeshRenderer>(selected);

        // Section title
        auto meshRendererTitle = std::make_shared<UILabel>();
        meshRendererTitle->text = "Mesh Renderer";
        meshRendererTitle->position = glm::vec2(10.0f, y);
        meshRendererTitle->textColor = glm::vec4(0.9f, 0.9f, 0.9f, 1.0f);
        inspectorWindow.addComponent(meshRendererTitle);
        y += 30.0f;

        // Mesh path
        if (meshRenderer.mesh) {
            auto meshLabel = std::make_shared<UILabel>();
            meshLabel->text = "Mesh";
            meshLabel->position = glm::vec2(20.0f, y + 5.0f);
            meshLabel->textColor = glm::vec4(0.85f, 0.85f, 0.85f, 1.0f);
            inspectorWindow.addComponent(meshLabel);

            auto meshPath = std::make_shared<UILabel>();
            meshPath->text = project.toRelativePath(meshRenderer.mesh->modelPath);
            meshPath->position = glm::vec2(100.0f, y + 5.0f);
            meshPath->textColor = glm::vec4(0.7f, 0.7f, 0.7f, 1.0f);
            inspectorWindow.addComponent(meshPath);
            y += 28.0f;
        }

        // Texture path
        if (meshRenderer.texture) {
            auto texLabel = std::make_shared<UILabel>();
            texLabel->text = "Texture";
            texLabel->position = glm::vec2(20.0f, y + 5.0f);
            texLabel->textColor = glm::vec4(0.85f, 0.85f, 0.85f, 1.0f);
            inspectorWindow.addComponent(texLabel);

            auto texPath = std::make_shared<UILabel>();
            texPath->text = project.toRelativePath(meshRenderer.texture->texturePath);
            texPath->position = glm::vec2(100.0f, y + 5.0f);
            texPath->textColor = glm::vec4(0.7f, 0.7f, 0.7f, 1.0f);
            inspectorWindow.addComponent(texPath);
            y += 28.0f;
        }

        // Color tint (read-only for now)
        auto colorLabel = std::make_shared<UILabel>();
        colorLabel->text = "Color";
        colorLabel->position = glm::vec2(20.0f, y + 5.0f);
        colorLabel->textColor = glm::vec4(0.85f, 0.85f, 0.85f, 1.0f);
        inspectorWindow.addComponent(colorLabel);

        std::string colorStr = "(" +
            std::to_string(meshRenderer.color.r).substr(0, 4) + ", " +
            std::to_string(meshRenderer.color.g).substr(0, 4) + ", " +
            std::to_string(meshRenderer.color.b).substr(0, 4) + ")";
        auto colorValue = std::make_shared<UILabel>();
        colorValue->text = colorStr;
        colorValue->position = glm::vec2(100.0f, y + 5.0f);
        colorValue->textColor = glm::vec4(meshRenderer.color.r, meshRenderer.color.g, meshRenderer.color.b, 1.0f);
        inspectorWindow.addComponent(colorValue);
        y += 35.0f;
    } catch (...) {
        // No MeshRenderer component
    }

    // Tag section
    try {
        const auto& tag = registry.getComponent<Tag>(selected);

        auto tagTitle = std::make_shared<UILabel>();
        tagTitle->text = "Tag";
        tagTitle->position = glm::vec2(10.0f, y);
        tagTitle->textColor = glm::vec4(0.9f, 0.9f, 0.9f, 1.0f);
        inspectorWindow.addComponent(tagTitle);
        y += 30.0f;

        auto tagField = std::make_shared<UIInputField>();
        tagField->position = glm::vec2(20.0f, y);
        tagField->size = glm::vec2(inspectorWindow.getContentSize().x - 40.0f, 28.0f);
        tagField->backgroundColor = glm::vec4(0.18f, 0.18f, 0.2f, 1.0f);
        tagField->focusedColor = glm::vec4(0.22f, 0.22f, 0.25f, 1.0f);
        tagField->hoverColor = glm::vec4(0.2f, 0.2f, 0.22f, 1.0f);
        tagField->borderWidth = 1.0f;
        tagField->padding = 6.0f;
        tagField->text = tag.tag;
        tagField->onCommit = [this, selected](const std::string& s){
            auto& r = ECSRegistry::getRegistry();
            auto& t = r.getComponent<Tag>(selected);
            t.tag = s;
            markSceneModified();
            // Rebuild hierarchy to show updated name
            initializeUnityLayout();
        };
        inspectorWindow.addComponent(tagField);
        y += 38.0f;
    } catch (...) {
        // No Tag component
    }

    // Add Component button
    y += 10.0f;
    auto addButton = std::make_shared<UIButton>();
    addButton->label = "Add Component";
    addButton->position = glm::vec2(10.0f, y);
    addButton->size = glm::vec2(inspectorWindow.getContentSize().x - 20.0f, 32.0f);
    addButton->normalColor = glm::vec4(0.15f, 0.15f, 0.17f, 1.0f);
    addButton->hoverColor = glm::vec4(0.2f, 0.2f, 0.22f, 1.0f);
    addButton->pressedColor = glm::vec4(0.12f, 0.12f, 0.14f, 1.0f);
    addButton->textColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    addButton->onClick = [](){ std::cout << "Add Component clicked" << std::endl; };
    inspectorWindow.addComponent(addButton);
}

void Engine::loadScene(const std::string& scenePath) {
    isLoadingScene = true;
    currentScenePath = scenePath;

    // Use SceneLoader to load scene data into ECS
    updateLoadingProgress(0.1f);
    SceneLoader::loadScene(scenePath, project);
    updateLoadingProgress(0.2f);

    // Load camera pose from scene if present
    {
        glm::vec3 camPos = camera.getPosition();
        glm::vec3 camRot = camera.getRotation();
        if (SceneLoader::loadCameraFromScene(scenePath, camPos, camRot)) {
            camera.setViewYXZ(camPos, camRot);
            camera.updateViewMatrix();
        }
    }
    updateLoadingProgress(0.25f);

    // Load GPU resources for all entities with MeshRenderer components
    auto& reg = ECSRegistry::getRegistry();
    auto entities = reg.getEntities();
    size_t entityCount = 0;
    size_t totalEntities = 0;

    // Count entities with MeshRenderer
    for (const auto& entity : entities) {
        const auto& components = reg.getComponents(entity);
        if (components.find(std::type_index(typeid(MeshRenderer))) != components.end()) {
            totalEntities++;
        }
    }

    // Load resources with progress updates
    for (const auto& entity : entities) {
        const auto& components = reg.getComponents(entity);
        if (components.find(std::type_index(typeid(MeshRenderer))) != components.end()) {
            auto& meshRenderer = reg.getComponent<MeshRenderer>(entity);
            std::string modelPath = meshRenderer.mesh->modelPath;
            std::string texturePath = meshRenderer.texture ? meshRenderer.texture->texturePath : "";

            // Load mesh resources using ResourceManager
            resourceManager->loadMeshResources(modelPath, texturePath);
            entityCount++;

            // Progress from 25% to 60%
            float progress = 0.25f + (0.35f * static_cast<float>(entityCount) / static_cast<float>(std::max(totalEntities, size_t(1))));
            updateLoadingProgress(progress);
        }
    }

    // Create uniform buffers and descriptor sets
    updateLoadingProgress(0.65f);
    uint32_t imageCount = static_cast<uint32_t>(swapChain->imageCount());
    createUniformBuffers(device, physicalDevice, imageCount,
                        uniformBuffers, uniformBuffersMemory);

    updateLoadingProgress(0.70f);

    uint32_t meshCount = static_cast<uint32_t>(resourceManager->getMeshCache().size());
    createDescriptorPool(device, meshCount, imageCount, descriptorPool);
    createDescriptorSets(device, descriptorPool, descriptorSetLayout, uniformBuffers,
                        imageCount, &resourceManager->getMeshCache());
    updateLoadingProgress(0.75f);

    // Create graphics pipeline
    PipelineConfigInfo pipelineConfig{};
    Pipeline::defaultPipelineConfigInfo(pipelineConfig);
    pipelineConfig.renderPass = renderPass;
    pipelineConfig.pipelineLayout = pipelineLayout;
    pipelineConfig.multisampleInfo.rasterizationSamples = msaaSamples;

    std::string vertShader = project.getShadersPath() + "/vert.spv";
    std::string fragShader = project.getShadersPath() + "/frag.spv";
    pipeline = std::make_unique<Pipeline>(device, vertShader, fragShader, pipelineConfig);
    updateLoadingProgress(0.85f);

    // Initialize outline rendering resources
    initOutlineResources(device, physicalDevice, swapChain->getSwapChainExtent(),
                        descriptorSetLayout, imageCount, outlineResources);
    // Create outline composite pipeline after render pass is ready
    createOutlineCompositePipeline(device, renderPass, swapChain->getSwapChainExtent(), msaaSamples, outlineResources);
    updateLoadingProgress(0.90f);

    // Initialize Unity-like layout
    initializeUnityLayout();
    updateLoadingProgress(0.95f);

    // Small delay to show progress before hiding
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    updateLoadingProgress(1.0f);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Hide loading modal and mark scene as clean
    showLoadingModal(false);
    isLoadingScene = false;
    sceneModified = false;
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

    // Inspector UI is rebuilt on selection change; avoid rebuilding each frame to preserve hover/focus

    // Sync selected entity from UI for outline rendering
    selectedEntity = uiRenderer->getSelectedEntity();

    // Record command buffer
    vkResetCommandBuffer(commandBuffers[currentFrame], 0);
    auto& registry = ECSRegistry::getRegistry();
    impgine::recordCommandBuffer(commandBuffers[currentFrame], imageIndex, renderPass,
                                 swapChainFramebuffers, swapChain->getSwapChainExtent(),
                                 pipeline.get(), pipelineLayout,
                                 &resourceManager->getMeshCache(), &registry,
                                 uiRenderer.get(), window.get(), &uiWindows,
                                 &outlineResources, selectedEntity);

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

    // Recreate outline resources with new extent
    if (outlineResources.initialized) {
        recreateOutlineResources(device, physicalDevice, swapChain->getSwapChainExtent(), outlineResources);
    }
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
                                     app->uiRenderer.get(), app->uiWindows, app);
}

void Engine::characterCallback(GLFWwindow* window, unsigned int codepoint) {
    auto app = reinterpret_cast<Engine*>(glfwGetWindowUserPointer(window));
    InputManager::characterCallback(window, codepoint, app->uiWindows);
}

void Engine::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    auto app = reinterpret_cast<Engine*>(glfwGetWindowUserPointer(window));
    InputManager::keyCallback(window, key, scancode, action, mods, app->uiWindows);
}

void Engine::windowCloseCallback(GLFWwindow* window) {
    auto app = reinterpret_cast<Engine*>(glfwGetWindowUserPointer(window));

    // Release mouse capture if viewport is focused
    if (app->inputManager && app->inputManager->isMouseCaptured()) {
        app->inputManager->setMouseCaptured(false);
        app->window->setCursorInputMode(GLFW_CURSOR_NORMAL);
    }

    // Check if scene has been modified
    if (app->sceneModified) {
        // Cancel the close and show dialog
        app->window->cancelClose();
        app->showUnsavedChangesDialog();
    }
    // If not modified, let the window close normally
}

void Engine::saveCurrentScene() {
    if (currentScenePath.empty()) {
        std::cerr << "No scene path to save to" << std::endl;
        return;
    }

    auto& registry = ECSRegistry::getRegistry();
    if (SceneLoader::saveScene(currentScenePath, project, camera, registry)) {
        sceneModified = false;
        std::cout << "Scene saved to: " << currentScenePath << std::endl;
    } else {
        std::cerr << "Failed to save scene to: " << currentScenePath << std::endl;
    }
}

void Engine::showLoadingModal(bool show, float progress) {
    if (!uiRenderer) return;

    // Remove existing loading modal if any
    auto it = std::find_if(uiWindows.begin(), uiWindows.end(),
        [](const UIWindow& w) { return w.type == UIWindowType::LoadingModal; });

    if (it != uiWindows.end()) {
        uiWindows.erase(it);
    }

    if (!show) return;

    // Create loading modal
    UIWindow loadingModal;
    loadingModal.title = "";
    loadingModal.type = UIWindowType::LoadingModal;
    loadingModal.dockPosition = DockPosition::Floating;
    loadingModal.isVisible = true;

    // Center the modal on screen
    int ww = 0, wh = 0;
    window->getFramebufferSize(&ww, &wh);

    float modalWidth = 400.0f;
    float modalHeight = 180.0f;
    loadingModal.position = glm::vec2((ww - modalWidth) / 2.0f, (wh - modalHeight) / 2.0f);
    loadingModal.size = glm::vec2(modalWidth, modalHeight);

    // Dark semi-transparent background
    loadingModal.backgroundColor = glm::vec4(0.1f, 0.1f, 0.1f, 0.95f);
    loadingModal.titleBarHeight = 0.0f;
    loadingModal.borderWidth = 2.0f;
    loadingModal.borderColor = glm::vec4(0.3f, 0.3f, 0.3f, 1.0f);

    // Add loading title
    auto loadingLabel = std::make_shared<UILabel>();
    loadingLabel->text = "Loading Scene...";
    loadingLabel->position = glm::vec2(modalWidth / 2.0f - 80.0f, 30.0f);
    loadingLabel->textColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    loadingModal.addComponent(loadingLabel);

    // Add progress bar background
    auto progressBg = std::make_shared<UIButton>();
    progressBg->label = "";
    progressBg->position = glm::vec2(40.0f, 80.0f);
    progressBg->size = glm::vec2(modalWidth - 80.0f, 30.0f);
    progressBg->normalColor = glm::vec4(0.15f, 0.15f, 0.15f, 1.0f);
    progressBg->hoverColor = glm::vec4(0.15f, 0.15f, 0.15f, 1.0f);
    progressBg->pressedColor = glm::vec4(0.15f, 0.15f, 0.15f, 1.0f);
    progressBg->isEnabled = false;
    loadingModal.addComponent(progressBg);

    // Add progress bar fill
    auto progressFill = std::make_shared<UIButton>();
    progressFill->label = "";
    progressFill->position = glm::vec2(40.0f, 80.0f);
    float fillWidth = (modalWidth - 80.0f) * glm::clamp(progress, 0.0f, 1.0f);
    progressFill->size = glm::vec2(fillWidth, 30.0f);
    progressFill->normalColor = glm::vec4(0.2f, 0.6f, 0.9f, 1.0f);
    progressFill->hoverColor = glm::vec4(0.2f, 0.6f, 0.9f, 1.0f);
    progressFill->pressedColor = glm::vec4(0.2f, 0.6f, 0.9f, 1.0f);
    progressFill->isEnabled = false;
    loadingModal.addComponent(progressFill);

    // Add percentage text
    auto percentLabel = std::make_shared<UILabel>();
    int percent = static_cast<int>(progress * 100.0f);
    percentLabel->text = std::to_string(percent) + "%";
    percentLabel->position = glm::vec2(modalWidth / 2.0f - 20.0f, 125.0f);
    percentLabel->textColor = glm::vec4(0.9f, 0.9f, 0.9f, 1.0f);
    loadingModal.addComponent(percentLabel);

    uiWindows.push_back(loadingModal);
}

void Engine::initializeLoadingScreen() {
    // Create basic rendering infrastructure to show loading screen
    // This is a minimal setup just for the loading UI

    // Create command buffers for initial rendering
    createCommandBuffers(device, commandPool, SwapChain::MAX_FRAMES_IN_FLIGHT, commandBuffers);

    // Create basic UI renderer
    std::string uiVert = project.getShadersPath() + "/ui.vert.spv";
    std::string uiFrag = project.getShadersPath() + "/ui.frag.spv";

    // We need a temporary descriptor set layout and pipeline layout for UI
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = 1;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding, samplerLayoutBinding};
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout!");
    }

    // Create pipeline layout
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

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create pipeline layout!");
    }

    // Initialize UI renderer
    uiRenderer = std::make_unique<UIRenderer>(device, physicalDevice, renderPass, pipelineLayout,
                                              commandPool, graphicsQueue, *swapChain, msaaSamples,
                                              uiVert, uiFrag);

    // Load font
    std::string fontPath = project.getEnginePath() + "/TTF/arial.ttf";
    uiRenderer->getTextRenderer()->loadFont(fontPath, 48);
    uiRenderer->createDescriptorSets();
}

void Engine::renderLoadingFrame(float progress) {
    (void)progress; // Unused parameter, progress is already shown in the UI
    if (!uiRenderer) return;

    window->pollEvents();

    // Wait for previous frame
    VkFence inFlightFence = swapChain->getInFlightFence(currentFrame);
    vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkSemaphore imageAvailableSemaphore = swapChain->getImageAvailableSemaphore(currentFrame);
    auto result = swapChain->acquireNextImage(&imageIndex, currentFrame, imageAvailableSemaphore);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        return; // Skip this frame
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    vkResetFences(device, 1, &inFlightFence);

    // Record command buffer with just the loading UI
    vkResetCommandBuffer(commandBuffers[currentFrame], 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(commandBuffers[currentFrame], &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapChain->getSwapChainExtent();

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{0.01f, 0.01f, 0.01f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffers[currentFrame], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Render UI (loading screen)
    uiRenderer->record(commandBuffers[currentFrame], *window, uiWindows, imageIndex);

    vkCmdEndRenderPass(commandBuffers[currentFrame]);

    if (vkEndCommandBuffer(commandBuffers[currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }

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
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        return; // Skip
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("failed to present swap chain image!");
    }

    currentFrame = (currentFrame + 1) % SwapChain::MAX_FRAMES_IN_FLIGHT;
}

void Engine::updateLoadingProgress(float progress) {
    showLoadingModal(true, progress);
    renderLoadingFrame(progress);
}

void Engine::showUnsavedChangesDialog() {
    if (!uiRenderer) return;

    // Remove existing dialog if any
    auto it = std::find_if(uiWindows.begin(), uiWindows.end(),
        [](const UIWindow& w) { return w.type == UIWindowType::UnsavedChangesModal; });

    if (it != uiWindows.end()) {
        return; // Already showing
    }

    // Create unsaved changes modal
    UIWindow dialog;
    dialog.title = "Unsaved Changes";
    dialog.type = UIWindowType::UnsavedChangesModal;
    dialog.dockPosition = DockPosition::Floating;
    dialog.isVisible = true;

    // Center the modal on screen
    int ww = 0, wh = 0;
    window->getFramebufferSize(&ww, &wh);

    float dialogWidth = 400.0f;
    float dialogHeight = 200.0f;
    dialog.position = glm::vec2((ww - dialogWidth) / 2.0f, (wh - dialogHeight) / 2.0f);
    dialog.size = glm::vec2(dialogWidth, dialogHeight);

    dialog.backgroundColor = glm::vec4(0.15f, 0.15f, 0.15f, 1.0f);
    dialog.titleBarColor = glm::vec4(0.2f, 0.2f, 0.2f, 1.0f);
    dialog.titleBarHeight = 35.0f;
    dialog.borderWidth = 2.0f;
    dialog.borderColor = glm::vec4(0.3f, 0.3f, 0.3f, 1.0f);

    // Add message text
    auto messageLabel = std::make_shared<UILabel>();
    messageLabel->text = "You have unsaved changes.";
    messageLabel->position = glm::vec2(20.0f, 20.0f);
    messageLabel->textColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    dialog.addComponent(messageLabel);

    auto messageLabel2 = std::make_shared<UILabel>();
    messageLabel2->text = "Do you want to save before closing?";
    messageLabel2->position = glm::vec2(20.0f, 50.0f);
    messageLabel2->textColor = glm::vec4(0.9f, 0.9f, 0.9f, 1.0f);
    dialog.addComponent(messageLabel2);

    // Add buttons
    float buttonWidth = 110.0f;
    float buttonHeight = 35.0f;
    float buttonSpacing = 10.0f;
    float totalButtonWidth = buttonWidth * 3 + buttonSpacing * 2;
    float startX = (dialogWidth - totalButtonWidth) / 2.0f;
    float buttonY = dialogHeight - buttonHeight - 45.0f;

    // Save button
    auto saveButton = std::make_shared<UIButton>();
    saveButton->label = "Save";
    saveButton->position = glm::vec2(startX, buttonY);
    saveButton->size = glm::vec2(buttonWidth, buttonHeight);
    saveButton->normalColor = glm::vec4(0.2f, 0.5f, 0.8f, 1.0f);
    saveButton->hoverColor = glm::vec4(0.3f, 0.6f, 0.9f, 1.0f);
    saveButton->pressedColor = glm::vec4(0.15f, 0.4f, 0.7f, 1.0f);
    saveButton->textColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    saveButton->onClick = [this]() {
        saveCurrentScene();
        // Close the dialog
        auto it = std::find_if(uiWindows.begin(), uiWindows.end(),
            [](const UIWindow& w) { return w.type == UIWindowType::UnsavedChangesModal; });
        if (it != uiWindows.end()) uiWindows.erase(it);
        // Proceed with closing - for now just close the window
        window->close();
    };
    dialog.addComponent(saveButton);

    // Don't Save button
    auto dontSaveButton = std::make_shared<UIButton>();
    dontSaveButton->label = "Don't Save";
    dontSaveButton->position = glm::vec2(startX + buttonWidth + buttonSpacing, buttonY);
    dontSaveButton->size = glm::vec2(buttonWidth, buttonHeight);
    dontSaveButton->normalColor = glm::vec4(0.6f, 0.3f, 0.3f, 1.0f);
    dontSaveButton->hoverColor = glm::vec4(0.7f, 0.4f, 0.4f, 1.0f);
    dontSaveButton->pressedColor = glm::vec4(0.5f, 0.2f, 0.2f, 1.0f);
    dontSaveButton->textColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    dontSaveButton->onClick = [this]() {
        // Close the dialog and window without saving
        auto it = std::find_if(uiWindows.begin(), uiWindows.end(),
            [](const UIWindow& w) { return w.type == UIWindowType::UnsavedChangesModal; });
        if (it != uiWindows.end()) uiWindows.erase(it);
        sceneModified = false;
        window->close();
    };
    dialog.addComponent(dontSaveButton);

    // Cancel button
    auto cancelButton = std::make_shared<UIButton>();
    cancelButton->label = "Cancel";
    cancelButton->position = glm::vec2(startX + (buttonWidth + buttonSpacing) * 2, buttonY);
    cancelButton->size = glm::vec2(buttonWidth, buttonHeight);
    cancelButton->normalColor = glm::vec4(0.25f, 0.25f, 0.25f, 1.0f);
    cancelButton->hoverColor = glm::vec4(0.35f, 0.35f, 0.35f, 1.0f);
    cancelButton->pressedColor = glm::vec4(0.2f, 0.2f, 0.2f, 1.0f);
    cancelButton->textColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    cancelButton->onClick = [this]() {
        // Just close the dialog
        auto it = std::find_if(uiWindows.begin(), uiWindows.end(),
            [](const UIWindow& w) { return w.type == UIWindowType::UnsavedChangesModal; });
        if (it != uiWindows.end()) uiWindows.erase(it);
    };
    dialog.addComponent(cancelButton);

    uiWindows.push_back(dialog);
}

void Engine::rebuildHierarchyUI() {
    if (uiWindows.size() <= 1) return;

    auto& hierarchyWindow = uiWindows[1];  // Hierarchy is second window
    hierarchyWindow.components.clear();

    auto& registry = ECSRegistry::getRegistry();
    const auto& entities = registry.getEntities();

    // Build a set of root entities (those without parents)
    std::vector<Entity> rootEntities;
    for (const auto& entity : entities) {
        Entity parent = registry.getParent(entity);
        if (parent == INVALID_ENTITY) {
            rootEntities.push_back(entity);
        }
    }

    // Recursive helper to add tree nodes
    float y = 10.0f;
    const float rowHeight = 24.0f;

    std::function<void(Entity, int)> addEntityNode = [&](Entity entity, int depth) {
        std::string entityName = "Entity " + std::to_string(entity);
        try {
            const auto& tag = registry.getComponent<Tag>(entity);
            if (!tag.tag.empty()) {
                entityName = tag.tag;
            }
        } catch (...) {}

        auto treeNode = std::make_shared<UITreeNode>();
        treeNode->label = entityName;
        treeNode->entityId = entity;
        treeNode->depth = depth;
        treeNode->position = glm::vec2(0.0f, y);
        treeNode->size = glm::vec2(hierarchyWindow.getContentSize().x, rowHeight);
        treeNode->isSelected = (selectedEntity == entity);

        // Check if entity has children
        auto children = registry.getChildren(entity);
        treeNode->hasChildren = !children.empty();

        // Check if expanded
        auto expandIt = expandedEntities.find(entity);
        treeNode->isExpanded = (expandIt != expandedEntities.end()) ? expandIt->second : true;

        // Click handler
        treeNode->onClick = [this, entity](uint32_t) {
            selectedEntity = entity;
            if (uiRenderer) uiRenderer->setSelectedEntity(entity);
            rebuildInspectorUI();
            // Don't rebuild hierarchy here - it will be rebuilt on next frame
        };

        // Expand/collapse handler - mark for rebuild, don't do it immediately
        treeNode->onToggleExpand = [this, entity]() {
            expandedEntities[entity] = !expandedEntities[entity];
            needsHierarchyRebuild = true;
        };

        // Drop handler for reparenting
        treeNode->onDrop = [this, entity](uint32_t draggedEnt, uint32_t) {
            if (draggedEnt != entity) {
                auto& reg = ECSRegistry::getRegistry();

                // Prevent circular dependencies
                Entity checkParent = entity;
                bool wouldCreateCycle = false;
                while (checkParent != INVALID_ENTITY) {
                    if (checkParent == draggedEnt) {
                        wouldCreateCycle = true;
                        break;
                    }
                    checkParent = reg.getParent(checkParent);
                }

                if (!wouldCreateCycle) {
                    reg.setParent(draggedEnt, entity);
                    markSceneModified();
                    needsHierarchyRebuild = true;
                }
            }
        };

        hierarchyWindow.addComponent(treeNode);
        y += rowHeight;

        // Add children if expanded
        if (treeNode->isExpanded) {
            auto children = registry.getChildren(entity);
            for (Entity child : children) {
                addEntityNode(child, depth + 1);
            }
        }
    };

    // Add all root entities
    for (Entity root : rootEntities) {
        addEntityNode(root, 0);
    }

    // Add "Create Entity" button at the bottom
    y += 10.0f;
    auto createButton = std::make_shared<UIButton>();
    createButton->label = "+ Create Entity";
    createButton->position = glm::vec2(10.0f, y);
    createButton->size = glm::vec2(hierarchyWindow.getContentSize().x - 20.0f, 30.0f);
    createButton->normalColor = glm::vec4(0.25f, 0.45f, 0.35f, 1.0f);
    createButton->hoverColor = glm::vec4(0.30f, 0.55f, 0.45f, 1.0f);
    createButton->pressedColor = glm::vec4(0.20f, 0.40f, 0.30f, 1.0f);
    createButton->onClick = [this]() {
        // Create with default mesh if available
        std::string defaultModel = project.getFullPath("assets/models/viking_room.obj");
        std::string defaultTexture = project.getFullPath("assets/textures/viking_room.png");
        auto& reg = ECSRegistry::getRegistry();
        Entity newEntity = reg.createEntity();
        reg.addComponent<Tag>(newEntity, Tag{"New Entity"});
        reg.addComponent<Transform>(newEntity, Transform{});
        reg.addComponent<Active>(newEntity, Active{true, true});

        // Create empty mesh renderer (user can set paths in inspector)
        reg.addComponent<MeshRenderer>(newEntity, MeshRenderer{
            std::make_shared<Mesh>(defaultModel),
            std::make_shared<Texture2D>(defaultTexture),
            glm::vec3(1.0f)
        });

        selectedEntity = newEntity;
        markSceneModified();
        needsHierarchyRebuild = true;
        rebuildInspectorUI();
    };
    hierarchyWindow.addComponent(createButton);
}

}  // namespace impgine
