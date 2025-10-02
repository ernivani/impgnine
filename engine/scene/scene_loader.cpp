#include "scene_loader.hpp"
#include "../ECS/components.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <optional>
#include <vector>
#include <glm/glm.hpp>

namespace impgine {

void SceneLoader::loadScene(const std::string& scenePath, const ProjectSettings& project) {
    // Minimal YAML-like scene format inspired by Unity text:
    // --- !imp!Entity &id
    // name: MyEntity
    // Transform:
    //   position: {x: 0, y: 0, z: 0}
    //   rotation: {x: 0, y: 0, z: 0}
    //   scale: {x: 1, y: 1, z: 1}
    // Model:
    //   modelPath: models/viking_room.obj
    //   texturePath: textures/viking_room.png
    //   color: {r: 1, g: 1, b: 1}

    std::ifstream file(scenePath);
    if (!file.is_open()) {
        std::cerr << "Could not open scene file: " << scenePath << ". Using defaults.\n";
    }

    struct PendingEntity {
        std::optional<std::string> tag;
        glm::vec3 position {0.0f};
        glm::vec3 rotation {0.0f};
        glm::vec3 scale {1.0f};
        std::string modelPath;
        std::string texturePath;
        glm::vec3 color {1.0f, 1.0f, 1.0f};
    };

    std::vector<PendingEntity> entitiesToCreate;
    PendingEntity current;
    bool inEntityBlock = false;
    enum class Section { None, Transform, MeshRenderer };
    Section section = Section::None;

    auto trim = [](std::string s) {
        size_t a = s.find_first_not_of(" \t\r\n");
        size_t b = s.find_last_not_of(" \t\r\n");
        if (a == std::string::npos) return std::string();
        return s.substr(a, b - a + 1);
    };

    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            line = trim(line);
            if (line.empty() || line[0] == '#') continue;
            if (line.rfind("%", 0) == 0) continue; // header like %IMP 1.0

            if (line.rfind("---", 0) == 0) {
                // Start of a new entity block if tagged as !imp!Entity
                if (line.find("!imp!Entity") != std::string::npos) {
                    if (inEntityBlock) entitiesToCreate.push_back(current);
                    inEntityBlock = true;
                    current = PendingEntity{};
                    section = Section::None;
                    continue;
                }
            }

            if (line == "Transform:") { section = Section::Transform; continue; }
            if (line == "MeshRenderer:") { section = Section::MeshRenderer; continue; }

            auto sep = line.find(':');
            if (sep == std::string::npos) continue;
            std::string key = trim(line.substr(0, sep));
            std::string value = trim(line.substr(sep + 1));

            if (key == "name") {
                if (!value.empty()) current.tag = value;
                continue;
            }

            auto parseObjVec3 = [&](const std::string& obj) -> glm::vec3 {
                // Expect: {x: a, y: b, z: c} or {r: a, g: b, b: c}
                glm::vec3 out{0.0f};
                std::string s = obj;
                if (!s.empty() && s.front() == '{' && s.back() == '}') {
                    s = s.substr(1, s.size()-2);
                }
                std::stringstream ss2(s);
                std::string part;
                while (std::getline(ss2, part, ',')) {
                    auto colon = part.find(':');
                    if (colon == std::string::npos) continue;
                    auto k = trim(part.substr(0, colon));
                    auto v = trim(part.substr(colon+1));
                    float f = std::stof(v);
                    if (k == "x" || k == "r") out.x = f;
                    else if (k == "y" || k == "g") out.y = f;
                    else if (k == "z" || k == "b") out.z = f;
                }
                return out;
            };

            if (section == Section::Transform) {
                if (key == "position") current.position = parseObjVec3(value);
                else if (key == "rotation") current.rotation = parseObjVec3(value);
                else if (key == "scale") current.scale = parseObjVec3(value);
            } else if (section == Section::MeshRenderer) {
                if (key == "modelPath") current.modelPath = value;
                else if (key == "texturePath") current.texturePath = value;
                else if (key == "color") current.color = parseObjVec3(value);
            }
        }
        // Flush last block or default
        if (inEntityBlock) {
            entitiesToCreate.push_back(current);
        } else {
            // If no explicit entity block, use accumulated defaults
            entitiesToCreate.push_back(current);
        }
    } else {
        // No scene file; create one default entity
        entitiesToCreate.push_back(PendingEntity{});
    }

    // Instantiate ECS entities and components
    auto& reg = ECSRegistry::getRegistry();
    for (const auto& pe : entitiesToCreate) {
        Entity e = reg.createEntity();
        reg.addComponent<impgine::Transform>(e, { pe.position, pe.rotation, pe.scale });
        if (pe.tag.has_value()) {
            reg.addComponent<impgine::Tag>(e, impgine::Tag{ *pe.tag });
        }

        // Resolve paths relative to project
        std::string fullModelPath = project.getFullPath(pe.modelPath);
        std::string fullTexturePath = project.getFullPath(pe.texturePath);

        reg.addComponent<impgine::MeshRenderer>(e, impgine::MeshRenderer{ std::make_shared<impgine::Mesh>(fullModelPath), std::make_shared<impgine::Texture2D>(fullTexturePath), pe.color });
    }
}

bool SceneLoader::loadCameraFromScene(const std::string& scenePath, glm::vec3& outPos, glm::vec3& outRot) {
    std::ifstream file(scenePath);
    if (!file.is_open()) return false;

    auto trim = [](std::string s) {
        size_t a = s.find_first_not_of(" \t\r\n");
        size_t b = s.find_last_not_of(" \t\r\n");
        if (a == std::string::npos) return std::string();
        return s.substr(a, b - a + 1);
    };

    auto parseObjVec3 = [&](const std::string& obj) -> glm::vec3 {
        glm::vec3 out{0.0f};
        std::string s = obj;
        if (!s.empty() && s.front() == '{' && s.back() == '}') s = s.substr(1, s.size()-2);
        std::stringstream ss2(s);
        std::string part;
        while (std::getline(ss2, part, ',')) {
            auto colon = part.find(':');
            if (colon == std::string::npos) continue;
            auto k = trim(part.substr(0, colon));
            auto v = trim(part.substr(colon+1));
            float f = std::stof(v);
            if (k == "x") out.x = f; else if (k == "y") out.y = f; else if (k == "z") out.z = f;
        }
        return out;
    };

    std::string line; bool inCamera = false;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line == "Camera:") { inCamera = true; continue; }
        if (line.empty() || line.back() == ':') { inCamera = false; }
        if (!inCamera) continue;

        auto sep = line.find(':');
        if (sep == std::string::npos) continue;
        std::string key = trim(line.substr(0, sep));
        std::string value = trim(line.substr(sep + 1));
        if (key == "position") outPos = parseObjVec3(value);
        else if (key == "rotation") outRot = parseObjVec3(value);
    }
    return true;
}

bool SceneLoader::saveScene(const std::string& scenePath, const ProjectSettings& project, const Camera& camera, ECSRegistry& registry) {
    std::ofstream file(scenePath);
    if (!file.is_open()) return false;

    auto writeVec3 = [&](const char* name, const glm::vec3& v){
        file << "  " << name << ": {x: " << v.x << ", y: " << v.y << ", z: " << v.z << "}\n";
    };

    // Header
    file << "%IMP 1.0\n";

    // Camera block
    file << "Camera:\n";
    writeVec3("position", camera.getPosition());
    writeVec3("rotation", camera.getRotation());

    // Entities
    for (auto e : registry.getEntities()) {
        file << "--- !imp!Entity\n";
        try {
            const auto& tag = registry.getComponent<Tag>(e);
            file << "name: " << tag.tag << "\n";
        } catch (...) {}

        try {
            const auto& t = registry.getComponent<Transform>(e);
            file << "Transform:\n";
            writeVec3("position", t.position);
            writeVec3("rotation", t.rotation);
            writeVec3("scale", t.scale);
        } catch (...) {}

        try {
            const auto& mr = registry.getComponent<MeshRenderer>(e);
            file << "MeshRenderer:\n";
            // Persist paths relative to project root
            std::string relModel = project.toRelativePath(mr.mesh->modelPath);
            std::string relTex = mr.texture ? project.toRelativePath(mr.texture->texturePath) : "";
            file << "  modelPath: " << relModel << "\n";
            file << "  texturePath: " << relTex << "\n";
            writeVec3("color", mr.color);
        } catch (...) {}
    }

    return true;
}
} // namespace impgine
