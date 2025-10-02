#include "camera.hpp"

#include <cassert>
#include <limits>

namespace impgine {

    void Camera::setOrthographicProjection(float left, float right, float top, float bottom, float near,
        float far) {
        projectionMatrix = glm::mat4 {
            1.0f
        };
        projectionMatrix[0][0] = 2.0f / (right - left);
        projectionMatrix[1][1] = 2.0f / (bottom - top);
        projectionMatrix[2][2] = 1.0f / (far - near);
        projectionMatrix[3][0] = -(right + left) / (right - left);
        projectionMatrix[3][1] = -(bottom + top) / (bottom - top);
        projectionMatrix[3][2] = -near / (far - near);
    }

    void Camera::setPerspectiveProjection(float fovy, float aspect, float near, float far) {
        // Use GLM's perspective directly and apply Vulkan Y-flip
        projectionMatrix = glm::perspective(fovy, aspect, near, far);
        projectionMatrix[1][1] *= -1;  // Vulkan Y-flip
    }

    void Camera::setViewDirection(glm::vec3 position, glm::vec3 direction, glm::vec3 up) {
        // Use GLM's lookAt directly - much simpler and guaranteed to work
        glm::vec3 target = position + direction;
        viewMatrix = glm::lookAt(position, target, up);
        inverseViewMatrix = glm::inverse(viewMatrix);
    }

    void Camera::setViewTarget(glm::vec3 position, glm::vec3 target, glm::vec3 up) {
        // Use GLM's lookAt directly - this is exactly what we want
        viewMatrix = glm::lookAt(position, target, up);
        inverseViewMatrix = glm::inverse(viewMatrix);
    }

    void Camera::setViewYXZ(glm::vec3 newPosition, glm::vec3 newRotation) {
        position = newPosition;
        rotation = newRotation;
        updateViewMatrix();
    }

    void Camera::moveForward(float distance) {
        // Calculate forward direction from yaw rotation
        glm::vec3 forward;
        forward.x = sin(rotation.y) * cos(rotation.x);
        forward.y = cos(rotation.y) * cos(rotation.x);
        forward.z = -sin(rotation.x);
        position += forward * distance;
        if (onModified) onModified();
    }

    void Camera::moveBackward(float distance) {
        moveForward(-distance);
    }

    void Camera::moveLeft(float distance) {
        // Calculate right direction and move opposite
        glm::vec3 forward;
        forward.x = sin(rotation.y) * cos(rotation.x);
        forward.y = cos(rotation.y) * cos(rotation.x);
        forward.z = -sin(rotation.x);

        glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 0.0f, 1.0f)));
        position -= right * distance;
        if (onModified) onModified();
    }

    void Camera::moveRight(float distance) {
        moveLeft(-distance);
    }

    void Camera::moveUp(float distance) {
        position.z += distance;  // Z is up in our coordinate system
        if (onModified) onModified();
    }

    void Camera::moveDown(float distance) {
        position.z -= distance;
        if (onModified) onModified();
    }

    void Camera::rotateYaw(float angle) {
        rotation.y += angle;
        if (onModified) onModified();
    }

    void Camera::rotatePitch(float angle) {
        rotation.x += angle;
        // Clamp pitch to prevent flipping
        const float maxPitch = glm::radians(89.0f);
        rotation.x = glm::clamp(rotation.x, -maxPitch, maxPitch);
        if (onModified) onModified();
    }

    void Camera::panCamera(float deltaX, float deltaY) {
        // Calculate right and up vectors based on current view
        glm::vec3 forward;
        forward.x = sin(rotation.y) * cos(rotation.x);
        forward.y = cos(rotation.y) * cos(rotation.x);
        forward.z = -sin(rotation.x);

        glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 0.0f, 1.0f)));
        glm::vec3 up = glm::normalize(glm::cross(right, forward));

        // Pan the camera based on screen space movement
        const float panSpeed = 0.005f;
        position -= right * deltaX * panSpeed;
        position += up * deltaY * panSpeed;

        if (onModified) onModified();
    }

    void Camera::zoomCamera(float delta) {
        // Zoom by moving forward/backward along view direction
        glm::vec3 forward;
        forward.x = sin(rotation.y) * cos(rotation.x);
        forward.y = cos(rotation.y) * cos(rotation.x);
        forward.z = -sin(rotation.x);

        const float zoomSpeed = 0.5f;
        position += forward * delta * zoomSpeed;

        if (onModified) onModified();
    }

    void Camera::orbitCamera(float deltaYaw, float deltaPitch, const glm::vec3& target) {
        // Update rotation for orbit
        rotation.y += deltaYaw;
        rotation.x += deltaPitch;

        // Clamp pitch
        const float maxPitch = glm::radians(89.0f);
        rotation.x = glm::clamp(rotation.x, -maxPitch, maxPitch);

        // Calculate new position based on rotation and distance from target
        float distance = glm::length(position - target);

        glm::vec3 forward;
        forward.x = sin(rotation.y) * cos(rotation.x);
        forward.y = cos(rotation.y) * cos(rotation.x);
        forward.z = -sin(rotation.x);

        // Position camera at the specified distance from target, looking at it
        position = target - forward * distance;

        if (onModified) onModified();
    }

    void Camera::frameTarget(const glm::vec3& target, float distance) {
        // Calculate direction from target to current camera position
        glm::vec3 direction = glm::normalize(position - target);

        // Position camera at specified distance from target
        position = target + direction * distance;

        // Calculate rotation to look at target
        glm::vec3 forward = glm::normalize(target - position);
        rotation.y = atan2(forward.x, forward.y);
        rotation.x = -asin(forward.z);

        updateViewMatrix();
        if (onModified) onModified();
    }

    void Camera::updateViewMatrix() {
        // Calculate the look direction from rotation
        glm::vec3 forward;
        forward.x = sin(rotation.y) * cos(rotation.x);
        forward.y = cos(rotation.y) * cos(rotation.x);
        forward.z = -sin(rotation.x);

        glm::vec3 target = position + forward;
        viewMatrix = glm::lookAt(position, target, glm::vec3(0.0f, 0.0f, 1.0f));
        inverseViewMatrix = glm::inverse(viewMatrix);
    }

} // namespace impgine