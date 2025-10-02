#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <string>
#include <map>
#include <memory>

namespace impgine {

    struct Character {
        glm::ivec2 size;       // Size of glyph
        glm::ivec2 bearing;    // Offset from baseline to left/top of glyph
        uint32_t advance;      // Horizontal offset to advance to next glyph
        glm::vec2 texCoordMin; // UV coordinates in atlas
        glm::vec2 texCoordMax;
    };

    class TextRenderer {
    public:
        TextRenderer(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue);
        ~TextRenderer();

        TextRenderer(const TextRenderer&) = delete;
        TextRenderer& operator=(const TextRenderer&) = delete;

        // Load a font from file path
        bool loadFont(const std::string& fontPath, uint32_t fontSize);

        // Get character info
        const Character* getCharacter(char c) const;

        // Get texture for binding
        VkImageView getTextureView() const { return textureImageView; }
        VkSampler getSampler() const { return textureSampler; }

    private:
        void createTextureAtlas(uint32_t width, uint32_t height);
        void createTextureSampler();
        void uploadTextureData(const unsigned char* data, uint32_t width, uint32_t height);
        uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

        VkDevice device;
        VkPhysicalDevice physicalDevice;
        VkCommandPool commandPool;
        VkQueue graphicsQueue;

        FT_Library ftLibrary;
        FT_Face ftFace;

        std::map<char, Character> characters;

        VkImage textureImage { VK_NULL_HANDLE };
        VkDeviceMemory textureMemory { VK_NULL_HANDLE };
        VkImageView textureImageView { VK_NULL_HANDLE };
        VkSampler textureSampler { VK_NULL_HANDLE };

        uint32_t atlasWidth { 0 };
        uint32_t atlasHeight { 0 };
    };

} // namespace impgine