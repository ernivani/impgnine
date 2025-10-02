#include "input_manager.hpp"
#include "../ui/UIComponents.hpp"
#include "../scene/scene_loader.hpp"
#include "../engine.hpp"
#include <GLFW/glfw3.h>
#include <iostream>

namespace impgine {

namespace {
    // Blur an input: if empty, display 0; always defocus
    inline void blurInput(std::shared_ptr<UIInputField>& input) {
        if (!input) return;
        if (input->text.empty()) {
            input->text = "0"; // show 0 only on blur
            input->cursorPosition = input->text.length();
            input->clearSelection();
        }
        input->state = UIComponentState::Normal;
    }
}

InputManager::InputManager(Window* window, Camera* camera, Engine* engine)
    : window(window), camera(camera), engine(engine), lastMouseX(400.0), lastMouseY(300.0) {
}

void InputManager::processInput(float deltaTime) {
    // Track modifier keys
    shiftPressed = window->isKeyPressed(GLFW_KEY_LEFT_SHIFT) || window->isKeyPressed(GLFW_KEY_RIGHT_SHIFT);
    altPressed = window->isKeyPressed(GLFW_KEY_LEFT_ALT) || window->isKeyPressed(GLFW_KEY_RIGHT_ALT);

    // ESC to release mouse capture (always check this)
    if (window->isKeyPressed(GLFW_KEY_ESCAPE)) {
        if (mouseCaptured) {
            mouseCaptured = false;
            window->setCursorInputMode(GLFW_CURSOR_NORMAL);
        }
    }

    // Arrow keys for panning (always available)
    const float panSpeed = 5.0f * deltaTime;
    if (window->isKeyPressed(GLFW_KEY_UP)) {
        camera->panCamera(0, panSpeed * 100.0f);
        camera->updateViewMatrix();
    }
    if (window->isKeyPressed(GLFW_KEY_DOWN)) {
        camera->panCamera(0, -panSpeed * 100.0f);
        camera->updateViewMatrix();
    }
    if (window->isKeyPressed(GLFW_KEY_LEFT)) {
        camera->panCamera(-panSpeed * 100.0f, 0);
        camera->updateViewMatrix();
    }
    if (window->isKeyPressed(GLFW_KEY_RIGHT)) {
        camera->panCamera(panSpeed * 100.0f, 0);
        camera->updateViewMatrix();
    }

    // F key to frame selected object
    if (window->isKeyPressed(GLFW_KEY_F) && engine && engine->selectedEntity != INVALID_ENTITY) {
        auto& registry = ECSRegistry::getRegistry();
        try {
            auto& transform = registry.getComponent<Transform>(engine->selectedEntity);
            camera->frameTarget(transform.position, 5.0f);
        } catch (...) {
            // Entity doesn't have a Transform component
        }
    }

    // Only process WASD movement if right-click is held (flythrough mode)
    if (!rightButtonPressed) return;

    float moveSpeed = 5.0f; // units per second
    if (shiftPressed) {
        moveSpeed *= 2.0f; // Faster movement with Shift
    }

    // WASD movement (flythrough mode with right-click)
    if (window->isKeyPressed(GLFW_KEY_W)) {
        camera->moveForward(moveSpeed * deltaTime);
    }
    if (window->isKeyPressed(GLFW_KEY_S)) {
        camera->moveBackward(moveSpeed * deltaTime);
    }
    if (window->isKeyPressed(GLFW_KEY_A)) {
        camera->moveLeft(moveSpeed * deltaTime);
    }
    if (window->isKeyPressed(GLFW_KEY_D)) {
        camera->moveRight(moveSpeed * deltaTime);
    }

    // Q/E for up/down movement (flythrough mode)
    if (window->isKeyPressed(GLFW_KEY_Q) || window->isKeyPressed(GLFW_KEY_SPACE)) {
        camera->moveUp(moveSpeed * deltaTime);
    }
    if (window->isKeyPressed(GLFW_KEY_E) || window->isKeyPressed(GLFW_KEY_LEFT_CONTROL)) {
        camera->moveDown(moveSpeed * deltaTime);
    }
}

void InputManager::handleMouseMovement() {
    // Handle mouse movement for various camera modes
    if (currentMode == CameraMode::None) return;

    double xpos, ypos;
    window->getCursorPos(&xpos, &ypos);

    if (firstMouse) {
        lastMouseX = xpos;
        lastMouseY = ypos;
        firstMouse = false;
        return;
    }

    double xoffset = xpos - lastMouseX;
    double yoffset = ypos - lastMouseY;

    lastMouseX = xpos;
    lastMouseY = ypos;

    const float sensitivity = 0.002f;

    switch (currentMode) {
        case CameraMode::Rotate:
            // Right-click drag: rotate view
            camera->rotateYaw(static_cast<float>(xoffset * sensitivity));
            camera->rotatePitch(static_cast<float>(yoffset * sensitivity));
            camera->updateViewMatrix();
            break;

        case CameraMode::Pan:
            // Middle-click drag: pan view
            camera->panCamera(static_cast<float>(xoffset), static_cast<float>(-yoffset));
            camera->updateViewMatrix();
            break;

        case CameraMode::Orbit:
            // Alt + Left-click: orbit around target
            camera->orbitCamera(
                static_cast<float>(xoffset * sensitivity),
                static_cast<float>(yoffset * sensitivity),
                orbitTarget
            );
            camera->updateViewMatrix();
            break;

        case CameraMode::Zoom:
            // Alt + Right-click: smooth zoom
            camera->zoomCamera(static_cast<float>(-yoffset * 0.1f));
            camera->updateViewMatrix();
            break;

        default:
            break;
    }
}

void InputManager::updateComponentHover(std::vector<UIWindow>& uiWindows, float mouseX, float mouseY) {
    // Reset all component states to normal first
    for (auto& window : uiWindows) {
        for (auto& comp : window.components) {
            if (comp->state != UIComponentState::Focused) {
                comp->state = UIComponentState::Normal;
            }
            // Reset drop target state for tree nodes
            if (comp->type == UIComponentType::TreeNode) {
                auto treeNode = std::dynamic_pointer_cast<UITreeNode>(comp);
                if (treeNode) {
                    treeNode->isDropTarget = false;
                }
            }
        }
    }

    // Check which component is being hovered
    for (auto& window : uiWindows) {
        if (!window.isVisible) continue;

        glm::vec2 contentPos = window.getContentPosition();
        glm::vec2 contentSize = window.getContentSize();

        for (auto& comp : window.components) {
            if (!comp->isEnabled || !comp->isVisible) continue;

            // Flip Y position to match rendering
            float flippedY = contentSize.y - comp->position.y - comp->size.y;
            glm::vec2 absolutePos = glm::vec2(contentPos.x + comp->position.x, contentPos.y + flippedY);

            if (mouseX >= absolutePos.x && mouseX <= absolutePos.x + comp->size.x &&
                mouseY >= absolutePos.y && mouseY <= absolutePos.y + comp->size.y) {

                if (comp->state != UIComponentState::Focused) {
                    comp->state = UIComponentState::Hovered;
                }

                // If dragging an entity, mark tree nodes as drop targets
                if (engine->isDraggingEntity && comp->type == UIComponentType::TreeNode) {
                    auto treeNode = std::dynamic_pointer_cast<UITreeNode>(comp);
                    if (treeNode && treeNode->entityId != engine->draggedEntity) {
                        treeNode->isDropTarget = true;
                    }
                }

                return; // Only one component can be hovered at a time
            }
        }
    }
}

void InputManager::handleUIInput(UIRenderer* uiRenderer, std::vector<UIWindow>& uiWindows) {
    // Get mouse position in screen coordinates
    double mx, my;
    window->getCursorPos(&mx, &my);

    // Scale mouse coordinates to framebuffer coordinates for UI hit testing
    int windowWidth = 0, windowHeight = 0;
    int fbWidth = 0, fbHeight = 0;
    window->getWindowSize(&windowWidth, &windowHeight);
    window->getFramebufferSize(&fbWidth, &fbHeight);

    float scaleX = (windowWidth > 0) ? static_cast<float>(fbWidth) / static_cast<float>(windowWidth) : 1.0f;
    float scaleY = (windowHeight > 0) ? static_cast<float>(fbHeight) / static_cast<float>(windowHeight) : 1.0f;

    float mouseX = static_cast<float>(mx) * scaleX;
    float mouseY = static_cast<float>(fbHeight) - (static_cast<float>(my) * scaleY);

    // Handle slider dragging
    if (draggedSlider && window->isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) {
        // Find the slider's window to get content position
        for (auto& uiWindow : uiWindows) {
            if (!uiWindow.isVisible) continue;

            glm::vec2 contentPos = uiWindow.getContentPosition();
            glm::vec2 contentSize = uiWindow.getContentSize();

            for (auto& comp : uiWindow.components) {
                if (comp == draggedSlider) {
                    float flippedY = contentSize.y - comp->position.y - comp->size.y;
                    glm::vec2 absolutePos = glm::vec2(contentPos.x + comp->position.x, contentPos.y + flippedY);
                    float relativeX = mouseX - absolutePos.x;
                    float normalizedValue = glm::clamp(relativeX / comp->size.x, 0.0f, 1.0f);
                    float newValue = draggedSlider->minValue + normalizedValue * (draggedSlider->maxValue - draggedSlider->minValue);
                    draggedSlider->setValue(newValue);
                    break;
                }
            }
        }
    }

    // Update component hover states
    updateComponentHover(uiWindows, mouseX, mouseY);

    // Change cursor to I-beam when hovering an input field
    bool overInput = false;
    for (auto& windowRef : uiWindows) {
        if (!windowRef.isVisible) continue;
        for (auto& comp : windowRef.components) {
            if (comp->type == UIComponentType::InputField && comp->state == UIComponentState::Hovered) {
                overInput = true; break;
            }
        }
        if (overInput) break;
    }
    if (overInput) {
        glfwSetCursor(window->getGLFWWindow(), glfwCreateStandardCursor(GLFW_IBEAM_CURSOR));
    } else {
        glfwSetCursor(window->getGLFWWindow(), nullptr);
    }

    // Check if mouse is over any UI panel
    bool mouseOverUI = uiRenderer->getLayout().isPointOverUI(mouseX, mouseY, uiWindows);

    // Track mouse button states for viewport navigation
    leftButtonPressed = window->isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT);
    rightButtonPressed = window->isMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT);
    middleButtonPressed = window->isMouseButtonPressed(GLFW_MOUSE_BUTTON_MIDDLE);

    // Determine camera mode based on mouse buttons and modifiers
    if (!mouseOverUI) {
        if (altPressed && leftButtonPressed) {
            // Alt + Left-click: Orbit
            if (currentMode == CameraMode::None) {
                currentMode = CameraMode::Orbit;
                // Set orbit target to selected entity or scene center
                if (engine && engine->selectedEntity != INVALID_ENTITY) {
                    auto& registry = ECSRegistry::getRegistry();
                    try {
                        auto& transform = registry.getComponent<Transform>(engine->selectedEntity);
                        orbitTarget = transform.position;
                    } catch (...) {
                        // Entity doesn't have a Transform component, use scene center
                        orbitTarget = glm::vec3(0.0f);
                    }
                } else {
                    orbitTarget = glm::vec3(0.0f);
                }
                firstMouse = true;
            }
        } else if (altPressed && rightButtonPressed) {
            // Alt + Right-click: Zoom
            if (currentMode == CameraMode::None) {
                currentMode = CameraMode::Zoom;
                firstMouse = true;
            }
        } else if (rightButtonPressed) {
            // Right-click: Rotate (flythrough mode)
            if (currentMode == CameraMode::None) {
                currentMode = CameraMode::Rotate;
                firstMouse = true;
            }
        } else if (middleButtonPressed) {
            // Middle-click: Pan
            if (currentMode == CameraMode::None) {
                currentMode = CameraMode::Pan;
                firstMouse = true;
            }
        } else if (leftButtonPressed && !altPressed) {
            // Left-click without Alt: Pick entity in viewport
            // Only pick on initial click (when mode transitions from None)
            if (currentMode == CameraMode::None) {
                Entity pickedEntity = pickEntityAtScreenPosition(mouseX, mouseY, uiRenderer, uiWindows);
                if (pickedEntity != INVALID_ENTITY) {
                    engine->selectedEntity = pickedEntity;
                    if (uiRenderer) uiRenderer->setSelectedEntity(pickedEntity);
                    // Rebuild UIs to show selected entity
                    engine->markHierarchyNeedsRebuild();
                    engine->markInspectorNeedsRebuild();
                }
                // Don't set camera mode, allow single clicks for selection
            }
        } else {
            // No mouse buttons pressed: reset mode
            currentMode = CameraMode::None;
            firstMouse = true;
        }
    } else {
        // Mouse over UI: reset camera mode
        if (!leftButtonPressed && !rightButtonPressed && !middleButtonPressed) {
            currentMode = CameraMode::None;
        }
    }
}

void InputManager::characterCallback(GLFWwindow* window, unsigned int codepoint, std::vector<UIWindow>& uiWindows) {
    (void)window;


    // Find focused input field
    for (auto& uiWindow : uiWindows) {
        if (!uiWindow.isVisible) continue;

        for (auto& comp : uiWindow.components) {
            if (comp->type == UIComponentType::InputField && comp->state == UIComponentState::Focused) {
                auto input = std::dynamic_pointer_cast<UIInputField>(comp);
                if (input && codepoint >= 32 && codepoint < 127) { // Printable ASCII
                    input->insertChar(static_cast<char>(codepoint));
                }
                return;
            }
        }
    }
}

void InputManager::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods, std::vector<UIWindow>& uiWindows) {
    (void)scancode;

    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;

    bool ctrlHeld = (mods & GLFW_MOD_CONTROL) != 0;
    bool cmdHeld = (mods & GLFW_MOD_SUPER) != 0;  // Command key on Mac
    bool shiftHeld = (mods & GLFW_MOD_SHIFT) != 0;
    bool altHeld = (mods & GLFW_MOD_ALT) != 0;    // Option key on Mac

    // On Mac, use Cmd for shortcuts; on other platforms, use Ctrl
    bool modifierHeld = cmdHeld || ctrlHeld;

    // Save scene on Cmd/Ctrl+S
    if ((cmdHeld || ctrlHeld) && key == GLFW_KEY_S) {
        auto appPtr = glfwGetWindowUserPointer(window);
        if (appPtr) {
            auto& registry = ECSRegistry::getRegistry();
            // Avoid needing Engine's private members by querying through Project singleton-like usage
            // We know Engine set the window user pointer to itself; cast and access public members
            impgine::Engine* app = static_cast<impgine::Engine*>(appPtr);
            SceneLoader::saveScene(app->getProject().getFullPath(app->getProject().lastScene), app->getProject(), app->getCamera(), registry);
            std::cout << "Scene saved." << std::endl;
        }
        return;
    }

    // Find focused input field
    for (auto& uiWindow : uiWindows) {
        if (!uiWindow.isVisible) continue;

        for (auto& comp : uiWindow.components) {
            if (comp->type == UIComponentType::InputField && comp->state == UIComponentState::Focused) {
                auto input = std::dynamic_pointer_cast<UIInputField>(comp);
                if (input) {
                    // Handle Cmd/Ctrl shortcuts
                    // Use glfwGetKeyName to get the actual character for keyboard layout independence
                    if (modifierHeld) {
                        const char* keyName = glfwGetKeyName(key, scancode);

                        // Match by character name (works across all keyboard layouts)
                        if (keyName && strlen(keyName) == 1) {
                            char keyChar = tolower(keyName[0]);

                            if (keyChar == 'c') {
                                // Copy
                                if (input->hasSelection()) {
                                    glfwSetClipboardString(window, input->getSelectedText().c_str());
                                }
                            } else if (keyChar == 'x') {
                                // Cut
                                if (input->hasSelection()) {
                                    glfwSetClipboardString(window, input->getSelectedText().c_str());
                                    input->deleteSelection();
                                }
                            } else if (keyChar == 'v') {
                                // Paste
                                const char* clipboardText = glfwGetClipboardString(window);
                                if (clipboardText) {
                                    input->insertText(std::string(clipboardText));
                                }
                            } else if (keyChar == 'a') {
                                // Select all
                                input->selectAll();
                            }
                        }

                        // Also handle arrow keys for Cmd+Left/Right
                        if (key == GLFW_KEY_LEFT) {
                            // Cmd+Left: Jump to start of line (Mac)
                            input->moveCursorToStart(shiftHeld);
                        } else if (key == GLFW_KEY_RIGHT) {
                            // Cmd+Right: Jump to end of line (Mac)
                            input->moveCursorToEnd(shiftHeld);
                        }
                        return;
                    }

                    // Mac-specific: Option+Arrow for word jumping (future enhancement)
                    if (altHeld && (key == GLFW_KEY_LEFT || key == GLFW_KEY_RIGHT)) {
                        // TODO: Implement word-by-word navigation
                        // For now, just do regular movement
                    }

                    // Handle navigation and editing
                    if (key == GLFW_KEY_BACKSPACE) {
                        input->deleteChar();
                    } else if (key == GLFW_KEY_DELETE) {
                        input->deleteForward();
                    } else if (key == GLFW_KEY_LEFT) {
                        input->moveCursorLeft(shiftHeld);
                    } else if (key == GLFW_KEY_RIGHT) {
                        input->moveCursorRight(shiftHeld);
                    } else if (key == GLFW_KEY_HOME) {
                        input->moveCursorToStart(shiftHeld);
                    } else if (key == GLFW_KEY_END) {
                        input->moveCursorToEnd(shiftHeld);
                    } else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) {
                        // Defocus; if empty, display 0
                        blurInput(input);
                    } else if (key == GLFW_KEY_ESCAPE) {
                        // Defocus; if empty, display 0
                        blurInput(input);
                    } else if (key == GLFW_KEY_TAB) {
                        // Move to next/previous input field
                        blurInput(input);

                        // Find all input fields in order
                        std::vector<std::shared_ptr<UIInputField>> allInputs;
                        for (auto& w : uiWindows) {
                            if (!w.isVisible) continue;
                            for (auto& c : w.components) {
                                if (c->type == UIComponentType::InputField && c->isEnabled && c->isVisible) {
                                    auto inp = std::dynamic_pointer_cast<UIInputField>(c);
                                    if (inp) allInputs.push_back(inp);
                                }
                            }
                        }

                        // Find current input index
                        int currentIdx = -1;
                        for (size_t i = 0; i < allInputs.size(); ++i) {
                            if (allInputs[i] == input) {
                                currentIdx = static_cast<int>(i);
                                break;
                            }
                        }

                        // Move to next or previous
                        if (currentIdx >= 0 && !allInputs.empty()) {
                            int nextIdx;
                            if (shiftHeld) {
                                // Shift+Tab: go to previous field
                                nextIdx = (currentIdx - 1 + static_cast<int>(allInputs.size())) % static_cast<int>(allInputs.size());
                            } else {
                                // Tab: go to next field
                                nextIdx = (currentIdx + 1) % static_cast<int>(allInputs.size());
                            }
                            allInputs[nextIdx]->state = UIComponentState::Focused;
                            allInputs[nextIdx]->selectAll();
                        }
                    }
                }
                return;
            }
        }
    }
}

void InputManager::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods, UIRenderer* uiRenderer, std::vector<UIWindow>& uiWindows, Engine* engine) {
    (void)mods;

    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);

        // Convert to framebuffer coordinates
        int fbWidth, fbHeight;
        glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
        int winWidth, winHeight;
        glfwGetWindowSize(window, &winWidth, &winHeight);

        xpos = xpos * fbWidth / winWidth;
        ypos = fbHeight - (ypos * fbHeight / winHeight);

        // Check if clicking on a component
        bool clickedComponent = false;
        for (auto& uiWindow : uiWindows) {
            if (!uiWindow.isVisible) continue;

            glm::vec2 contentPos = uiWindow.getContentPosition();
            glm::vec2 contentSize = uiWindow.getContentSize();

            for (auto& comp : uiWindow.components) {
                if (!comp->isEnabled || !comp->isVisible) continue;

                // Flip Y position to match rendering
                float flippedY = contentSize.y - comp->position.y - comp->size.y;
                glm::vec2 absolutePos = glm::vec2(contentPos.x + comp->position.x, contentPos.y + flippedY);

                if (xpos >= absolutePos.x && xpos <= absolutePos.x + comp->size.x &&
                    ypos >= absolutePos.y && ypos <= absolutePos.y + comp->size.y) {

                    clickedComponent = true;

                    // Handle different component types
                    switch (comp->type) {
                        case UIComponentType::Button: {
                            auto button = std::dynamic_pointer_cast<UIButton>(comp);
                            if (button && button->onClick) {
                                comp->state = UIComponentState::Pressed;
                                button->onClick();
                            }
                            break;
                        }
                        case UIComponentType::Checkbox: {
                            auto checkbox = std::dynamic_pointer_cast<UICheckbox>(comp);
                            if (checkbox) {
                                checkbox->toggle();
                            }
                            break;
                        }
                        case UIComponentType::InputField: {
                            // Focus the input field
                            auto input = std::dynamic_pointer_cast<UIInputField>(comp);
                            if (input) {
                                bool wasAlreadyFocused = (comp->state == UIComponentState::Focused);

                                // Unfocus all other inputs
                                for (auto& w : uiWindows) {
                                    for (auto& c : w.components) {
                                        if (c->type == UIComponentType::InputField && c != comp) {
                                            auto other = std::dynamic_pointer_cast<UIInputField>(c);
                                            blurInput(other);
                                        }
                                    }
                                }
                                comp->state = UIComponentState::Focused;

                                // If field wasn't focused and has text, select all
                                if (!wasAlreadyFocused && !input->text.empty()) {
                                    input->selectAll();
                                } else {
                                    // Set cursor position based on click location
                                    float relativeX = xpos - (absolutePos.x + input->padding);
                                    size_t clickPos = input->getCursorFromPixelOffset(relativeX);
                                    input->cursorPosition = clickPos;
                                    input->selectionStart = clickPos;
                                    input->selectionEnd = clickPos;
                                }
                            }
                            break;
                        }
                        case UIComponentType::Slider: {
                            auto slider = std::dynamic_pointer_cast<UISlider>(comp);
                            if (slider) {
                                draggedSlider = slider;
                                comp->state = UIComponentState::Pressed;

                                // Calculate slider value from click position
                                float relativeX = xpos - absolutePos.x;
                                float normalizedValue = relativeX / comp->size.x;
                                float newValue = slider->minValue + normalizedValue * (slider->maxValue - slider->minValue);
                                slider->setValue(newValue);
                            }
                            break;
                        }
                        case UIComponentType::TreeNode: {
                            auto treeNode = std::dynamic_pointer_cast<UITreeNode>(comp);
                            if (treeNode) {
                                glm::vec2 localClick(xpos - absolutePos.x, ypos - absolutePos.y);

                                // Check if clicking on the arrow
                                float indent = treeNode->getIndentOffset();
                                float arrowX = indent;
                                if (localClick.x >= arrowX && localClick.x <= arrowX + treeNode->arrowSize) {
                                    // Toggle expand/collapse
                                    treeNode->isExpanded = !treeNode->isExpanded;
                                    if (treeNode->onToggleExpand) {
                                        treeNode->onToggleExpand();
                                    }
                                } else {
                                    // Regular click - select entity and start drag
                                    if (treeNode->onClick) {
                                        treeNode->onClick(treeNode->entityId);
                                    }
                                    // Start drag operation
                                    engine->draggedEntity = treeNode->entityId;
                                    engine->isDraggingEntity = true;
                                    treeNode->isDragging = true;
                                }
                            }
                            break;
                        }
                        default:
                            break;
                    }

                    return; // Only click one component
                }
            }
        }

        // If not clicking a component, handle other UI clicks (like hierarchy selection)
        if (!clickedComponent && uiRenderer) {
            // Blur any focused inputs when clicking empty UI space
            for (auto& w : uiWindows) {
                for (auto& c : w.components) {
                    if (c->type == UIComponentType::InputField && c->state == UIComponentState::Focused) {
                        auto input = std::dynamic_pointer_cast<UIInputField>(c);
                        blurInput(input);
                    }
                }
            }
            uiRenderer->handleMouseClick(xpos, ypos, uiWindows);
        }
    }

    // Handle slider drag release
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
        if (draggedSlider) {
            draggedSlider->state = UIComponentState::Normal;
            draggedSlider = nullptr;
        }

        // Handle entity drag and drop
        if (engine->isDraggingEntity) {
            // Find the tree node under the cursor
            double xpos, ypos;
            glfwGetCursorPos(window, &xpos, &ypos);

            UITreeNode* dropTarget = nullptr;

            for (auto& uiWindow : uiWindows) {
                if (!uiWindow.isVisible || uiWindow.type != UIWindowType::Hierarchy) continue;

                glm::vec2 contentPos = uiWindow.getContentPosition();
                glm::vec2 contentSize = uiWindow.getContentSize();

                for (auto& comp : uiWindow.components) {
                    if (!comp->isVisible || comp->type != UIComponentType::TreeNode) continue;

                    auto treeNode = std::dynamic_pointer_cast<UITreeNode>(comp);
                    if (!treeNode) continue;

                    // Calculate absolute position (flip Y)
                    float flippedY = contentSize.y - comp->position.y - comp->size.y;
                    glm::vec2 absolutePos = glm::vec2(contentPos.x + comp->position.x, contentPos.y + flippedY);

                    if (xpos >= absolutePos.x && xpos <= absolutePos.x + comp->size.x &&
                        ypos >= absolutePos.y && ypos <= absolutePos.y + comp->size.y) {
                        dropTarget = treeNode.get();
                        break;
                    }
                }
            }

            // Execute drop if valid target found
            if (dropTarget && dropTarget->onDrop) {
                dropTarget->onDrop(engine->draggedEntity, dropTarget->entityId);
            }

            // Reset all tree nodes' drag state
            for (auto& uiWindow : uiWindows) {
                for (auto& comp : uiWindow.components) {
                    if (comp->type == UIComponentType::TreeNode) {
                        auto treeNode = std::dynamic_pointer_cast<UITreeNode>(comp);
                        if (treeNode) {
                            treeNode->isDragging = false;
                            treeNode->isDropTarget = false;
                        }
                    }
                }
            }

            engine->isDraggingEntity = false;
            engine->draggedEntity = INVALID_ENTITY;
        }
    }
}

void InputManager::scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    (void)xoffset;

    auto appPtr = glfwGetWindowUserPointer(window);
    if (!appPtr) return;

    auto* engine = static_cast<impgine::Engine*>(appPtr);
    auto& camera = engine->getCamera();

    // Zoom camera with scroll wheel
    camera.zoomCamera(static_cast<float>(yoffset));
    camera.updateViewMatrix();
}

Entity InputManager::pickEntityAtScreenPosition(float screenX, float screenY, UIRenderer* uiRenderer, std::vector<UIWindow>& ) {
    if (!uiRenderer || !camera || !engine) {
        return INVALID_ENTITY;
    }

    // Get viewport rect from UI layout
    glm::vec4 viewportRect = uiRenderer->getLayout().getViewportRect();

    // Get framebuffer height to convert viewport Y from top-origin to bottom-origin
    int fbWidth, fbHeight;
    window->getFramebufferSize(&fbWidth, &fbHeight);

    // viewportRect uses top-left origin (Y=0 at top)
    // screenY uses bottom-left origin (Y=0 at bottom, from handleUIInput line 225)
    // Convert viewportRect.y from top-origin to bottom-origin
    float viewportBottomY = fbHeight - viewportRect.y - viewportRect.w;

    // Convert screen coordinates to viewport-relative coordinates
    float viewportX = screenX - viewportRect.x;
    float viewportY = screenY - viewportBottomY;

    // Check if click is within viewport
    if (viewportX < 0 || viewportX > viewportRect.z || viewportY < 0 || viewportY > viewportRect.w) {
        return INVALID_ENTITY;
    }

    // Normalize to [0, 1] where 0 = bottom, 1 = top
    float normalizedX = viewportX / viewportRect.z;
    float normalizedY = viewportY / viewportRect.w;

    // Convert to NDC [-1, 1] where -1 = bottom, +1 = top
    float ndcX = normalizedX * 2.0f - 1.0f;
    float ndcY = normalizedY * 2.0f - 1.0f;

    // Get camera matrices
    glm::mat4 projectionMatrix = camera->getProjection();
    glm::mat4 viewMatrix = camera->getView();
    glm::mat4 invProjection = glm::inverse(projectionMatrix);
    glm::mat4 invView = glm::inverse(viewMatrix);

    // Create ray in NDC space (two points: near and far plane)
    glm::vec4 rayStartNDC = glm::vec4(ndcX, ndcY, -1.0f, 1.0f);  // Near plane
    glm::vec4 rayEndNDC = glm::vec4(ndcX, ndcY, 1.0f, 1.0f);     // Far plane

    // Transform to world space
    glm::vec4 rayStartWorld = invView * invProjection * rayStartNDC;
    glm::vec4 rayEndWorld = invView * invProjection * rayEndNDC;

    // Perspective divide
    rayStartWorld /= rayStartWorld.w;
    rayEndWorld /= rayEndWorld.w;

    // Calculate ray origin and direction
    glm::vec3 rayOrigin = glm::vec3(rayStartWorld);
    glm::vec3 rayEnd = glm::vec3(rayEndWorld);
    glm::vec3 rayDirection = glm::normalize(rayEnd - rayOrigin);

    // Ray-entity intersection test using AABB (Axis-Aligned Bounding Box)
    auto& registry = ECSRegistry::getRegistry();
    Entity closestEntity = INVALID_ENTITY;
    float closestDistance = std::numeric_limits<float>::max();

    for (Entity entity : registry.getEntities()) {
        // Skip inactive entities
        if (!registry.isActiveInHierarchy(entity)) continue;

        // Check if entity has Transform and MeshRenderer
        try {
            registry.getComponent<Transform>(entity);
            auto& meshRenderer = registry.getComponent<MeshRenderer>(entity);

            // Get world matrix
            glm::mat4 worldMatrix = Transform::getWorldMatrix(entity);

            // Transform local-space AABB to world-space AABB
            // Get 8 corners of local AABB and transform them
            glm::vec3 localMin = meshRenderer.boundingBox.min;
            glm::vec3 localMax = meshRenderer.boundingBox.max;

            // Transform all 8 corners to world space
            glm::vec3 corners[8] = {
                glm::vec3(worldMatrix * glm::vec4(localMin.x, localMin.y, localMin.z, 1.0f)),
                glm::vec3(worldMatrix * glm::vec4(localMax.x, localMin.y, localMin.z, 1.0f)),
                glm::vec3(worldMatrix * glm::vec4(localMin.x, localMax.y, localMin.z, 1.0f)),
                glm::vec3(worldMatrix * glm::vec4(localMax.x, localMax.y, localMin.z, 1.0f)),
                glm::vec3(worldMatrix * glm::vec4(localMin.x, localMin.y, localMax.z, 1.0f)),
                glm::vec3(worldMatrix * glm::vec4(localMax.x, localMin.y, localMax.z, 1.0f)),
                glm::vec3(worldMatrix * glm::vec4(localMin.x, localMax.y, localMax.z, 1.0f)),
                glm::vec3(worldMatrix * glm::vec4(localMax.x, localMax.y, localMax.z, 1.0f))
            };

            // Find AABB that encompasses all transformed corners
            glm::vec3 aabbMin(std::numeric_limits<float>::max());
            glm::vec3 aabbMax(std::numeric_limits<float>::lowest());
            for (int i = 0; i < 8; ++i) {
                aabbMin = glm::min(aabbMin, corners[i]);
                aabbMax = glm::max(aabbMax, corners[i]);
            }

            // Ray-AABB intersection (slab method) - fast early rejection
            glm::vec3 invDir = 1.0f / rayDirection;
            glm::vec3 t0s = (aabbMin - rayOrigin) * invDir;
            glm::vec3 t1s = (aabbMax - rayOrigin) * invDir;

            glm::vec3 tsmaller = glm::min(t0s, t1s);
            glm::vec3 tbigger = glm::max(t0s, t1s);

            float tmin = glm::max(glm::max(tsmaller.x, tsmaller.y), tsmaller.z);
            float tmax = glm::min(glm::min(tbigger.x, tbigger.y), tbigger.z);

            // Check if ray intersects AABB
            if (tmax >= tmin && tmax > 0.0f) {
                // AABB hit - now test individual triangles for precise intersection
                const auto& vertices = meshRenderer.vertices;
                const auto& indices = meshRenderer.indices;

                if (!vertices.empty() && !indices.empty()) {
                    // Test each triangle
                    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
                        // Get triangle vertices in local space
                        glm::vec3 v0 = vertices[indices[i]];
                        glm::vec3 v1 = vertices[indices[i + 1]];
                        glm::vec3 v2 = vertices[indices[i + 2]];

                        // Transform to world space
                        glm::vec3 wv0 = glm::vec3(worldMatrix * glm::vec4(v0, 1.0f));
                        glm::vec3 wv1 = glm::vec3(worldMatrix * glm::vec4(v1, 1.0f));
                        glm::vec3 wv2 = glm::vec3(worldMatrix * glm::vec4(v2, 1.0f));

                        // Möller-Trumbore ray-triangle intersection algorithm
                        glm::vec3 edge1 = wv1 - wv0;
                        glm::vec3 edge2 = wv2 - wv0;
                        glm::vec3 h = glm::cross(rayDirection, edge2);
                        float a = glm::dot(edge1, h);

                        // Check if ray is parallel to triangle
                        if (a > -0.00001f && a < 0.00001f)
                            continue;

                        float f = 1.0f / a;
                        glm::vec3 s = rayOrigin - wv0;
                        float u = f * glm::dot(s, h);

                        if (u < 0.0f || u > 1.0f)
                            continue;

                        glm::vec3 q = glm::cross(s, edge1);
                        float v = f * glm::dot(rayDirection, q);

                        if (v < 0.0f || u + v > 1.0f)
                            continue;

                        // Compute t to find intersection point
                        float t = f * glm::dot(edge2, q);

                        if (t > 0.00001f && t < closestDistance) {
                            closestDistance = t;
                            closestEntity = entity;
                        }
                    }
                }
            }
        } catch (...) {
            // Entity doesn't have required components
            continue;
        }
    }

    return closestEntity;
}

} // namespace impgine
