#pragma once

#include <vector>
#include <unordered_map>
#include <algorithm>
#include <typeindex>
#include <typeinfo>
#include <memory>
#include <unordered_set>
#include <cstdint>
#include <limits>
#include <stdexcept>

using Entity = uint32_t;
const Entity INVALID_ENTITY = std::numeric_limits<Entity>::max();

namespace impgine {

    class ECSRegistry 
    {
        public:


        static ECSRegistry& getRegistry();
        Entity createEntity();
        void ensureAlive(Entity entity);
        void destroyEntity(Entity entity);
        template<typename T>
        void addComponent(Entity entity, T component);
        template<typename T>
        void removeComponent(Entity entity, T component);
        template<typename T>
        T& getComponent(Entity entity);

        std::vector<Entity> getEntities();
        const std::unordered_map<std::type_index, void*>& getComponents(Entity entity) const;

        private:
        Entity nextEntityId = 0;
        std::unordered_set<Entity> alive;
        bool isAlive(Entity entity) const;
        std::vector<Entity> entities;
        std::unordered_map<Entity, std::unordered_map<std::type_index, void*>> components;
    };

    template<typename T>
    inline void ECSRegistry::addComponent(Entity entity, T component) {
        ensureAlive(entity);
        auto & byType = components[entity];
        std::type_index typeIndex = std::type_index(typeid(T));
        auto it = byType.find(typeIndex);
        if (it != byType.end()) {
            delete static_cast<T*>(it->second);
        }
        byType[typeIndex] = new T(std::move(component));
    }

    template<typename T>
    inline void ECSRegistry::removeComponent(Entity entity, T ) {
        auto entityIt = components.find(entity);
        if (entityIt == components.end()) return;
        auto & byType = entityIt->second;
        std::type_index typeIndex = std::type_index(typeid(T));
        auto it = byType.find(typeIndex);
        if (it == byType.end()) return;
        delete static_cast<T*>(it->second);
        byType.erase(it);
    }

    template<typename T>
    inline T& ECSRegistry::getComponent(Entity entity) {
        auto entityIt = components.find(entity);
        if (entityIt == components.end()) {
            throw std::runtime_error("Entity does not exist or has no components");
        }
        auto & byType = entityIt->second;
        std::type_index typeIndex = std::type_index(typeid(T));
        auto it = byType.find(typeIndex);
        if (it == byType.end()) {
            throw std::runtime_error("Requested component not found on entity");
        }
        return *static_cast<T*>(it->second);
    }

}
