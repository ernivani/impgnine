#include "input_manager.hpp"
#include <GLFW/glfw3.h>

namespace impgine {

InputManager::InputManager(Window* window, Camera* camera)
    : window(window), camera(camera), lastMouseX(400.0), lastMouseY(300.0) {
}

void InputManager::processInput(float deltaTime) {
    const float moveSpeed = 5.0f; // units per second

    // WASD movement (using ZQSD for AZERTY keyboards, but we'll use WASD)
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

    // ESC to release mouse capture
    if (window->isKeyPressed(GLFW_KEY_ESCAPE)) {
        if (mouseCaptured) {
            mouseCaptured = false;
            window->setCursorInputMode(GLFW_CURSOR_NORMAL);
        }
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
    float mouseY = static_cast<float>(my) * scaleY;

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

    // Handle UI window dragging (optional feature)
    // Currently disabled, but can be re-enabled for floating windows
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
        ypos = ypos * fbHeight / winHeight;

        if (uiRenderer) {
            uiRenderer->handleMouseClick(xpos, ypos, uiWindows);
        }
    }
}

} // namespace impgine
