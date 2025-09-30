#pragma once

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

namespace impgine {

struct ProjectSettings {
    std::string projectPath;
    std::string projectName;
    std::string version;
    std::string engineVersion;

    // Window settings
    int windowWidth = 800;
    int windowHeight = 600;
    std::string windowTitle = "Impgine";

    // Rendering settings
    int msaaSamples = 4;
    bool vsync = true;

    // Asset paths (relative to project folder)
    std::string scenesPath = "assets/scenes";
    std::string modelsPath = "assets/models";
    std::string texturesPath = "assets/textures";
    std::string shadersPath = "engine/shaders";

    // Startup
    std::string startupScene = "assets/scenes/scene.imp";

    // Helper to get full path
    std::string getFullPath(const std::string& relativePath) const {
        return projectPath + "/" + relativePath;
    }
};

class Project {
public:
    static ProjectSettings loadProject(const std::string& projectFilePath) {
        ProjectSettings settings;

        // Extract project path from file path
        size_t lastSlash = projectFilePath.find_last_of("/\\");
        if (lastSlash != std::string::npos) {
            settings.projectPath = projectFilePath.substr(0, lastSlash);
        }

        std::ifstream file(projectFilePath);
        if (!file.is_open()) {
            std::cerr << "Could not open project file: " << projectFilePath << ". Using defaults.\n";
            return settings;
        }

        std::string line;
        std::string currentSection;

        auto trim = [](std::string s) {
            size_t a = s.find_first_not_of(" \t\r\n");
            size_t b = s.find_last_not_of(" \t\r\n");
            if (a == std::string::npos) return std::string();
            return s.substr(a, b - a + 1);
        };

        while (std::getline(file, line)) {
            line = trim(line);
            if (line.empty() || line[0] == '#' || line.rfind("%", 0) == 0 || line.rfind("---", 0) == 0) {
                continue;
            }

            // Check for section headers
            if (line.find(':') == line.length() - 1) {
                currentSection = line.substr(0, line.length() - 1);
                continue;
            }

            auto sep = line.find(':');
            if (sep == std::string::npos) continue;

            std::string key = trim(line.substr(0, sep));
            std::string value = trim(line.substr(sep + 1));

            // Parse based on current section
            if (currentSection == "project") {
                if (key == "name") settings.projectName = value;
                else if (key == "version") settings.version = value;
                else if (key == "engine_version") settings.engineVersion = value;
            }
            else if (currentSection == "window") {
                if (key == "width") settings.windowWidth = std::stoi(value);
                else if (key == "height") settings.windowHeight = std::stoi(value);
                else if (key == "title") settings.windowTitle = value;
            }
            else if (currentSection == "rendering") {
                if (key == "msaa_samples") settings.msaaSamples = std::stoi(value);
                else if (key == "vsync") settings.vsync = (value == "true");
            }
            else if (currentSection == "assets") {
                if (key == "scenes_path") settings.scenesPath = value;
                else if (key == "models_path") settings.modelsPath = value;
                else if (key == "textures_path") settings.texturesPath = value;
                else if (key == "shaders_path") settings.shadersPath = value;
            }
            else if (currentSection == "startup") {
                if (key == "scene") settings.startupScene = value;
            }
        }

        std::cout << "Loaded project: " << settings.projectName << " from " << settings.projectPath << std::endl;
        return settings;
    }
};

} // namespace impgine