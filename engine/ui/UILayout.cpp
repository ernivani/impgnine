#include "UILayout.hpp"
#include <iostream>

namespace impgine {

    UILayout::UILayout(int windowWidth, int windowHeight)
        : windowWidth(windowWidth), windowHeight(windowHeight) {
        // Initialize with empty viewport
        viewportRect = glm::vec4(0.0f, 0.0f, static_cast<float>(windowWidth), static_cast<float>(windowHeight));
    }

    void UILayout::setWindowSize(int width, int height) {
        windowWidth = width;
        windowHeight = height;
    }

    void UILayout::computeLayout(std::vector<UIWindow>& windows) {
        /*
         * Coordinate system: (0,0) at top-left, (width, height) at bottom-right
         *
         * Layout structure:
         * +----------------------------------+
         * |  LEFT   |   VIEWPORT   |  RIGHT |
         * |  PANEL  |   (CENTER)   |  PANEL |
         * |         |              |        |
         * |         +--------------+        |
         * |         |    BOTTOM    |        |
         * +----------------------------------+
         */

        // Initialize viewport to full window
        float vpLeft = 0.0f;
        float vpTop = 0.0f;
        float vpRight = static_cast<float>(windowWidth);
        float vpBottom = static_cast<float>(windowHeight);

        // Check which panels exist
        bool hasLeft = false, hasRight = false, hasBottom = false, hasTop = false;
        for (const auto& win : windows) {
            if (!win.isVisible) continue;
            if (win.dockPosition == DockPosition::Left) hasLeft = true;
            if (win.dockPosition == DockPosition::Right) hasRight = true;
            if (win.dockPosition == DockPosition::Bottom) hasBottom = true;
            if (win.dockPosition == DockPosition::Top) hasTop = true;
        }

        // Carve out space for panels from viewport
        if (hasLeft) vpLeft += leftPanelWidth;
        if (hasRight) vpRight -= rightPanelWidth;
        if (hasTop) vpTop += topPanelHeight;
        if (hasBottom) vpBottom -= bottomPanelHeight;

        // Store viewport rectangle
        viewportRect.x = vpLeft;
        viewportRect.y = vpTop;
        viewportRect.z = vpRight - vpLeft;   // width
        viewportRect.w = vpBottom - vpTop;   // height

        // Position all windows
        for (auto& win : windows) {
            if (!win.isVisible) continue;
            layoutDockedWindow(win);
        }
    }

    void UILayout::layoutDockedWindow(UIWindow& window) {
        float winWidth = static_cast<float>(windowWidth);
        float winHeight = static_cast<float>(windowHeight);

        switch (window.dockPosition) {
            case DockPosition::Left:
                // Left side, full height
                window.position = glm::vec2(0.0f, 0.0f);
                window.size = glm::vec2(leftPanelWidth, winHeight);
                break;

            case DockPosition::Right:
                // Right side, full height
                window.position = glm::vec2(winWidth - rightPanelWidth, 0.0f);
                window.size = glm::vec2(rightPanelWidth, winHeight);
                break;

            case DockPosition::Top:
                // Top, full width
                window.position = glm::vec2(0.0f, 0.0f);
                window.size = glm::vec2(winWidth, topPanelHeight);
                break;

            case DockPosition::Bottom:
                // Bottom, between left and right panels
                window.position = glm::vec2(viewportRect.x, 0.0f);
                window.size = glm::vec2(viewportRect.z, bottomPanelHeight);
                break;

            case DockPosition::Center:
                // Center viewport area
                window.position = glm::vec2(viewportRect.x, viewportRect.y);
                window.size = glm::vec2(viewportRect.z, viewportRect.w);
                break;

            case DockPosition::Floating:
                // Don't modify floating windows
                break;
        }
    }

    bool UILayout::isPointOverUI(float x, float y, const std::vector<UIWindow>& windows) const {
        // Check if point is over any visible UI window (excluding center viewport)
        for (const auto& win : windows) {
            if (!win.isVisible) continue;
            if (win.type == UIWindowType::SceneView) continue; // Skip viewport

            // For bottom panel, the render position is at Y=0 but hit testing should be at bottom
            float hitY = win.position.y;
            if (win.dockPosition == DockPosition::Bottom) {
                hitY = static_cast<float>(windowHeight) - win.size.y;
            }

            bool inX = (x >= win.position.x) && (x <= win.position.x + win.size.x);
            bool inY = (y >= hitY) && (y <= hitY + win.size.y);

            if (inX && inY) {
                return true;
            }
        }
        return false;
    }

} // namespace impgine