#include "ECSRegistry.hpp"
#include "components.hpp"

namespace impgine {

    ECSRegistry& ECSRegistry::getRegistry() {
        static ECSRegistry registry;
        return registry;
    }

    Entity ECSRegistry::createEntity() {
        Entity e = nextEntityId++;
        entities.push_back(e);
        alive.insert(e);
        return e;
    }

    void ECSRegistry::ensureAlive(Entity entity) {
        if (!isAlive(entity)) {
            entities.push_back(entity);
            alive.insert(entity);
        }
    }

    void ECSRegistry::destroyEntity(Entity entity) {
        if (isAlive(entity)) {
            entities.erase(std::find(entities.begin(), entities.end(), entity));
            alive.erase(entity);
        }
    }

    bool ECSRegistry::isAlive(Entity entity) const {
        return alive.find(entity) != alive.end();
    }

    std::vector<Entity> ECSRegistry::getEntities() {
        return entities;
    }

    const std::unordered_map<std::type_index, void*>& ECSRegistry::getComponents(Entity entity) const {
        static const std::unordered_map<std::type_index, void*> empty;
        auto it = components.find(entity);
        if (it == components.end()) {
            return empty;
        }
        return it->second;
    }

    void ECSRegistry::setParent(Entity child, Entity parent) {
        ensureAlive(child);
        ensureAlive(parent);

        // Remove from old parent if exists
        removeParent(child);

        // Get or create hierarchy component for child
        Hierarchy* childHierarchy = nullptr;
        try {
            childHierarchy = &getComponent<Hierarchy>(child);
        } catch (...) {
            addComponent<Hierarchy>(child, Hierarchy{});
            childHierarchy = &getComponent<Hierarchy>(child);
        }

        // Get or create hierarchy component for parent
        Hierarchy* parentHierarchy = nullptr;
        try {
            parentHierarchy = &getComponent<Hierarchy>(parent);
        } catch (...) {
            addComponent<Hierarchy>(parent, Hierarchy{});
            parentHierarchy = &getComponent<Hierarchy>(parent);
        }

        // Set the parent-child relationship
        childHierarchy->parent = parent;
        parentHierarchy->children.push_back(child);

        // Update active state in hierarchy
        try {
            auto& childActive = getComponent<Active>(child);
            childActive.isActive = childActive.isActiveSelf && isActiveInHierarchy(parent);
        } catch (...) {}
    }

    void ECSRegistry::removeParent(Entity child) {
        try {
            auto& hierarchy = getComponent<Hierarchy>(child);
            if (hierarchy.parent != INVALID_ENTITY) {
                Entity oldParent = hierarchy.parent;

                // Remove from parent's children list
                try {
                    auto& parentHierarchy = getComponent<Hierarchy>(oldParent);
                    auto it = std::find(parentHierarchy.children.begin(),
                                      parentHierarchy.children.end(), child);
                    if (it != parentHierarchy.children.end()) {
                        parentHierarchy.children.erase(it);
                    }
                } catch (...) {}

                hierarchy.parent = INVALID_ENTITY;

                // Update active state
                try {
                    auto& active = getComponent<Active>(child);
                    active.isActive = active.isActiveSelf;
                } catch (...) {}
            }
        } catch (...) {}
    }

    std::vector<Entity> ECSRegistry::getChildren(Entity parent) {
        try {
            const auto& hierarchy = getComponent<Hierarchy>(parent);
            return hierarchy.children;
        } catch (...) {
            return {};
        }
    }

    Entity ECSRegistry::getParent(Entity child) {
        try {
            const auto& hierarchy = getComponent<Hierarchy>(child);
            return hierarchy.parent;
        } catch (...) {
            return INVALID_ENTITY;
        }
    }

    void ECSRegistry::setActive(Entity entity, bool active) {
        Active* activeComp = nullptr;
        try {
            activeComp = &getComponent<Active>(entity);
        } catch (...) {
            addComponent<Active>(entity, Active{});
            activeComp = &getComponent<Active>(entity);
        }

        activeComp->isActiveSelf = active;

        // Calculate actual active state based on parent
        bool parentActive = true;
        try {
            const auto& hierarchy = getComponent<Hierarchy>(entity);
            if (hierarchy.parent != INVALID_ENTITY) {
                parentActive = isActiveInHierarchy(hierarchy.parent);
            }
        } catch (...) {}

        activeComp->isActive = activeComp->isActiveSelf && parentActive;

        // Propagate to children
        for (Entity child : getChildren(entity)) {
            try {
                auto& childActive = getComponent<Active>(child);
                childActive.isActive = childActive.isActiveSelf && activeComp->isActive;

                // Recursively update children if this was deactivation
                if (!activeComp->isActive) {
                    setActive(child, childActive.isActiveSelf);
                }
            } catch (...) {}
        }
    }

    bool ECSRegistry::isActiveInHierarchy(Entity entity) {
        try {
            const auto& active = getComponent<Active>(entity);
            return active.isActive;
        } catch (...) {
            // No active component means active by default
            return true;
        }
    }

}