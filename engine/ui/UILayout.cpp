#include "UILayout.hpp"

namespace impgine {

    UILayout::UILayout(int windowWidth, int windowHeight)
        : windowWidth(windowWidth), windowHeight(windowHeight) {
        std::vector<UIWindow> empty;
        computeLayout(empty);
    }

    void UILayout::setWindowSize(int width, int height) {
        windowWidth = width;
        windowHeight = height;
    }

    void UILayout::computeLayout(std::vector<UIWindow>& windows) {
        // Calculate available space for each docked position
        float leftX = 0.0f;
        float rightX = static_cast<float>(windowWidth);
        float topY = topBarHeight;
        float bottomY = static_cast<float>(windowHeight);

        // First pass: identify which dock positions are occupied
        bool hasLeft = false, hasRight = false, hasBottom = false, hasTop = false;
        for (const auto& window : windows) {
            if (!window.isVisible || window.dockPosition == DockPosition::Floating) continue;

            switch (window.dockPosition) {
                case DockPosition::Left: hasLeft = true; break;
                case DockPosition::Right: hasRight = true; break;
                case DockPosition::Bottom: hasBottom = true; break;
                case DockPosition::Top: hasTop = true; break;
                default: break;
            }
        }

        // Adjust boundaries based on docked panels
        if (hasLeft) leftX += leftPanelWidth;
        if (hasRight) rightX -= rightPanelWidth;
        if (hasBottom) bottomY -= bottomPanelHeight;
        if (hasTop) topY += topBarHeight;

        // Calculate center viewport rectangle
        viewportRect = glm::vec4(leftX, topY, rightX - leftX, bottomY - topY);

        // Second pass: position all docked windows
        for (auto& window : windows) {
            if (!window.isVisible || window.dockPosition == DockPosition::Floating) continue;
            layoutDockedWindow(window);
        }
    }

    void UILayout::layoutDockedWindow(UIWindow& window) {
        switch (window.dockPosition) {
            case DockPosition::Left:
                // Left panel: full height on the left side
                window.position = glm::vec2(0.0f, topBarHeight);
                window.size = glm::vec2(leftPanelWidth, static_cast<float>(windowHeight) - topBarHeight);
                break;

            case DockPosition::Right:
                // Right panel: full height on the right side
                window.position = glm::vec2(static_cast<float>(windowWidth) - rightPanelWidth, topBarHeight);
                window.size = glm::vec2(rightPanelWidth, static_cast<float>(windowHeight) - topBarHeight);
                break;

            case DockPosition::Bottom: {
                // Bottom panel: spans between left and right panels, at the bottom
                float leftMargin = leftPanelWidth;
                float rightMargin = rightPanelWidth;
                float availableWidth = static_cast<float>(windowWidth) - leftMargin - rightMargin;

                window.position = glm::vec2(leftMargin, topBarHeight);
                window.size = glm::vec2(availableWidth, bottomPanelHeight);
                break;
            }

            case DockPosition::Top:
                window.position = glm::vec2(0.0f, 0.0f);
                window.size = glm::vec2(static_cast<float>(windowWidth), topBarHeight);
                break;

            case DockPosition::Center:
                // Center viewport: between all the panels
                window.position = glm::vec2(viewportRect.x, viewportRect.y);
                window.size = glm::vec2(viewportRect.z, viewportRect.w);
                break;

            default:
                break;
        }
    }

} // namespace impgine