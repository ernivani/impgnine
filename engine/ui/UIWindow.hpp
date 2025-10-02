#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <memory>

namespace impgine {

    struct UIComponent;

    enum class UIWindowType {
        SceneView,      // Central viewport with Vulkan rendering
        Hierarchy,      // Scene tree (left panel)
        Inspector,      // Object properties (right panel)
        Assets,         // Asset browser (bottom panel)
        LoadingModal,   // Loading indicator modal
        UnsavedChangesModal,  // Unsaved changes dialog
        Custom          // For extensibility
    };

    enum class DockPosition {
        Center,
        Left,
        Right,
        Top,
        Bottom,
        Floating        // Not docked
    };

    struct UIWindow {
        std::string title { "Window" };
        UIWindowType type { UIWindowType::Custom };
        DockPosition dockPosition { DockPosition::Floating };

        glm::vec2 position { 50.0f, 50.0f };    // top-left in pixels
        glm::vec2 size { 300.0f, 200.0f };
        glm::vec2 minSize { 200.0f, 150.0f };   // minimum window size

        bool isDragging { false };
        bool isResizing { false };
        bool isCollapsed { false };
        bool isVisible { true };

        glm::vec2 dragOffset { 0.0f, 0.0f };

        // UI styling
        glm::vec4 backgroundColor { 0.08f, 0.08f, 0.08f, 1.0f };
        glm::vec4 titleBarColor { 0.12f, 0.12f, 0.12f, 1.0f };
        glm::vec4 borderColor { 0.08f, 0.08f, 0.08f, 1.0f };
        float titleBarHeight { 30.0f };
        float borderWidth { 0.0f };

        // Helper methods
        bool isPointInTitleBar(const glm::vec2& point) const {
            return point.x >= position.x && point.x <= position.x + size.x &&
                   point.y >= position.y && point.y <= position.y + titleBarHeight;
        }

        bool isPointInWindow(const glm::vec2& point) const {
            return point.x >= position.x && point.x <= position.x + size.x &&
                   point.y >= position.y && point.y <= position.y + size.y;
        }

        glm::vec2 getContentPosition() const {
            // Content area is below the title bar (title bar is at top, content at bottom)
            return glm::vec2(position.x + borderWidth, position.y + borderWidth);
        }

        glm::vec2 getContentSize() const {
            return glm::vec2(size.x - 2.0f * borderWidth, size.y - titleBarHeight - 2.0f * borderWidth);
        }

        // UI components
        std::vector<std::shared_ptr<UIComponent>> components;

        void addComponent(std::shared_ptr<UIComponent> component) {
            components.push_back(component);
        }

        template<typename T>
        std::shared_ptr<T> findComponent(const glm::vec2& localPoint);
    };

} // namespace impgine



