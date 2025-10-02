#include "shader_compiler.hpp"
#include <iostream>
#include <cstdlib>
#include <stdexcept>

namespace impgine {

bool ShaderCompiler::compileShader(const std::string& sourcePath, const std::string& outputPath, const std::string& shaderType) {
    std::string command = "glslc " + sourcePath + " -o " + outputPath;
    std::cout << "Compiling " << shaderType << " shader: " << sourcePath << std::endl;
    int result = std::system(command.c_str());
    if (result != 0) {
        std::cerr << "Failed to compile shader: " << sourcePath << std::endl;
        return false;
    }
    std::cout << "Successfully compiled " << shaderType << " shader to " << outputPath << std::endl;
    return true;
}

void ShaderCompiler::compileAllShaders(const ProjectSettings& project) {
    std::cout << "Compiling shaders..." << std::endl;
    std::string shaderPath = project.getShadersPath();
    std::string vertSource = shaderPath + "/shader.vert";
    std::string fragSource = shaderPath + "/shader.frag";
    std::string vertOutput = shaderPath + "/vert.spv";
    std::string fragOutput = shaderPath + "/frag.spv";
    // UI shaders
    std::string uiVertSrc = shaderPath + "/ui.vert";
    std::string uiFragSrc = shaderPath + "/ui.frag";
    std::string uiVertOut = shaderPath + "/ui.vert.spv";
    std::string uiFragOut = shaderPath + "/ui.frag.spv";

    bool vertSuccess = compileShader(vertSource, vertOutput, "vertex");
    bool fragSuccess = compileShader(fragSource, fragOutput, "fragment");
    bool uiVertSuccess = compileShader(uiVertSrc, uiVertOut, "vertex");
    bool uiFragSuccess = compileShader(uiFragSrc, uiFragOut, "fragment");

    if (!vertSuccess || !fragSuccess || !uiVertSuccess || !uiFragSuccess) {
        throw std::runtime_error("Failed to compile shaders");
    }
    std::cout << "All shaders compiled successfully!" << std::endl;
}

} // namespace impgine
