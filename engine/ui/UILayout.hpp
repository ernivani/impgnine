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

        // Check if a point is inside any UI window (not the viewport)
        bool isPointOverUI(float x, float y, const std::vector<UIWindow>& windows) const;

    private:
        int windowWidth;
        int windowHeight;

        // Layout constraints (panel sizes)
        float leftPanelWidth = 250.0f;
        float rightPanelWidth = 300.0f;
        float bottomPanelHeight = 200.0f;
        float topPanelHeight = 40.0f;

        // Calculated viewport rectangle (x, y, width, height)
        glm::vec4 viewportRect;

        void layoutDockedWindow(UIWindow& window);
    };

} // namespace impgine