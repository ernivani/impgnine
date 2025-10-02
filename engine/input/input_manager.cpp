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
    // ESC to release mouse capture (always check this)
    if (window->isKeyPressed(GLFW_KEY_ESCAPE)) {
        if (mouseCaptured) {
            mouseCaptured = false;
            window->setCursorInputMode(GLFW_CURSOR_NORMAL);
        }
    }

    // Only process camera movement if mouse is captured (viewport is focused)
    if (!mouseCaptured) return;

    const float moveSpeed = 5.0f; // units per second

    // WASD movement
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

    // Space and Shift for up/down movement
    if (window->isKeyPressed(GLFW_KEY_SPACE)) {
        camera->moveUp(moveSpeed * deltaTime);
    }
    if (window->isKeyPressed(GLFW_KEY_LEFT_SHIFT)) {
        camera->moveDown(moveSpeed * deltaTime);
    }
}

void InputManager::handleMouseMovement() {
    if (!mouseCaptured) return;

    double xpos, ypos;
    window->getCursorPos(&xpos, &ypos);

    if (firstMouse) {
        lastMouseX = xpos;
        lastMouseY = ypos;
        firstMouse = false;
    }

    double xoffset = xpos - lastMouseX;
    double yoffset = ypos - lastMouseY; // Normal direction for intuitive mouse look

    lastMouseX = xpos;
    lastMouseY = ypos;

    const float sensitivity = 0.002f; // Adjust as needed
    camera->rotateYaw(static_cast<float>(xoffset * sensitivity));
    camera->rotatePitch(static_cast<float>(yoffset * sensitivity));
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

    // Handle mouse capture for 3D viewport navigation
    if (!mouseCaptured) {
        if (window->isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) {
            if (!mouseOverUI) {
                mouseCaptured = true;
                window->setCursorInputMode(GLFW_CURSOR_DISABLED);

                // Center cursor
                int fbw = 0, fbh = 0;
                window->getFramebufferSize(&fbw, &fbh);
                window->setCursorPos(fbw / 2.0, fbh / 2.0);
                firstMouse = true;
            }
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
            glfwGetCursorPos(glfwGetCurrentContext(), &xpos, &ypos);

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

} // namespace impgine
