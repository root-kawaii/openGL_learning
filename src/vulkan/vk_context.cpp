#include "vk_context.h"
#include "VkBootstrap.h"

#ifdef __APPLE__
#include <vulkan/vulkan_metal.h>
#endif

// ─── Lifecycle ───────────────────────────────────────────────────────────────

VulkanContext::~VulkanContext() {
    cleanup();
}

bool VulkanContext::init(int width, int height) {
    std::cout << "[Vulkan] Starting initialization..." << std::endl;

    // ── 1. Create Instance ──────────────────────────────────────────────────
    //
    // VkInstance is the connection between your app and the Vulkan library.
    // Validation layers are debug-only error checkers that catch API misuse.
    // The debug messenger routes validation messages to stderr.

    // We create our own GLFW window with GLFW_NO_API because Vulkan needs
    // exclusive access to the window surface — it can't share with OpenGL.
    // During the migration, this window shows Vulkan output while the OpenGL
    // window stays available for toggling back.
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    vulkanWindow = glfwCreateWindow(width, height, "Hephaestus [Vulkan]", nullptr, nullptr);
    if (!vulkanWindow) {
        std::cerr << "[Vulkan] Failed to create GLFW window" << std::endl;
        return false;
    }
    glfwHideWindow(vulkanWindow); // Start hidden, show when toggled on

    auto inst_builder = vkb::InstanceBuilder()
        .set_app_name("Hephaestus")
        .set_engine_name("Hephaestus Engine")
        .require_api_version(1, 2, 0)
        .request_validation_layers()
        .use_default_debug_messenger()
        .build();

    if (!inst_builder) {
        std::cerr << "[Vulkan] Failed to create instance: "
                  << inst_builder.error().message() << std::endl;
        return false;
    }

    vkb::Instance vkb_inst = inst_builder.value();
    instance       = vkb_inst.instance;
    debugMessenger = vkb_inst.debug_messenger;
    std::cout << "[Vulkan] Instance created" << std::endl;

    // ── 2. Create Surface ───────────────────────────────────────────────────
    //
    // VkSurfaceKHR is the bridge between Vulkan and the OS window system.
    // GLFW abstracts the platform-specific surface creation (Metal on macOS).
    // Since our window was created with GLFW_NO_API, this works cleanly.

    VkResult surfaceResult = glfwCreateWindowSurface(instance, vulkanWindow, nullptr, &surface);
    if (surfaceResult != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to create window surface (VkResult "
                  << surfaceResult << ")" << std::endl;
        return false;
    }
    std::cout << "[Vulkan] Surface created" << std::endl;

    // ── 3. Select Physical Device ───────────────────────────────────────────
    //
    // VkPhysicalDevice represents a GPU. The selector picks the best one
    // based on our requirements (Vulkan 1.2, can present to our surface).
    // On macOS with MoltenVK, the portability subset is auto-enabled.

    VkPhysicalDeviceFeatures requiredFeatures{};
    requiredFeatures.samplerAnisotropy = VK_TRUE;

    auto phys_selector = vkb::PhysicalDeviceSelector(vkb_inst)
        .set_surface(surface)
        .set_minimum_version(1, 2)
        .set_required_features(requiredFeatures)
        .prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
        .select();

    if (!phys_selector) {
        std::cerr << "[Vulkan] Failed to select physical device: "
                  << phys_selector.error().message() << std::endl;
        return false;
    }

    vkb::PhysicalDevice vkb_phys = phys_selector.value();
    physicalDevice = vkb_phys.physical_device;
    std::cout << "[Vulkan] Selected GPU: " << vkb_phys.name << std::endl;

    // ── 4. Create Logical Device ────────────────────────────────────────────
    //
    // VkDevice is the logical interface to the GPU. We request queues here.
    // vk-bootstrap automatically creates a graphics queue (and present if
    // the queue family supports it).

    auto dev_builder = vkb::DeviceBuilder(vkb_phys).build();

    if (!dev_builder) {
        std::cerr << "[Vulkan] Failed to create logical device: "
                  << dev_builder.error().message() << std::endl;
        return false;
    }

    vkb::Device vkb_dev = dev_builder.value();
    device = vkb_dev.device;

    // Get queue handles — these are the "channels" for submitting work to GPU
    auto gq = vkb_dev.get_queue(vkb::QueueType::graphics);
    auto pq = vkb_dev.get_queue(vkb::QueueType::present);
    auto gqi = vkb_dev.get_queue_index(vkb::QueueType::graphics);
    auto pqi = vkb_dev.get_queue_index(vkb::QueueType::present);

    if (!gq || !pq || !gqi || !pqi) {
        std::cerr << "[Vulkan] Failed to get queues" << std::endl;
        return false;
    }

    graphicsQueue       = gq.value();
    presentQueue        = pq.value();
    graphicsQueueFamily = gqi.value();
    presentQueueFamily  = pqi.value();

    std::cout << "[Vulkan] Logical device created (graphics queue family: "
              << graphicsQueueFamily << ", present queue family: "
              << presentQueueFamily << ")" << std::endl;

    // ── 5. Create Swapchain ─────────────────────────────────────────────────
    int fbWidth, fbHeight;
    glfwGetFramebufferSize(vulkanWindow, &fbWidth, &fbHeight);
    if (!createSwapchain(static_cast<uint32_t>(fbWidth), static_cast<uint32_t>(fbHeight))) {
        return false;
    }

    printDeviceInfo();
    return true;
}

void VulkanContext::cleanup() {
    if (device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device);
    }

    cleanupSwapchain();

    if (device != VK_NULL_HANDLE) {
        vkDestroyDevice(device, nullptr);
        device = VK_NULL_HANDLE;
    }
    if (surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance, surface, nullptr);
        surface = VK_NULL_HANDLE;
    }
    if (debugMessenger != VK_NULL_HANDLE) {
        auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (func) func(instance, debugMessenger, nullptr);
        debugMessenger = VK_NULL_HANDLE;
    }
    if (instance != VK_NULL_HANDLE) {
        vkDestroyInstance(instance, nullptr);
        instance = VK_NULL_HANDLE;
    }
    if (vulkanWindow) {
        glfwDestroyWindow(vulkanWindow);
        vulkanWindow = nullptr;
    }
}

void VulkanContext::showWindow() {
    if (vulkanWindow) glfwShowWindow(vulkanWindow);
}

void VulkanContext::hideWindow() {
    if (vulkanWindow) glfwHideWindow(vulkanWindow);
}

// ─── Swapchain ───────────────────────────────────────────────────────────────
//
// The swapchain is a queue of images that the GPU renders to and the display
// presents. Think of it as the Vulkan equivalent of double/triple buffering.
// We request a format (color space), present mode (vsync behavior), and extent
// (resolution). MoltenVK typically gives us B8G8R8A8_UNORM or _SRGB.

bool VulkanContext::createSwapchain(uint32_t width, uint32_t height) {
    vkb::SwapchainBuilder swapchain_builder(physicalDevice, device, surface);

    auto swap_result = swapchain_builder
        .set_desired_extent(width, height)
        .set_desired_format({VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
        .add_fallback_format({VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR) // vsync — guaranteed available
        .set_old_swapchain(swapchain) // pass old swapchain for recreation
        .build();

    if (!swap_result) {
        std::cerr << "[Vulkan] Failed to create swapchain: "
                  << swap_result.error().message() << std::endl;
        return false;
    }

    // Destroy old swapchain resources before replacing
    cleanupSwapchain();

    vkb::Swapchain vkb_swap = swap_result.value();
    swapchain       = vkb_swap.swapchain;
    swapchainFormat = vkb_swap.image_format;
    swapchainExtent = vkb_swap.extent;

    // Get swapchain images (owned by the swapchain — do NOT destroy them manually)
    auto images = vkb_swap.get_images();
    if (!images) {
        std::cerr << "[Vulkan] Failed to get swapchain images" << std::endl;
        return false;
    }
    swapchainImages = images.value();

    // Get image views (we own these — must destroy on cleanup)
    auto views = vkb_swap.get_image_views();
    if (!views) {
        std::cerr << "[Vulkan] Failed to get swapchain image views" << std::endl;
        return false;
    }
    swapchainImageViews = views.value();

    std::cout << "[Vulkan] Swapchain created: " << swapchainExtent.width << "x"
              << swapchainExtent.height << " (" << swapchainImages.size()
              << " images)" << std::endl;
    return true;
}

bool VulkanContext::recreateSwapchain(uint32_t width, uint32_t height) {
    vkDeviceWaitIdle(device);
    return createSwapchain(width, height);
}

void VulkanContext::cleanupSwapchain() {
    for (auto imageView : swapchainImageViews) {
        vkDestroyImageView(device, imageView, nullptr);
    }
    swapchainImageViews.clear();
    swapchainImages.clear();

    if (swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, swapchain, nullptr);
        swapchain = VK_NULL_HANDLE;
    }
}

// ─── Info ────────────────────────────────────────────────────────────────────

void VulkanContext::printDeviceInfo() const {
    if (physicalDevice == VK_NULL_HANDLE) return;

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physicalDevice, &props);

    const char* deviceTypeStr = "Unknown";
    switch (props.deviceType) {
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: deviceTypeStr = "Integrated GPU"; break;
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   deviceTypeStr = "Discrete GPU";   break;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    deviceTypeStr = "Virtual GPU";    break;
        case VK_PHYSICAL_DEVICE_TYPE_CPU:            deviceTypeStr = "CPU";            break;
        default: break;
    }

    std::cout << "\n=== Vulkan Device Info ===" << std::endl;
    std::cout << "  Device:     " << props.deviceName << std::endl;
    std::cout << "  Type:       " << deviceTypeStr << std::endl;
    std::cout << "  API:        " << VK_VERSION_MAJOR(props.apiVersion) << "."
              << VK_VERSION_MINOR(props.apiVersion) << "."
              << VK_VERSION_PATCH(props.apiVersion) << std::endl;
    std::cout << "  Swapchain:  " << swapchainExtent.width << "x"
              << swapchainExtent.height << std::endl;
    std::cout << "  Images:     " << swapchainImages.size() << std::endl;
    std::cout << "  Format:     " << swapchainFormat << std::endl;
    std::cout << "=========================\n" << std::endl;
}
