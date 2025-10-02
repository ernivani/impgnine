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

        // Layout constraints
        // Ratios are relative to window width/height so panels are responsive
        float leftPanelRatio = 0.18f;   // 18% of window width
        float rightPanelRatio = 0.19f;  // 19% of window width
        float bottomPanelRatio = 0.17f; // 17% of window height
        float topPanelHeight = 40.0f;   // Keep top as fixed height (optional)

        // Calculated viewport rectangle (x, y, width, height)
        glm::vec4 viewportRect;

        // Bottom panel Y position (computed during layout)
        float bottomPanelY = 0.0f;

        // Cached computed sizes for this layout pass
        float currentLeftWidth = 0.0f;
        float currentRightWidth = 0.0f;
        float currentBottomHeight = 0.0f;

        void layoutDockedWindow(UIWindow& window);
    };

} // namespace impgine