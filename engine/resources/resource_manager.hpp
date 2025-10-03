#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <unordered_map>
#include <vector>
#include "../backend/pipeline.hpp"
#include "../ECS/components.hpp"

namespace impgine {

struct MeshGPUResources {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;
    VkImage textureImage;
    VkDeviceMemory textureImageMemory;
    VkImageView textureImageView;
    VkSampler textureSampler;
    uint32_t mipLevels;
    std::vector<VkDescriptorSet> descriptorSets;
    AABB boundingBox;  // Bounding box in local space
};

struct SpriteGPUResources {
    std::vector<Vertex> vertices;  // Quad vertices
    std::vector<uint32_t> indices;  // Quad indices (6 indices for 2 triangles)
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;
    VkImage textureImage;
    VkDeviceMemory textureImageMemory;
    VkImageView textureImageView;
    VkSampler textureSampler;
    uint32_t mipLevels;
    std::vector<VkDescriptorSet> descriptorSets;
};

class ResourceManager {
public:
    ResourceManager(VkDevice device, VkPhysicalDevice physicalDevice);
    ~ResourceManager() = default;

    void setFallbackTexturePath(const std::string& path) { fallbackTexturePath = path; }

    MeshGPUResources& loadMeshResources(const std::string& modelPath, const std::string& texturePath);
    const std::unordered_map<std::string, MeshGPUResources>& getMeshCache() const { return meshCache; }
    std::unordered_map<std::string, MeshGPUResources>& getMeshCache() { return meshCache; }

    SpriteGPUResources& loadSpriteResources(const std::string& texturePath);
    const std::unordered_map<std::string, SpriteGPUResources>& getSpriteCache() const { return spriteCache; }
    std::unordered_map<std::string, SpriteGPUResources>& getSpriteCache() { return spriteCache; }

    void createVertexBuffer(const std::vector<Vertex>& vertices, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    void createIndexBuffer(const std::vector<uint32_t>& indices, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    void createTextureImage(const std::string& texturePath, VkImage& image, VkDeviceMemory& imageMemory, uint32_t& mipLevels);
    VkImageView createTextureImageView(VkImage image, uint32_t mipLevels);
    VkSampler createTextureSampler(uint32_t mipLevels);

private:
    VkDevice device;
    VkPhysicalDevice physicalDevice;
    std::unordered_map<std::string, MeshGPUResources> meshCache;
    std::unordered_map<std::string, SpriteGPUResources> spriteCache;
    std::string fallbackTexturePath = "textures/texture.jpg";
};

} // namespace impgine
