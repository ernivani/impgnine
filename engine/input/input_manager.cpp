#include "input_manager.hpp"
#include "../ui/UIComponents.hpp"
#include <GLFW/glfw3.h>
#include <iostream>

namespace impgine {

InputManager::InputManager(Window* window, Camera* camera)
    : window(window), camera(camera), lastMouseX(400.0), lastMouseY(300.0) {
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

void InputManager::updateComponentHover(std::vector<UIWindow>& uiWindows, float mouseX, float mouseY, int fbWidth, int fbHeight) {
    static int debugCounter = 0;
    if (debugCounter++ % 60 == 0) { // Print every 60 frames
        std::cout << "Mouse: (" << mouseX << ", " << mouseY << ") | FB: " << fbWidth << "x" << fbHeight << std::endl;
    }

    // Reset all component states to normal first
    for (auto& window : uiWindows) {
        for (auto& comp : window.components) {
            if (comp->state != UIComponentState::Focused) {
                comp->state = UIComponentState::Normal;
            }
        }
    }

    // Check which component is being hovered
    for (auto& window : uiWindows) {
        if (!window.isVisible) continue;

        glm::vec2 contentPos = window.getContentPosition();

        for (auto& comp : window.components) {
            if (!comp->isEnabled || !comp->isVisible) continue;

            glm::vec2 absolutePos = contentPos + comp->position;

            if (mouseX >= absolutePos.x && mouseX <= absolutePos.x + comp->size.x &&
                mouseY >= absolutePos.y && mouseY <= absolutePos.y + comp->size.y) {

                if (comp->state != UIComponentState::Focused) {
                    comp->state = UIComponentState::Hovered;
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

            for (auto& comp : uiWindow.components) {
                if (comp == draggedSlider) {
                    glm::vec2 absolutePos = contentPos + comp->position;
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
    updateComponentHover(uiWindows, mouseX, mouseY, fbWidth, fbHeight);

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
    (void)window;
    (void)scancode;
    (void)mods;

    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;

    // Find focused input field
    for (auto& uiWindow : uiWindows) {
        if (!uiWindow.isVisible) continue;

        for (auto& comp : uiWindow.components) {
            if (comp->type == UIComponentType::InputField && comp->state == UIComponentState::Focused) {
                auto input = std::dynamic_pointer_cast<UIInputField>(comp);
                if (input) {
                    if (key == GLFW_KEY_BACKSPACE) {
                        input->deleteChar();
                    } else if (key == GLFW_KEY_LEFT) {
                        input->moveCursorLeft();
                    } else if (key == GLFW_KEY_RIGHT) {
                        input->moveCursorRight();
                    } else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_ESCAPE) {
                        // Unfocus on Enter/Escape
                        comp->state = UIComponentState::Normal;
                    }
                }
                return;
            }
        }
    }
}

void InputManager::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods, UIRenderer* uiRenderer, std::vector<UIWindow>& uiWindows) {
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

            for (auto& comp : uiWindow.components) {
                if (!comp->isEnabled || !comp->isVisible) continue;

                glm::vec2 absolutePos = contentPos + comp->position;

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
                                // Unfocus all other inputs
                                for (auto& w : uiWindows) {
                                    for (auto& c : w.components) {
                                        if (c->type == UIComponentType::InputField && c != comp) {
                                            c->state = UIComponentState::Normal;
                                        }
                                    }
                                }
                                comp->state = UIComponentState::Focused;
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
                        default:
                            break;
                    }

                    return; // Only click one component
                }
            }
        }

        // If not clicking a component, handle other UI clicks (like hierarchy selection)
        if (!clickedComponent && uiRenderer) {
            uiRenderer->handleMouseClick(xpos, ypos, uiWindows);
        }
    }

    // Handle slider drag release
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
        if (draggedSlider) {
            draggedSlider->state = UIComponentState::Normal;
            draggedSlider = nullptr;
        }
    }
}

} // namespace impgine
