#pragma once

#include "../backend/window.hpp"
#include "../camera.hpp"
#include "../ui/UIRenderer.hpp"
#include "../ui/UIWindow.hpp"
#include "../ui/UIComponents.hpp"
#include <vector>

namespace impgine {

class Engine;  // Forward declaration

class InputManager {
public:
    InputManager(Window* window, Camera* camera, Engine* engine = nullptr);

    void processInput(float deltaTime);
    void handleMouseMovement();
    void handleUIInput(UIRenderer* uiRenderer, std::vector<UIWindow>& uiWindows);
    void updateComponentHover(std::vector<UIWindow>& uiWindows, float mouseX, float mouseY);

    // Viewport entity picking
    Entity pickEntityAtScreenPosition(float screenX, float screenY, UIRenderer* uiRenderer, std::vector<UIWindow>& uiWindows);

    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods, UIRenderer* uiRenderer, std::vector<UIWindow>& uiWindows, Engine* engine);
    static void characterCallback(GLFWwindow* window, unsigned int codepoint, std::vector<UIWindow>& uiWindows);
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods, std::vector<UIWindow>& uiWindows);

    bool isMouseCaptured() const { return mouseCaptured; }
    void setMouseCaptured(bool captured) { mouseCaptured = captured; }

    // Scroll callback
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);

private:
    Window* window;
    Camera* camera;
    Engine* engine;

    double lastMouseX;
    double lastMouseY;
    bool firstMouse = true;
    bool mouseCaptured = false;

    // Mouse button states
    bool leftButtonPressed = false;
    bool rightButtonPressed = false;
    bool middleButtonPressed = false;
    bool altPressed = false;
    bool shiftPressed = false;

    // Camera control modes
    enum class CameraMode {
        None,
        Rotate,      // Right-click drag
        Pan,         // Middle-click drag
        Orbit,       // Alt + Left-click drag
        Zoom         // Alt + Right-click drag
    };
    CameraMode currentMode = CameraMode::None;

    // Orbit state
    glm::vec3 orbitTarget = glm::vec3(0.0f);

    // Slider dragging state
    static inline std::shared_ptr<UISlider> draggedSlider = nullptr;
};

} // namespace impgine
