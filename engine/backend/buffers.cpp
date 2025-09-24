#include "buffers.hpp"

#include <cassert>
#include <cstring>
#include <stdexcept>

namespace impgine {

    VkDeviceSize Buffer::getAlignment(VkDeviceSize instanceSize, VkDeviceSize minOffsetAlignment) {
        if (minOffsetAlignment > 0) {
            return (instanceSize + minOffsetAlignment - 1) & ~(minOffsetAlignment - 1);
        }
        return instanceSize;
    }

    Buffer::Buffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize instanceSize,
            uint32_t instanceCount, VkBufferUsageFlags usageFlags,
            VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize minOffsetAlignment): device {
            device
        },
        physicalDevice {
            physicalDevice
        },
        instanceCount {
            instanceCount
        },
        instanceSize {
            instanceSize
        },
        usageFlags {
            usageFlags
        },
        memoryPropertyFlags {
            memoryPropertyFlags
        } {
            alignmentSize = getAlignment(instanceSize, minOffsetAlignment);
            bufferSize = alignmentSize * instanceCount;

            VkBufferCreateInfo bufferInfo {};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = bufferSize;
            bufferInfo.usage = usageFlags;
            bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            if (vkCreateBuffer(device, & bufferInfo, nullptr, & buffer) != VK_SUCCESS) {
                throw std::runtime_error("failed to create vertex buffer!");
            }

            VkMemoryRequirements memRequirements;
            vkGetBufferMemoryRequirements(device, buffer, & memRequirements);

            VkMemoryAllocateInfo allocInfo {};
            allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocInfo.allocationSize = memRequirements.size;
            allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, memoryPropertyFlags);

            if (vkAllocateMemory(device, & allocInfo, nullptr, & memory) != VK_SUCCESS) {
                throw std::runtime_error("failed to allocate vertex buffer memory!");
            }

            vkBindBufferMemory(device, buffer, memory, 0);
        }

    Buffer::~Buffer() {
        unmap();
        vkDestroyBuffer(device, buffer, nullptr);
        vkFreeMemory(device, memory, nullptr);
    }

    uint32_t Buffer::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, & memProperties);
        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) &&
                (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }

        throw std::runtime_error("failed to find suitable memory type!");
    }

    VkResult Buffer::map(VkDeviceSize size, VkDeviceSize offset) {
        assert(buffer && memory && "Called map on buffer before create");
        return vkMapMemory(device, memory, offset, size, 0, & mapped);
    }

    void Buffer::unmap() {
        if (mapped) {
            vkUnmapMemory(device, memory);
            mapped = nullptr;
        }
    }

    void Buffer::writeToBuffer(void * data, VkDeviceSize size, VkDeviceSize offset) {
        assert(mapped && "Cannot copy to unmapped buffer");

        if (size == VK_WHOLE_SIZE) {
            memcpy(mapped, data, bufferSize);
        } else {
            char * memOffset = (char * ) mapped;
            memOffset += offset;
            memcpy(memOffset, data, size);
        }
    }

    VkResult Buffer::flush(VkDeviceSize size, VkDeviceSize offset) {
        VkMappedMemoryRange mappedRange = {};
        mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        mappedRange.memory = memory;
        mappedRange.offset = offset;
        mappedRange.size = size;
        return vkFlushMappedMemoryRanges(device, 1, & mappedRange);
    }

    VkResult Buffer::invalidate(VkDeviceSize size, VkDeviceSize offset) {
        VkMappedMemoryRange mappedRange = {};
        mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        mappedRange.memory = memory;
        mappedRange.offset = offset;
        mappedRange.size = size;
        return vkInvalidateMappedMemoryRanges(device, 1, & mappedRange);
    }

    VkDescriptorBufferInfo Buffer::descriptorInfo(VkDeviceSize size, VkDeviceSize offset) {
        return VkDescriptorBufferInfo {
            buffer,
            offset,
            size,
        };
    }

    void Buffer::writeToIndex(void * data, int index) {
        writeToBuffer(data, instanceSize, index * alignmentSize);
    }

    VkResult Buffer::flushIndex(int index) {
        return flush(alignmentSize, index * alignmentSize);
    }

    VkDescriptorBufferInfo Buffer::descriptorInfoForIndex(int index) {
        return descriptorInfo(alignmentSize, index * alignmentSize);
    }

    VkResult Buffer::invalidateIndex(int index) {
        return invalidate(alignmentSize, index * alignmentSize);
    }

} // namespace impgine