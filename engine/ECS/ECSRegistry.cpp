#include "ECSRegistry.hpp"

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

}