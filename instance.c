#include "instance.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan_core.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

extern int _glfwHasBeenInitialized;

typedef enum {
    SS_QUEUE_FAMILY_GRAPHICS = 0,
    SS_QUEUE_FAMILY_COMPUTE = 1,
    SS_QUEUE_FAMILY_PRESENT = 2,
    SS_QUEUE_FAMILY_MAX = 3
} SsInstanceQueueFamilies;

struct SsInstance_s {


    struct {
        VkInstance instance;
        VkPhysicalDevice chosenDevice;
        VkDevice device;
        uint32_t queueFamilies[SS_QUEUE_FAMILY_MAX];
        VkQueue queues[SS_QUEUE_FAMILY_MAX];
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

    instance->vulkanCore.queueFamilies[SS_QUEUE_FAMILY_PRESENT] = queueFamilyCount + 1;

    //int altrimenti farebbe overflow
    //al contrario perche' piu e' basso l'indice piu la famiglia e' primaria
    
    //temporanea, funzione corta quindi non apro un blocco solo per questa
    VkBool32 presentIsSupported;
    uint32_t currentQueues;
    for(int i = queueFamilyCount - 1; i >= 0; --i) {
        currentQueues = 0;
        if(queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            currentQueues++;
            instance->vulkanCore.queueFamilies[SS_QUEUE_FAMILY_GRAPHICS] = i;
        }
        if(queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT &&
            queueFamilyProperties[i].queueCount > currentQueues) {
            currentQueues++;
            instance->vulkanCore.queueFamilies[SS_QUEUE_FAMILY_COMPUTE] = i;
        }
        vkGetPhysicalDeviceSurfaceSupportKHR(instance->vulkanCore.chosenDevice, i, instance->window.surface, &presentIsSupported);
        if(presentIsSupported &&
            queueFamilyProperties[i].queueCount > currentQueues) {
            instance->vulkanCore.queueFamilies[SS_QUEUE_FAMILY_PRESENT] = i;
        }
    }

    free(queueFamilyProperties);

    if(instance->vulkanCore.queueFamilies[SS_QUEUE_FAMILY_PRESENT] > queueFamilyCount) {
        return SS_ERROR_VULKAN_PRESENTATION_NOT_SUPPORTED;
    }

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

    uint32_t uniqueQueueFamilyCount = 1;
    //per non stare a chiamare calloc o malloc inutilmente
    uint32_t uniqueQueueFamilies[SS_QUEUE_FAMILY_MAX];
    
    //tiene conto di quante code sono assegnate a ciascuna famiglia unica
    uint32_t queuesPerFamily[SS_QUEUE_FAMILY_MAX];
    
    uniqueQueueFamilies[0] = instance->vulkanCore.queueFamilies[SS_QUEUE_FAMILY_GRAPHICS];
    queuesPerFamily[0] = 1;

    for(uint32_t i = 1; i < SS_QUEUE_FAMILY_MAX; ++i) {
        uint32_t unique = 1;
        for(uint32_t j = 0; j < uniqueQueueFamilyCount; ++j) {
            if(uniqueQueueFamilies[j] == instance->vulkanCore.queueFamilies[i]) {
                //se la famiglia di una coda e' gia presente, aggiorna il conteggio
                ++queuesPerFamily[j];
                unique = 0;
                break;
            }
        }
        if(unique) {
            //se la coda e' unica aumenta di uno la dimensione dell'array famiglie uniche
            //e assegna una coda alla quantita di code per famiglia
            uniqueQueueFamilies[uniqueQueueFamilyCount] = instance->vulkanCore.queueFamilies[i];
            queuesPerFamily[uniqueQueueFamilyCount] = 1;
            ++uniqueQueueFamilyCount;
        }
    }

    //stesso ragionamento di sopra, usare un malloc o calloc butterebbe tempo di esecuzione
    VkDeviceQueueCreateInfo deviceQueues[SS_QUEUE_FAMILY_MAX] = {};
    float genericPriorities[SS_QUEUE_FAMILY_MAX];

    //stesso array di priorita generiche usato per ogni famiglia,
    //tanto le code hanno gia' l'ordine corretto
    for(uint32_t i = 0; i < SS_QUEUE_FAMILY_MAX; ++i) {
        genericPriorities[i] = 1.0f - i / (float)SS_QUEUE_FAMILY_MAX;
    }

    for(uint32_t i = 0; i < uniqueQueueFamilyCount; ++i) {
        deviceQueues[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        //entrambi i commenti sono righe inutili
        //deviceQueues[i].queueCount = 0;
        deviceQueues[i].queueFamilyIndex = uniqueQueueFamilies[i];
        deviceQueues[i].pQueuePriorities = genericPriorities;
        //deviceQueues[i].flags = 0;
        for(uint32_t j = 0; j < SS_QUEUE_FAMILY_MAX; ++j) {
            if(instance->vulkanCore.queueFamilies[j] == deviceQueues[i].queueFamilyIndex) {
                ++deviceQueues[i].queueCount;
            }
        }
    }    

    VkPhysicalDeviceFeatures deviceFeatures = {};

    const uint32_t deviceExtensionCount = 1;
    const char* deviceExtensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    //controllo sia supportata la swapchain

    for(uint32_t i = 0; i < deviceExtensionCount; ++i)
    {
        uint32_t found = 0;
        uint32_t availableExtensionCount;
        VkExtensionProperties *availableExtensions;
        vkEnumerateDeviceExtensionProperties(instance->vulkanCore.chosenDevice, NULL, &availableExtensionCount, NULL);
        vkEnumerateDeviceExtensionProperties(instance->vulkanCore.chosenDevice, NULL, 
            &availableExtensionCount, availableExtensions = calloc(availableExtensionCount, sizeof(VkExtensionProperties)));
        for(uint32_t j = 0; j < availableExtensionCount; ++j) {
            if(strcmp(availableExtensions[j].extensionName, deviceExtensions[i]) == 0) {
                found = 1;
                break;
            }
        }
        if(!found) {
            return SS_ERROR_NECESSARY_EXTENSION_NOT_SUPPORTED;
        }
    }

    VkDeviceCreateInfo deviceInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = uniqueQueueFamilyCount,
        .pQueueCreateInfos = deviceQueues,
        .pEnabledFeatures = &deviceFeatures,
        .enabledExtensionCount = deviceExtensionCount,
        .ppEnabledExtensionNames = deviceExtensions
    };

    if(vkCreateDevice(instance->vulkanCore.chosenDevice, &deviceInfo, NULL, &instance->vulkanCore.device)) {
        return SS_ERROR_VKDEVICE_CREATION_FAILURE;
    }

    //parte di retrieving degli handle delle code

    for(uint32_t i = 0; i < uniqueQueueFamilyCount; ++i) {
        for(int j = SS_QUEUE_FAMILY_MAX - 1; j >= 0; --j) {
            if(instance->vulkanCore.queueFamilies[j] == uniqueQueueFamilies[i]) {
                //dovrebbe funzionare (spero)
                vkGetDeviceQueue(instance->vulkanCore.device, instance->vulkanCore.queueFamilies[j],
                    --queuesPerFamily[i], &instance->vulkanCore.queues[j]);
            }
        }
    }
    //le code sono assegnate con il loop interno inverso in modo da averle corrispondenti all'ordinamento interno dell'enum

    return SS_SUCCESS;
}

static SsResult _createVkSwapchain(SsInstance instance) {

    VkSwapchainCreateInfoKHR swapchainInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        //non succede mai
        .oldSwapchain = NULL,
                
    };

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
    vkDeviceWaitIdle(instance->vulkanCore.device);
    vkDestroyDevice(instance->vulkanCore.device, NULL);
    vkDestroySurfaceKHR(instance->vulkanCore.instance, instance->window.surface, NULL);
    glfwDestroyWindow(instance->window.window);
    vkDestroyInstance(instance->vulkanCore.instance, NULL);
    free(instance);
}

uint32_t ssInstanceShouldClose(SsInstance instance) {
    //faccio aggiornamento eventi qui, delay di un frame massimo
    glfwPollEvents();
    return glfwWindowShouldClose(instance->window.window);
}
