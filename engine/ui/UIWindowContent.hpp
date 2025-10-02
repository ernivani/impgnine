#pragma once

#include <string>
#include <vector>
#include <memory>
#include "../ECS/ECSRegistry.hpp"

namespace impgine {

    // Base class for window-specific content
    class UIWindowContent {
    public:
        virtual ~UIWindowContent() = default;
        virtual void render() = 0;  // Override to render content
    };

    // Hierarchy window - displays scene tree
    class HierarchyContent : public UIWindowContent {
    public:
        HierarchyContent(ECSRegistry* registry) : registry(registry) {}

        void render() override {
            // TODO: Render entity tree
            // For now, just a placeholder
        }

        void setSelectedEntity(EntityID entity) {
            selectedEntity = entity;
        }

        EntityID getSelectedEntity() const {
            return selectedEntity;
        }

    private:
        ECSRegistry* registry;
        EntityID selectedEntity = NULL_ENTITY;
    };

    // Inspector window - displays properties of selected object
    class InspectorContent : public UIWindowContent {
    public:
        InspectorContent(ECSRegistry* registry) : registry(registry) {}

        void render() override {
            // TODO: Render component properties
            // For now, just a placeholder
        }

        void setTarget(EntityID entity) {
            targetEntity = entity;
        }

    private:
        ECSRegistry* registry;
        EntityID targetEntity = NULL_ENTITY;
    };

    // Assets window - displays project assets
    class AssetsContent : public UIWindowContent {
    public:
        AssetsContent(const std::string& assetsPath) : assetsPath(assetsPath) {}

        void render() override {
            // TODO: Render asset browser
            // For now, just a placeholder
        }

        void setCurrentPath(const std::string& path) {
            currentPath = path;
        }

    private:
        std::string assetsPath;
        std::string currentPath;
    };

    // Scene view window - contains the 3D viewport
    class SceneViewContent : public UIWindowContent {
    public:
        SceneViewContent() = default;

        void render() override {
            // The actual 3D rendering happens in the main render pass
            // This is just for UI overlay on the viewport
        }

        void setViewportRect(float x, float y, float width, float height) {
            viewportX = x;
            viewportY = y;
            viewportWidth = width;
            viewportHeight = height;
        }

        float getViewportX() const { return viewportX; }
        float getViewportY() const { return viewportY; }
        float getViewportWidth() const { return viewportWidth; }
        float getViewportHeight() const { return viewportHeight; }

    private:
        float viewportX = 0.0f;
        float viewportY = 0.0f;
        float viewportWidth = 800.0f;
        float viewportHeight = 600.0f;
    };

} // namespace impgine