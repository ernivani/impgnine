#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <optional>

namespace impgine {

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

// Helper functions for debug messenger
VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pDebugMessenger);

void DestroyDebugUtilsMessengerEXT(
    VkInstance instance,
    VkDebugUtilsMessengerEXT debugMessenger,
    const VkAllocationCallbacks* pAllocator);

// Vulkan initialization functions
void createInstance(
    VkInstance& instance,
    const std::vector<const char*>& validationLayers,
    bool enableValidationLayers);

void setupDebugMessenger(
    VkInstance instance,
    VkDebugUtilsMessengerEXT& debugMessenger,
    bool enableValidationLayers);

void createSurface(
    VkInstance instance,
    VkSurfaceKHR& surface,
    void* windowPtr);  // Will be cast to Window*

void pickPhysicalDevice(
    VkInstance instance,
    VkPhysicalDevice& physicalDevice,
    VkSurfaceKHR surface,
    const std::vector<const char*>& deviceExtensions,
    VkSampleCountFlagBits& msaaSamples,
    bool& needsPortabilitySubset);

void createLogicalDevice(
    VkPhysicalDevice physicalDevice,
    VkDevice& device,
    VkQueue& graphicsQueue,
    VkQueue& presentQueue,
    VkSurfaceKHR surface,
    const std::vector<const char*>& deviceExtensions,
    const std::vector<const char*>& validationLayers,
    bool enableValidationLayers,
    bool needsPortabilitySubset);

// Helper functions
bool checkValidationLayerSupport(const std::vector<const char*>& validationLayers);

bool checkDeviceExtensionSupport(
    VkPhysicalDevice device,
    const std::vector<const char*>& deviceExtensions,
    bool& needsPortabilitySubset);

QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);

bool isDeviceSuitable(
    VkPhysicalDevice device,
    VkSurfaceKHR surface,
    const std::vector<const char*>& deviceExtensions);

std::vector<const char*> getRequiredExtensions(bool enableValidationLayers);

void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

// Debug callback
VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData);

} // namespace impgine
