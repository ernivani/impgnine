#pragma once

#include <string>
#include "../project.hpp"

namespace impgine {

class ShaderCompiler {
public:
    static bool compileShader(const std::string& sourcePath, const std::string& outputPath, const std::string& shaderType);
    static void compileAllShaders(const ProjectSettings& project);
};

} // namespace impgine
