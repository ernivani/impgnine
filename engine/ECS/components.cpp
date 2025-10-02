#define GLM_ENABLE_EXPERIMENTAL
#include "components.hpp"
#include "ECSRegistry.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace impgine {

glm::mat4 Transform::getLocalMatrix() const {
    glm::mat4 matrix = glm::mat4(1.0f);

    // Translation
    matrix = glm::translate(matrix, position);

    // Rotation (Euler angles to rotation matrix)
    // Apply rotation in ZYX order (roll, pitch, yaw)
    glm::quat quat = glm::quat(glm::vec3(
        glm::radians(rotation.x),
        glm::radians(rotation.y),
        glm::radians(rotation.z)
    ));
    matrix *= glm::toMat4(quat);

    // Scale
    matrix = glm::scale(matrix, scale);

    return matrix;
}

glm::mat4 Transform::getWorldMatrix(Entity entity) {
    auto& registry = ECSRegistry::getRegistry();

    try {
        const auto& transform = registry.getComponent<Transform>(entity);
        glm::mat4 localMatrix = transform.getLocalMatrix();

        // Check if entity has a parent
        try {
            const auto& hierarchy = registry.getComponent<Hierarchy>(entity);
            if (hierarchy.parent != INVALID_ENTITY) {
                // Recursively get parent's world matrix and multiply
                glm::mat4 parentWorld = getWorldMatrix(hierarchy.parent);
                return parentWorld * localMatrix;
            }
        } catch (...) {
            // No hierarchy component, use local transform as world
        }

        return localMatrix;
    } catch (...) {
        // No transform component, return identity
        return glm::mat4(1.0f);
    }
}

} // namespace impgine
