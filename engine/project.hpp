#pragma once

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

namespace impgine {

struct ProjectSettings {
    std::string projectPath;
    std::string projectName;

    // Window settings
    int windowWidth = 800;
    int windowHeight = 600;
    std::string windowTitle = "Impgine";

    // Rendering settings
    int msaaSamples = 4;
    bool vsync = true;

    // Last opened scene
    std::string lastScene = "Assets/Scenes/scene.imp";

    // Helper to get full path
    std::string getFullPath(const std::string& relativePath) const {
        return projectPath + "/" + relativePath;
    }

    // Convert absolute paths back to project-relative if possible
    std::string toRelativePath(const std::string& absolutePath) const {
        // Normalize simple case: if it starts with projectPath + "/", strip it
        if (absolutePath.rfind(projectPath + "/", 0) == 0) {
            return absolutePath.substr(projectPath.size() + 1);
        }
        return absolutePath; // Already relative or external
    }

    // Engine paths (standardized by engine)
    std::string getEnginePath() const { return projectPath + "/Engine"; }
    std::string getShadersPath() const { return projectPath + "/Engine/Shaders"; }
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

        // Extract project name from folder name
        size_t secondLastSlash = settings.projectPath.find_last_of("/\\");
        if (secondLastSlash != std::string::npos) {
            settings.projectName = settings.projectPath.substr(secondLastSlash + 1);
        } else {
            settings.projectName = settings.projectPath;
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
            if (currentSection == "window") {
                if (key == "width") settings.windowWidth = std::stoi(value);
                else if (key == "height") settings.windowHeight = std::stoi(value);
                else if (key == "title") settings.windowTitle = value;
            }
            else if (currentSection == "rendering") {
                if (key == "msaa_samples") settings.msaaSamples = std::stoi(value);
                else if (key == "vsync") settings.vsync = (value == "true");
            }
            else if (currentSection == "editor") {
                if (key == "last_scene") settings.lastScene = value;
            }
        }

        std::cout << "Loaded project: " << settings.projectName << " from " << settings.projectPath << std::endl;
        return settings;
    }
};

} // namespace impgine