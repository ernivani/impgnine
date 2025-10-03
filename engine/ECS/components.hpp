#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>
#include <cstdint>
#include <limits>

// Forward declare Entity and INVALID_ENTITY from ECSRegistry.hpp
using Entity = uint32_t;
extern const Entity INVALID_ENTITY;

namespace impgine {

    // Axis-Aligned Bounding Box
    struct AABB {
        glm::vec3 min {std::numeric_limits<float>::max()};
        glm::vec3 max {std::numeric_limits<float>::lowest()};

        void expand(const glm::vec3& point) {
            min = glm::min(min, point);
            max = glm::max(max, point);
        }

        glm::vec3 center() const {
            return (min + max) * 0.5f;
        }

        glm::vec3 extents() const {
            return (max - min) * 0.5f;
        }
    };

    // Forward declarations for component pointer types
    struct Material;

    class Texture2D{
        public:
        std::string texturePath;
        glm::vec3 color {1.0f, 1.0f, 1.0f};
        Texture2D() = default;
        explicit Texture2D(const std::string& path)
            : texturePath(path) {}
        Texture2D(const std::string& path, const glm::vec3& tint)
            : texturePath(path), color(tint) {}
    };

    class Mesh {
        public:
        std::string modelPath;
        std::string texturePath;
        glm::vec3 color {1.0f, 1.0f, 1.0f};
        AABB boundingBox;  // Local-space bounding box
        Mesh() = default;
        explicit Mesh(const std::string& model)
            : modelPath(model) {}
        Mesh(const std::string& model, const std::string& texture)
            : modelPath(model), texturePath(texture) {}
        Mesh(const std::string& model, const std::string& texture, const glm::vec3& tint)
            : modelPath(model), texturePath(texture), color(tint) {}
    };

    struct Transform {
        glm::vec3 position {0.0f, 0.0f, 0.0f};
        glm::vec3 rotation {0.0f, 0.0f, 0.0f};
        glm::vec3 scale {1.0f, 1.0f, 1.0f};

        // Get local transform matrix
        glm::mat4 getLocalMatrix() const;

        // Get world transform (requires hierarchy traversal)
        static glm::mat4 getWorldMatrix(Entity entity);
    };

    struct Tag {
        std::string tag;
    };

    struct Hierarchy {
        Entity parent = INVALID_ENTITY;
        std::vector<Entity> children;
    };

    struct Active {
        bool isActive = true;
        bool isActiveSelf = true;  // Active state ignoring parent hierarchy
    };

    struct SpriteRenderer {
        glm::vec3 color = glm::vec3(1.0f);
        std::shared_ptr<Texture2D> texture;
    };


    struct MeshRenderer {
        // Mesh and Materials
        std::shared_ptr<Mesh> mesh;
        std::vector<std::shared_ptr<Material>> materials;
        glm::vec3 color{ 1.0f, 1.0f, 1.0f };  // Legacy color override

        // Lighting
        bool castShadows{true};
        bool receiveShadows{true};
        bool contributeGI{false};

        // Probes
        enum class LightProbeMode { Off, BlendProbes, UseProxyVolume };
        enum class ReflectionProbeMode { Off, BlendProbes, Simple };
        LightProbeMode lightProbes{LightProbeMode::BlendProbes};
        ReflectionProbeMode reflectionProbes{ReflectionProbeMode::BlendProbes};

        // Additional Settings
        enum class MotionVectorMode { Camera, Object, ForceNoMotion };
        MotionVectorMode motionVectors{MotionVectorMode::Object};
        bool dynamicOcclusion{true};

        // Internal fields for picking and bounds
        AABB boundingBox;  // Local-space bounding box (updated when mesh is loaded)
        std::vector<glm::vec3> vertices;  // Local-space vertices for precise picking
        std::vector<uint32_t> indices;     // Triangle indices for precise picking
    };


    struct PushConst {
        glm::mat4 model;
        glm::vec4 color;
    };

    struct Material {
        std::string name{"Default"};
        std::string shaderPath;
        std::string texturePath;
        glm::vec3 color{1.0f, 1.0f, 1.0f};

        // PBR properties
        float metallic{0.0f};
        float roughness{0.5f};
        float emissive{0.0f};

        Material() = default;
        explicit Material(const std::string& matName) : name(matName) {}
        Material(const std::string& matName, const std::string& texPath)
            : name(matName), texturePath(texPath) {}
    };
         




    
    
}