#pragma once

#include <string>
#include "../project.hpp"
#include "../ECS/ECSRegistry.hpp"

namespace impgine {

class Engine; // Forward declaration

class SceneLoader {
public:
    static void loadScene(const std::string& scenePath, const ProjectSettings& project);
};

} // namespace impgine
