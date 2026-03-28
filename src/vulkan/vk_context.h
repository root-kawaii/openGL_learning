#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <string>
#include <iostream>

class VulkanContext {
public:
    VulkanContext() = default;
    ~VulkanContext();

    // No copy
    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    // Initialize the full Vulkan context, creating its own GLFW window.
    bool init(int width, int height);

    // Initialize using an existing GLFW window (window unification).
    // Caller retains ownership — cleanup() will NOT destroy the window.
    // On macOS/MoltenVK, Metal and OpenGL use separate layers on the NSView,
    // so this works even if the window already has an OpenGL context.
    bool initFromExistingWindow(GLFWwindow* window, int width, int height);

    // Recreate swapchain (e.g. on window resize)
    bool recreateSwapchain(uint32_t width, uint32_t height);

    // Cleanup
    void cleanup();

    // Show/hide — no-op when using an externally owned window
    void showWindow();
    void hideWindow();
    GLFWwindow* getWindow() const { return vulkanWindow; }
    bool ownsWindow() const { return ownsWindow_; }

    // Accessors
    VkInstance       getInstance()       const { return instance; }
    VkPhysicalDevice getPhysicalDevice() const { return physicalDevice; }
    VkDevice         getDevice()         const { return device; }
    VkSurfaceKHR     getSurface()        const { return surface; }
    VkSwapchainKHR   getSwapchain()      const { return swapchain; }
    VkQueue          getGraphicsQueue()  const { return graphicsQueue; }
    VkQueue          getPresentQueue()   const { return presentQueue; }
    uint32_t         getGraphicsQueueFamily() const { return graphicsQueueFamily; }
    uint32_t         getPresentQueueFamily()  const { return presentQueueFamily; }
    VkFormat         getSwapchainFormat() const { return swapchainFormat; }
    VkExtent2D       getSwapchainExtent() const { return swapchainExtent; }
    const std::vector<VkImage>&     getSwapchainImages()     const { return swapchainImages; }
    const std::vector<VkImageView>& getSwapchainImageViews() const { return swapchainImageViews; }

    // Print device info for learning/debugging
    void printDeviceInfo() const;

private:
    bool createSwapchain(uint32_t width, uint32_t height);
    void cleanupSwapchain();

    GLFWwindow* vulkanWindow = nullptr;
    bool        ownsWindow_  = true;   // false when using initFromExistingWindow

    // Vulkan handles
    VkInstance               instance       = nullptr;
    VkDebugUtilsMessengerEXT debugMessenger = nullptr;
    VkSurfaceKHR             surface        = nullptr;
    VkPhysicalDevice         physicalDevice = nullptr;
    VkDevice                 device         = nullptr;
    VkSwapchainKHR           swapchain      = nullptr;

    // Queues
    VkQueue  graphicsQueue       = nullptr;
    VkQueue  presentQueue        = nullptr;
    uint32_t graphicsQueueFamily = 0;
    uint32_t presentQueueFamily  = 0;

    // Swapchain data
    VkFormat                 swapchainFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D               swapchainExtent = {0, 0};
    std::vector<VkImage>     swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
};
