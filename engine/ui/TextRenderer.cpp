#include "TextRenderer.hpp"
#include <iostream>
#include <cstring>
#include <algorithm>

namespace impgine {

    TextRenderer::TextRenderer(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue)
        : device(device), physicalDevice(physicalDevice), commandPool(commandPool), graphicsQueue(graphicsQueue) {

        if (FT_Init_FreeType(&ftLibrary)) {
            throw std::runtime_error("Failed to initialize FreeType library");
        }
    }

    TextRenderer::~TextRenderer() {
        if (textureSampler != VK_NULL_HANDLE) {
            vkDestroySampler(device, textureSampler, nullptr);
        }
        if (textureImageView != VK_NULL_HANDLE) {
            vkDestroyImageView(device, textureImageView, nullptr);
        }
        if (textureImage != VK_NULL_HANDLE) {
            vkDestroyImage(device, textureImage, nullptr);
        }
        if (textureMemory != VK_NULL_HANDLE) {
            vkFreeMemory(device, textureMemory, nullptr);
        }
        if (ftFace) {
            FT_Done_Face(ftFace);
        }
        if (ftLibrary) {
            FT_Done_FreeType(ftLibrary);
        }
    }

    bool TextRenderer::loadFont(const std::string& fontPath, uint32_t fontSize) {
        if (FT_New_Face(ftLibrary, fontPath.c_str(), 0, &ftFace)) {
            std::cerr << "Failed to load font: " << fontPath << std::endl;
            return false;
        }

        FT_Set_Pixel_Sizes(ftFace, 0, fontSize);

        // Calculate atlas dimensions - pack ASCII printable characters (32-126)
        uint32_t maxWidth = 0;
        uint32_t totalHeight = 0;

        for (unsigned char c = 32; c < 127; c++) {
            if (FT_Load_Char(ftFace, c, FT_LOAD_RENDER)) {
                std::cerr << "Failed to load glyph: " << c << std::endl;
                continue;
            }
            maxWidth = std::max(maxWidth, ftFace->glyph->bitmap.width);
            totalHeight = std::max(totalHeight, ftFace->glyph->bitmap.rows);
        }

        // Create atlas with horizontal layout
        atlasWidth = maxWidth * 95;  // 95 printable ASCII characters
        atlasHeight = totalHeight;

        // Create buffer to hold atlas data
        std::vector<unsigned char> atlasData(atlasWidth * atlasHeight, 0);

        uint32_t xOffset = 0;

        // Pack glyphs into atlas
        for (unsigned char c = 32; c < 127; c++) {
            if (FT_Load_Char(ftFace, c, FT_LOAD_RENDER)) {
                continue;
            }

            FT_GlyphSlot glyph = ftFace->glyph;

            // Copy glyph bitmap to atlas
            for (uint32_t y = 0; y < glyph->bitmap.rows; y++) {
                for (uint32_t x = 0; x < glyph->bitmap.width; x++) {
                    uint32_t atlasX = xOffset + x;
                    uint32_t atlasY = y;
                    if (atlasX < atlasWidth && atlasY < atlasHeight) {
                        atlasData[atlasY * atlasWidth + atlasX] = glyph->bitmap.buffer[y * glyph->bitmap.width + x];
                    }
                }
            }

            // Store character info
            Character character;
            character.size = glm::ivec2(glyph->bitmap.width, glyph->bitmap.rows);
            character.bearing = glm::ivec2(glyph->bitmap_left, glyph->bitmap_top);
            character.advance = glyph->advance.x;
            character.texCoordMin = glm::vec2(static_cast<float>(xOffset) / atlasWidth, 0.0f);
            character.texCoordMax = glm::vec2(static_cast<float>(xOffset + glyph->bitmap.width) / atlasWidth,
                                              static_cast<float>(glyph->bitmap.rows) / atlasHeight);

            characters[c] = character;

            xOffset += maxWidth;
        }

        // Upload atlas to GPU
        uploadTextureData(atlasData.data(), atlasWidth, atlasHeight);
        createTextureSampler();

        return true;
    }

    const Character* TextRenderer::getCharacter(char c) const {
        auto it = characters.find(c);
        if (it != characters.end()) {
            return &it->second;
        }
        return nullptr;
    }

    void TextRenderer::uploadTextureData(const unsigned char* data, uint32_t width, uint32_t height) {
        VkDeviceSize imageSize = width * height;

        // Create staging buffer
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = imageSize;
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        vkCreateBuffer(device, &bufferInfo, nullptr, &stagingBuffer);

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device, stagingBuffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        vkAllocateMemory(device, &allocInfo, nullptr, &stagingMemory);
        vkBindBufferMemory(device, stagingBuffer, stagingMemory, 0);

        void* mapped;
        vkMapMemory(device, stagingMemory, 0, imageSize, 0, &mapped);
        std::memcpy(mapped, data, imageSize);
        vkUnmapMemory(device, stagingMemory);

        // Create texture image
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = VK_FORMAT_R8_UNORM;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        vkCreateImage(device, &imageInfo, nullptr, &textureImage);

        vkGetImageMemoryRequirements(device, textureImage, &memRequirements);

        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        vkAllocateMemory(device, &allocInfo, nullptr, &textureMemory);
        vkBindImageMemory(device, textureImage, textureMemory, 0);

        // Transition image layout and copy buffer to image
        VkCommandBufferAllocateInfo cmdAllocInfo{};
        cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdAllocInfo.commandPool = commandPool;
        cmdAllocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(device, &cmdAllocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        // Transition to TRANSFER_DST
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = textureImage;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);

        // Copy buffer to image
        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {width, height, 1};

        vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        // Transition to SHADER_READ_ONLY
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);

        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphicsQueue);

        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);

        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingMemory, nullptr);

        // Create image view
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = textureImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_R8_UNORM;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        vkCreateImageView(device, &viewInfo, nullptr, &textureImageView);
    }

    void TextRenderer::createTextureSampler() {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxAnisotropy = 1.0f;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

        vkCreateSampler(device, &samplerInfo, nullptr, &textureSampler);
    }

    uint32_t TextRenderer::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }

        throw std::runtime_error("Failed to find suitable memory type");
    }

} // namespace impgine