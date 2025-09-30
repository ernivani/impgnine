#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>
#include <memory>
#include <string>

namespace impgine {

    // Forward declarations for component pointer types
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
    };

    struct Tag {
        std::string tag;
    };

    struct SpriteRenderer {
        glm::vec3 color = glm::vec3(1.0f);
        std::shared_ptr<Texture2D> texture;
    };


    struct MeshRenderer3D {
        std::shared_ptr<Mesh> mesh;
        std::shared_ptr<Texture2D> texture;
        glm::vec3 color{ 1.0f, 1.0f, 1.0f };
    };


    struct PushConst {
        glm::mat4 model;
        glm::vec4 color;
    };




    
    
}