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

    struct PendingMaterial {
        std::string name{"Default"};
        std::string texturePath;
        glm::vec3 color{1.0f, 1.0f, 1.0f};
        float metallic{0.0f};
        float roughness{0.5f};
        float emissive{0.0f};
    };

    struct PendingEntity {
        std::optional<std::string> tag;
        std::optional<std::string> parentName;
        bool isActive = true;
        glm::vec3 position {0.0f};
        glm::vec3 rotation {0.0f};
        glm::vec3 scale {1.0f};
        std::string modelPath;
        std::string texturePath;  // Legacy support
        glm::vec3 color {1.0f, 1.0f, 1.0f};  // Legacy support
        std::vector<PendingMaterial> materials;
        bool hasMeshRenderer = false;
        bool hasSpriteRenderer = false;
    };

    std::vector<PendingEntity> entitiesToCreate;
    PendingEntity current;
    PendingMaterial currentMaterial;
    bool inEntityBlock = false;
    bool inMaterialItem = false;
    enum class Section { None, Transform, MeshRenderer, MeshRendererMaterials, MeshRendererMesh, SpriteRenderer };
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
                    // Flush last material if any
                    if (inMaterialItem) {
                        current.materials.push_back(currentMaterial);
                        inMaterialItem = false;
                    }
                    if (inEntityBlock) entitiesToCreate.push_back(current);
                    inEntityBlock = true;
                    current = PendingEntity{};
                    section = Section::None;
                    continue;
                }
            }

            if (line == "Transform:") { section = Section::Transform; continue; }
            if (line == "MeshRenderer:") { section = Section::MeshRenderer; continue; }
            if (line == "SpriteRenderer:") { section = Section::SpriteRenderer; continue; }
            if (line == "materials:") { section = Section::MeshRendererMaterials; continue; }
            if (line == "mesh:") { section = Section::MeshRendererMesh; continue; }

            // Handle material array items (- name: ...)
            if (section == Section::MeshRendererMaterials && line.rfind("- ", 0) == 0) {
                if (inMaterialItem) {
                    current.materials.push_back(currentMaterial);
                }
                inMaterialItem = true;
                currentMaterial = PendingMaterial{};
                // Parse the line after "- "
                std::string rest = line.substr(2);
                auto sep = rest.find(':');
                if (sep != std::string::npos) {
                    std::string key = trim(rest.substr(0, sep));
                    std::string value = trim(rest.substr(sep + 1));
                    if (key == "name") currentMaterial.name = value;
                }
                continue;
            }

            auto sep = line.find(':');
            if (sep == std::string::npos) continue;
            std::string key = trim(line.substr(0, sep));
            std::string value = trim(line.substr(sep + 1));

            if (key == "name") {
                if (!value.empty()) current.tag = value;
                continue;
            }

            if (key == "parent") {
                if (!value.empty()) current.parentName = value;
                continue;
            }

            if (key == "active") {
                current.isActive = (value == "true" || value == "1");
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
            } else if (section == Section::MeshRendererMaterials) {
                // Parse material properties
                if (inMaterialItem) {
                    if (key == "texturePath") currentMaterial.texturePath = value;
                    else if (key == "color") currentMaterial.color = parseObjVec3(value);
                    else if (key == "metallic") currentMaterial.metallic = std::stof(value);
                    else if (key == "roughness") currentMaterial.roughness = std::stof(value);
                    else if (key == "emissive") currentMaterial.emissive = std::stof(value);
                }
            } else if (section == Section::MeshRendererMesh) {
                current.hasMeshRenderer = true;
                if (key == "modelPath") current.modelPath = value;
            } else if (section == Section::MeshRenderer) {
                current.hasMeshRenderer = true;
                // Legacy format support
                if (key == "modelPath") current.modelPath = value;
                else if (key == "texturePath") current.texturePath = value;
                else if (key == "color") current.color = parseObjVec3(value);
            } else if (section == Section::SpriteRenderer) {
                current.hasSpriteRenderer = true;
                if (key == "texture") current.texturePath = value;
                else if (key == "color") current.color = parseObjVec3(value);
            }
        }
        // Flush last material if any
        if (inMaterialItem) {
            current.materials.push_back(currentMaterial);
            inMaterialItem = false;
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
    std::unordered_map<std::string, Entity> entityNameMap;

    // First pass: create all entities
    for (const auto& pe : entitiesToCreate) {
        Entity e = reg.createEntity();
        reg.addComponent<impgine::Transform>(e, { pe.position, pe.rotation, pe.scale });

        if (pe.tag.has_value()) {
            reg.addComponent<impgine::Tag>(e, impgine::Tag{ *pe.tag });
            entityNameMap[*pe.tag] = e;
        }

        // Set active state
        reg.addComponent<impgine::Active>(e, impgine::Active{ pe.isActive, pe.isActive });

        // Add appropriate renderer component based on what was found in the scene file
        if (pe.hasMeshRenderer) {
            impgine::MeshRenderer meshRenderer;

            // Create mesh
            std::string fullModelPath = project.getFullPath(pe.modelPath);

            // Handle materials
            if (!pe.materials.empty()) {
                // New format: use materials array
                std::string meshTexture = pe.materials[0].texturePath.empty() ? "" : project.getFullPath(pe.materials[0].texturePath);
                meshRenderer.mesh = std::make_shared<impgine::Mesh>(fullModelPath, meshTexture);

                // Create Material objects
                for (const auto& pm : pe.materials) {
                    auto mat = std::make_shared<impgine::Material>(pm.name);
                    if (!pm.texturePath.empty()) {
                        mat->texturePath = project.getFullPath(pm.texturePath);
                    }
                    mat->color = pm.color;
                    mat->metallic = pm.metallic;
                    mat->roughness = pm.roughness;
                    mat->emissive = pm.emissive;
                    meshRenderer.materials.push_back(mat);
                }
            } else {
                // Legacy format: use texturePath and color directly
                std::string fullTexturePath = project.getFullPath(pe.texturePath);
                meshRenderer.mesh = std::make_shared<impgine::Mesh>(fullModelPath, fullTexturePath);
                meshRenderer.color = pe.color;
            }

            reg.addComponent<impgine::MeshRenderer>(e, meshRenderer);
        } else if (pe.hasSpriteRenderer) {
            // Resolve texture path relative to project
            std::string fullTexturePath = project.getFullPath(pe.texturePath);

            reg.addComponent<impgine::SpriteRenderer>(e, impgine::SpriteRenderer{
                pe.color,
                std::make_shared<impgine::Texture2D>(fullTexturePath, pe.color)
            });
        }
    }

    // Second pass: establish hierarchy relationships
    for (size_t i = 0; i < entitiesToCreate.size(); ++i) {
        const auto& pe = entitiesToCreate[i];
        if (pe.parentName.has_value()) {
            auto parentIt = entityNameMap.find(*pe.parentName);
            if (parentIt != entityNameMap.end()) {
                Entity child = reg.getEntities()[i];
                Entity parent = parentIt->second;
                reg.setParent(child, parent);
            } else {
                std::cerr << "Warning: Parent '" << *pe.parentName << "' not found for entity";
                if (pe.tag.has_value()) std::cerr << " '" << *pe.tag << "'";
                std::cerr << "\n";
            }
        }
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

        // Write parent relationship
        try {
            const auto& hierarchy = registry.getComponent<Hierarchy>(e);
            if (hierarchy.parent != INVALID_ENTITY) {
                try {
                    const auto& parentTag = registry.getComponent<Tag>(hierarchy.parent);
                    file << "parent: " << parentTag.tag << "\n";
                } catch (...) {}
            }
        } catch (...) {}

        // Write active state
        try {
            const auto& active = registry.getComponent<Active>(e);
            file << "active: " << (active.isActiveSelf ? "true" : "false") << "\n";
        } catch (...) {}

        try {
            const auto& t = registry.getComponent<Transform>(e);
            file << "Transform:\n";
            writeVec3("position", t.position);
            writeVec3("rotation", t.rotation);
            writeVec3("scale", t.scale);
        } catch (...) {}

        // Try MeshRenderer first
        try {
            const auto& mr = registry.getComponent<MeshRenderer>(e);
            file << "MeshRenderer:\n";

            // Write materials array
            if (!mr.materials.empty()) {
                file << "  materials:\n";
                for (const auto& mat : mr.materials) {
                    file << "    - name: " << mat->name << "\n";
                    if (!mat->texturePath.empty()) {
                        std::string relTex = project.toRelativePath(mat->texturePath);
                        file << "      texturePath: " << relTex << "\n";
                    }
                    file << "      color: {x: " << mat->color.x << ", y: " << mat->color.y << ", z: " << mat->color.z << "}\n";
                    file << "      metallic: " << mat->metallic << "\n";
                    file << "      roughness: " << mat->roughness << "\n";
                    file << "      emissive: " << mat->emissive << "\n";
                }
            }

            // Write mesh
            if (mr.mesh) {
                file << "  mesh:\n";
                std::string relModel = project.toRelativePath(mr.mesh->modelPath);
                file << "    modelPath: " << relModel << "\n";
            }
        } catch (...) {
            // Try SpriteRenderer if MeshRenderer not found
            try {
                const auto& sr = registry.getComponent<SpriteRenderer>(e);
                file << "SpriteRenderer:\n";
                // Persist texture path relative to project root
                std::string relTex = sr.texture ? project.toRelativePath(sr.texture->texturePath) : "";
                file << "  texture: " << relTex << "\n";
                writeVec3("color", sr.color);
            } catch (...) {}
        }
    }

    return true;
}
} // namespace impgine
