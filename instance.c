#include "instance.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <vulkan/vulkan_core.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

extern int _glfwHasBeenInitialized;

struct SsInstance_s {
    struct {
        VkInstance instance;
        VkPhysicalDevice chosenDevice;
        VkDevice device;
        uint32_t computeIndex, graphicsIndex, presentIndex;
        VkQueue compute, graphics, present;
    } vulkanCore;

    struct {
        VkSurfaceKHR surface;
        GLFWwindow* window;
        uint32_t windowSize;

        VkSwapchainKHR swapchain;
        uint32_t swapchainImageCount;
        VkImage* swapchainImages;
        VkImageView *swapchainImageViews;
    } window;
};

static SsResult _createVkInstance(SsInstance instance) {
    if(!glfwVulkanSupported()) 
        return SS_ERROR_VULKAN_NOT_SUPPORTED;
    
    VkApplicationInfo appInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .applicationVersion = VK_MAKE_VERSION(SS_VERSION_MAJOR, SS_VERSION_MINOR, SS_VERSION_PATCH),
        .apiVersion = VK_API_VERSION_1_1,
        .engineVersion = VK_MAKE_VERSION(SS_VERSION_MAJOR, SS_VERSION_MINOR, SS_VERSION_PATCH),
        .pEngineName = "Schrodinger Simulation",
        .pApplicationName = "Schrodinger Simulation"
    };


    uint32_t requiredExtensionCount;
    const char** requiredExtensions = glfwGetRequiredInstanceExtensions(&requiredExtensionCount);


    VkInstanceCreateInfo instanceInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        .enabledExtensionCount = requiredExtensionCount,
        .ppEnabledExtensionNames = requiredExtensions,
        
    };

#ifdef SS_DEBUG
    const char* layers[] = {"VK_LAYER_KHRONOS_validation"};
    instanceInfo.enabledLayerCount = 1;
    instanceInfo.ppEnabledLayerNames = layers;
#endif

    if(vkCreateInstance(&instanceInfo, NULL, &instance->vulkanCore.instance)) {
        return SS_ERROR_VKINSTANCE_CREATION_FAILURE;
    }
    return SS_SUCCESS;
}

static void _pickPhysDevice(SsInstance instance) {
    VkPhysicalDevice *devices;
    uint32_t deviceCount;
    vkEnumeratePhysicalDevices(instance->vulkanCore.instance, &deviceCount, NULL);
    vkEnumeratePhysicalDevices(instance->vulkanCore.instance, &deviceCount, 
        devices = calloc(deviceCount, sizeof(VkPhysicalDevice)));
    
    VkPhysicalDeviceProperties temp;
    uint32_t maxScore = 0, score;
    for(uint32_t i = 0; i < deviceCount; ++i) {
        score = 1;
        vkGetPhysicalDeviceProperties(devices[i], &temp);

        switch(temp.deviceType) {
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
                score += 10;
                break;
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
                score += 5;
                break;
            default:
                break;
        }

        if(score > maxScore) {
            maxScore = score;
            instance->vulkanCore.chosenDevice = devices[i];
        }
    }

    free(devices);
}

static SsResult _getQueueFamilies(SsInstance instance) {
    uint32_t queueFamilyCount;
    VkQueueFamilyProperties *queueFamilyProperties;
    vkGetPhysicalDeviceQueueFamilyProperties(instance->vulkanCore.chosenDevice, &queueFamilyCount, NULL);
    vkGetPhysicalDeviceQueueFamilyProperties(instance->vulkanCore.chosenDevice, &queueFamilyCount, 
        queueFamilyProperties = calloc(queueFamilyCount, sizeof(VkQueueFamilyProperties)));

    instance->vulkanCore.presentIndex = queueFamilyCount + 1;

    //int altrimenti farebbe overflow
    //al contrario perche' piu e' basso l'indice piu la famiglia e' primaria
    
    //temporanea, funzione corta quindi non apro un blocco solo per questa
    VkBool32 presentIsSupported;
    for(int i = queueFamilyCount - 1; i >= 0; --i) {
        if(queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            instance->vulkanCore.graphicsIndex = i;
        }
        if(queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            instance->vulkanCore.computeIndex = i;
        }
        //vkGetPhysicalDeviceSurfaceSupportKHR(instance->vulkanCore.chosenDevice, i, VkSurfaceKHR surface, VkBool32 *pSupported)
    }

    free(queueFamilyProperties);
    return SS_SUCCESS;
}

static SsResult _createWindowAndSurface(const SsInstanceCreateInfo* args, SsInstance instance) {
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    instance->window.windowSize = args->windowSize;
    instance->window.window = glfwCreateWindow(args->windowSize, args->windowSize, args->pWindowName, NULL, NULL);
    if(!instance->window.window) {
        return SS_ERROR_WINDOW_CREATION_FAILURE;
    }

    if(glfwCreateWindowSurface(instance->vulkanCore.instance, instance->window.window, NULL, &instance->window.surface)) {
        return SS_ERROR_WINDOW_SURFACE_CREATION_FAILURE;
    }

    return SS_SUCCESS;
}

static SsResult _createVkDevice(SsInstance instance) {

    return SS_SUCCESS;
}

SsResult ssCreateInstance(const SsInstanceCreateInfo *pArgs, SsInstance *pInstance){
    if(!pArgs || !pInstance) {
        return SS_ERROR_NULLPTR_PASSED;
    }
    if(!_glfwHasBeenInitialized){
        return SS_ERROR_MISSED_INITIALIZATION;
    }
    
    SsResult temp;

#define ALIAS (*pInstance)
    ALIAS = calloc(1, sizeof(struct SsInstance_s));

    SS_ERROR_CHECK(temp, _createVkInstance(ALIAS));
    SS_ERROR_CHECK(temp, _createWindowAndSurface(pArgs, ALIAS));
    _pickPhysDevice(ALIAS);
    SS_ERROR_CHECK(temp, _getQueueFamilies(ALIAS));
    
    SS_ERROR_CHECK(temp, _createVkDevice(ALIAS));
#undef ALIAS

    return SS_SUCCESS;
}

void ssDestroyInstance(SsInstance instance) {
    vkDestroySurfaceKHR(instance->vulkanCore.instance, instance->window.surface, NULL);
    glfwDestroyWindow(instance->window.window);
    vkDestroyInstance(instance->vulkanCore.instance, NULL);
    free(instance);
}

uint32_t ssInstanceShouldClose(SsInstance instance) {
    glfwPollEvents();
    return glfwWindowShouldClose(instance->window.window);
}
