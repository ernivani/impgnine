#pragma once

#include <string>
#include "../project.hpp"
#include "../ECS/ECSRegistry.hpp"
#include "../camera.hpp"

namespace impgine {

class Engine; // Forward declaration

class SceneLoader {
public:
    static void loadScene(const std::string& scenePath, const ProjectSettings& project);
    static bool loadCameraFromScene(const std::string& scenePath, glm::vec3& outPos, glm::vec3& outRot);
    static bool saveScene(const std::string& scenePath, const ProjectSettings& project, const Camera& camera, ECSRegistry& registry);
};

} // namespace impgine
