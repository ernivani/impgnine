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

        // Compute responsive panel sizes from ratios
        currentLeftWidth = hasLeft ? static_cast<float>(windowWidth) * leftPanelRatio : 0.0f;
        currentRightWidth = hasRight ? static_cast<float>(windowWidth) * rightPanelRatio : 0.0f;
        currentBottomHeight = hasBottom ? static_cast<float>(windowHeight) * bottomPanelRatio : 0.0f;

        // Carve out space for panels from viewport
        if (hasLeft) vpLeft += currentLeftWidth;
        if (hasRight) vpRight -= currentRightWidth;
        if (hasTop) vpTop += topPanelHeight;
        if (hasBottom) vpBottom -= currentBottomHeight;

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
                window.size = glm::vec2(currentLeftWidth, winHeight);
                break;

            case DockPosition::Right:
                // Right side, full height
                window.position = glm::vec2(winWidth - currentRightWidth, 0.0f);
                window.size = glm::vec2(currentRightWidth, winHeight);
                break;

            case DockPosition::Top:
                // Top, full width
                window.position = glm::vec2(0.0f, 0.0f);
                window.size = glm::vec2(winWidth, topPanelHeight);
                break;

            case DockPosition::Bottom:
                // Bottom, between left and right panels
                // Position at the bottom, directly below the viewport
                window.position = glm::vec2(
                    viewportRect.x,
                    static_cast<float>(windowHeight) - currentBottomHeight
                );
                window.size = glm::vec2(viewportRect.z, currentBottomHeight);
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

            bool inX = (x >= win.position.x) && (x <= win.position.x + win.size.x);
            bool inY = (y >= win.position.y) && (y <= win.position.y + win.size.y);

            if (inX && inY) {
                return true;
            }
        }
        return false;
    }

} // namespace impgine