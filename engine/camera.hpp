#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace impgine {

    class Camera {
        public: void setOrthographicProjection(float left, float right, float top, float bottom, float near,
            float far);
        void setPerspectiveProjection(float fovy, float aspect, float near, float far);

        void setViewDirection(glm::vec3 position, glm::vec3 direction,
            glm::vec3 up = glm::vec3 {
                0.0f, 0.0f, 1.0f
            });
        void setViewTarget(glm::vec3 position, glm::vec3 target,
            glm::vec3 up = glm::vec3 {
                0.0f, 0.0f, 1.0f
            });
        void setViewYXZ(glm::vec3 position, glm::vec3 rotation);

        // Movement and rotation methods
        void moveForward(float distance);
        void moveBackward(float distance);
        void moveLeft(float distance);
        void moveRight(float distance);
        void moveUp(float distance);
        void moveDown(float distance);
        
        void rotateYaw(float angle);   // Look left/right
        void rotatePitch(float angle); // Look up/down
        
        // Update view matrix after movement/rotation
        void updateViewMatrix();

        const glm::mat4 & getProjection() const {
            return projectionMatrix;
        }
        const glm::mat4 & getView() const {
            return viewMatrix;
        }
        const glm::mat4 & getInverseView() const {
            return inverseViewMatrix;
        }
        glm::vec3 getPosition() const {
            return position;
        }
        glm::vec3 getRotation() const {
            return rotation;
        }

        private: glm::mat4 projectionMatrix {
            1.0f
        };
        glm::mat4 viewMatrix {
            1.0f
        };
        glm::mat4 inverseViewMatrix {
            1.0f
        };
        
        // Camera state
        glm::vec3 position{3.0f, 1.5f, 3.0f};  // Initial position
        glm::vec3 rotation{0.0f, -2.356f, 0.0f}; // Pitch, Yaw, Roll (yaw = -135° to look at model)
    };

} // namespace impgine