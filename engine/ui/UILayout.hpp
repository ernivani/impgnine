#pragma once

#include <glm/glm.hpp>
#include <vector>
#include "UIWindow.hpp"

namespace impgine {

    class UILayout {
    public:
        UILayout(int windowWidth, int windowHeight);

        // Recalculate layout based on docked windows
        void computeLayout(std::vector<UIWindow>& windows);

        // Get the viewport area (central SceneView area)
        glm::vec4 getViewportRect() const { return viewportRect; }

        void setWindowSize(int width, int height);

    private:
        int windowWidth;
        int windowHeight;

        // Layout constraints
        float leftPanelWidth = 250.0f;
        float rightPanelWidth = 300.0f;
        float bottomPanelHeight = 200.0f;
        float topBarHeight = 0.0f;  // Optional toolbar

        // Calculated viewport rectangle (x, y, width, height)
        glm::vec4 viewportRect;

        void layoutDockedWindow(UIWindow& window);
    };

} // namespace impgine