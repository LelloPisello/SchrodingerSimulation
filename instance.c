#include "common.h"
#include "internal_instance.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan_core.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "internal_common.h"

//non

extern int _SSglfwHasBeenInitialized;


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
        if(queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT && queueFamilyProperties[i].queueFlags & VK_QUEUE_TRANSFER_BIT &&
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
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    //controllo sia supportata la swapchain
    uint32_t availableExtensionCount;
    VkExtensionProperties *availableExtensions;
    vkEnumerateDeviceExtensionProperties(instance->vulkanCore.chosenDevice, NULL, &availableExtensionCount, NULL);
    vkEnumerateDeviceExtensionProperties(instance->vulkanCore.chosenDevice, NULL, 
        &availableExtensionCount, availableExtensions = calloc(availableExtensionCount, sizeof(VkExtensionProperties)));
        
    for(uint32_t i = 0; i < deviceExtensionCount; ++i)
    {
        uint32_t found = 0;
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

    free(availableExtensions);

    VkDeviceCreateInfo deviceInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = uniqueQueueFamilyCount,
        .pQueueCreateInfos = deviceQueues,
        .pEnabledFeatures = &deviceFeatures,
        .enabledExtensionCount = deviceExtensionCount,
        .ppEnabledExtensionNames = deviceExtensions
    };

    if(vkCreateDevice(instance->vulkanCore.chosenDevice, &deviceInfo, NULL, &instance->vulkanCore.device)) {
        return SS_ERROR_DEVICE_CREATION_FAILURE;
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

static inline SsResult _chooseVkSwapchainSurfaceFormat(SsInstance instance) {
    uint32_t formatCount;
    VkSurfaceFormatKHR *formats;
    vkGetPhysicalDeviceSurfaceFormatsKHR(instance->vulkanCore.chosenDevice, instance->window.surface, 
        &formatCount, NULL);
    if(!formatCount) {
        return SS_ERROR_SWAPCHAIN_SUPPORTED_BUT_INADEQUATE;
    }
    vkGetPhysicalDeviceSurfaceFormatsKHR(instance->vulkanCore.chosenDevice, instance->window.surface, 
        &formatCount, formats = calloc(formatCount, sizeof(VkSurfaceFormatKHR)));

    for(uint32_t i = 0; i < formatCount; ++i) {
        if(formats[i].format == VK_FORMAT_B8G8R8A8_SRGB && formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            instance->window.surfaceFormat = formats[i];  
            free(formats);          
            return SS_SUCCESS;
        }
    }

    instance->window.surfaceFormat = formats[0];
    free(formats);
    return SS_SUCCESS;
}

static inline SsResult _chooseVkSwapchainPresentMode(SsInstance instance) {

    uint32_t presentModeCount;
    VkPresentModeKHR *presentModes;

    vkGetPhysicalDeviceSurfacePresentModesKHR(instance->vulkanCore.chosenDevice, instance->window.surface, 
        &presentModeCount, NULL);
    if(!presentModeCount) {
        return SS_ERROR_SWAPCHAIN_SUPPORTED_BUT_INADEQUATE;
    }
    vkGetPhysicalDeviceSurfacePresentModesKHR(instance->vulkanCore.chosenDevice, instance->window.surface, 
        &presentModeCount, presentModes = calloc(presentModeCount, sizeof(VkPresentModeKHR)));
    

    for(uint32_t i = 0; i < presentModeCount; ++i) {
        if(presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            instance->window.swapchainPresentMode = presentModes[i];
            free(presentModes);
            return SS_SUCCESS;
        }
    }

    instance->window.swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;
    free(presentModes);
    return SS_SUCCESS;
}

static inline void _chooseVkSwapchainExtent(VkSurfaceCapabilitiesKHR* surfaceCaps, SsInstance instance) {

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(instance->vulkanCore.chosenDevice, instance->window.surface, surfaceCaps);
    
    if(surfaceCaps->currentExtent.width != UINT32_MAX) {
        instance->window.swapchainExtent = surfaceCaps->currentExtent;
        return;
    } 
    int width, height;
    glfwGetFramebufferSize(instance->window.window, &width, &height);
    
    VkExtent2D temp = {
        .width = (uint32_t)width,
        .height = (uint32_t)height
    };
    
    instance->window.swapchainExtent.width = SS_CLAMP(temp.width, surfaceCaps->minImageExtent.width, surfaceCaps->maxImageExtent.width);
    instance->window.swapchainExtent.height = SS_CLAMP(temp.height, surfaceCaps->minImageExtent.height, surfaceCaps->maxImageExtent.height);


}

static SsResult _createVkSwapchain(SsInstance instance) {


    
    SsResult temp;

    VkSurfaceCapabilitiesKHR caps;

    SS_ERROR_CHECK(temp, _chooseVkSwapchainSurfaceFormat(instance));
    SS_ERROR_CHECK(temp, _chooseVkSwapchainPresentMode(instance));
    _chooseVkSwapchainExtent(&caps, instance);
    
    instance->window.swapchainImageCount = SS_MAX(caps.minImageCount, 2);
    //nel caso l'implementazione non supporta 3 immagini perche' sono troppe faccio un fall back al numero massimo di immagini
    instance->window.swapchainImageCount = SS_MIN(instance->window.swapchainImageCount, caps.maxImageCount);

    SS_PRINT("\t\tSwapchain has %u images\n", instance->window.swapchainImageCount);

    VkSwapchainCreateInfoKHR swapchainInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        //non succede mai
        .oldSwapchain = NULL,
        .imageExtent = instance->window.swapchainExtent,
        .imageFormat = instance->window.surfaceFormat.format,
        .presentMode = instance->window.swapchainPresentMode,
        .imageColorSpace = instance->window.surfaceFormat.colorSpace,
        .surface = instance->window.surface,
        .minImageCount = instance->window.swapchainImageCount,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .preTransform = caps.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .clipped = VK_TRUE,
    };
    
    if(instance->vulkanCore.queueFamilies[SS_QUEUE_FAMILY_GRAPHICS] == 
        instance->vulkanCore.queueFamilies[SS_QUEUE_FAMILY_PRESENT]) {
        swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    } else {
        swapchainInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchainInfo.queueFamilyIndexCount = 2;
        swapchainInfo.pQueueFamilyIndices = instance->vulkanCore.queueFamilies;
    }

    if(vkCreateSwapchainKHR(instance->vulkanCore.device, &swapchainInfo, NULL, &instance->window.swapchain)) {
        return SS_ERROR_SWAPCHAIN_CREATION_FAILURE;
    }

    return SS_SUCCESS;

}

static inline SsResult _createSwapchainImages(SsInstance instance) {
    vkGetSwapchainImagesKHR(instance->vulkanCore.device, instance->window.swapchain, 
        &instance->window.swapchainImageCount, NULL);
    vkGetSwapchainImagesKHR(instance->vulkanCore.device, instance->window.swapchain, 
        &instance->window.swapchainImageCount, instance->window.swapchainImages = calloc(instance->window.swapchainImageCount, sizeof(VkImage)));
    instance->window.swapchainImageViews = calloc(instance->window.swapchainImageCount, sizeof(VkImageView));
    VkImageViewCreateInfo imageInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = instance->window.surfaceFormat.format,
        .components.r = VK_COMPONENT_SWIZZLE_IDENTITY,
        .components.g = VK_COMPONENT_SWIZZLE_IDENTITY,
        .components.b = VK_COMPONENT_SWIZZLE_IDENTITY,
        .components.a = VK_COMPONENT_SWIZZLE_IDENTITY,
        .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .subresourceRange.baseMipLevel = 0,
        .subresourceRange.levelCount = 1,
        .subresourceRange.baseArrayLayer = 0,
        .subresourceRange.layerCount = 1,
    };
    for(uint32_t i = 0; i < instance->window.swapchainImageCount; ++i) {
        imageInfo.image = instance->window.swapchainImages[i];
        if(vkCreateImageView(instance->vulkanCore.device, &imageInfo, NULL, instance->window.swapchainImageViews + i)) {
            return SS_ERROR_SWAPCHAIN_IMAGE_VIEW_CREATION_FAILURE;
        }
    }
    return SS_SUCCESS;
}

static SsResult _createRenderPass(SsInstance instance) {
    VkAttachmentDescription colorAttachment = {
        .format = instance->window.surfaceFormat.format,
        //niente supersampling, inutile
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };

    VkAttachmentReference colorReference = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    VkSubpassDescription subPass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorReference,
    };

    VkSubpassDependency dependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    };

    VkRenderPassCreateInfo renderPassInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &colorAttachment,
        .subpassCount = 1,
        .pSubpasses = &subPass,
        .dependencyCount = 1,
        .pDependencies = &dependency
    };

    if(vkCreateRenderPass(instance->vulkanCore.device, &renderPassInfo, NULL, &instance->rendering.renderPass)) {
        return SS_ERROR_RENDERPASS_CREATION_FAILURE;
    }

    return SS_SUCCESS;
}

static SsResult _createFrameBuffers(SsInstance instance) {
    instance->rendering.framebuffers = calloc(instance->window.swapchainImageCount, sizeof(VkFramebuffer));
    VkFramebufferCreateInfo frameBufferInfo = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .attachmentCount = 1,
        .height = instance->window.swapchainExtent.height,
        .width = instance->window.swapchainExtent.width,
        .layers = 1,
        .renderPass = instance->rendering.renderPass,
        
    };
    for(uint32_t i = 0; i < instance->window.swapchainImageCount; ++i) {
        frameBufferInfo.pAttachments = &instance->window.swapchainImageViews[i];
        if(vkCreateFramebuffer(instance->vulkanCore.device, &frameBufferInfo, NULL, &instance->rendering.framebuffers[i])) {
            return SS_ERROR_FRAMEBUFFER_CREATION_FAILURE;
        }
    }
    return SS_SUCCESS;
}

static SsResult _createCommands(SsInstance instance) {
    VkCommandPoolCreateInfo commandPoolInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = instance->vulkanCore.queueFamilies[SS_QUEUE_FAMILY_GRAPHICS],
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    };
    if(vkCreateCommandPool(instance->vulkanCore.device, &commandPoolInfo, NULL, &instance->rendering.commandPool)) {
        return SS_ERROR_COMMAND_POOL_CREATION_FAILURE;
    }
    //riutilizzo
    commandPoolInfo.queueFamilyIndex = instance->vulkanCore.queueFamilies[SS_QUEUE_FAMILY_COMPUTE];
    if(vkCreateCommandPool(instance->vulkanCore.device, &commandPoolInfo, NULL, &instance->simulationCommons.commandPool)) {
        return SS_ERROR_COMMAND_POOL_CREATION_FAILURE;
    }

    VkCommandBufferAllocateInfo commandInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandBufferCount = 1,
        .commandPool = instance->rendering.commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        
    };

    if(vkAllocateCommandBuffers(instance->vulkanCore.device, &commandInfo, &instance->rendering.command)) {
        return SS_ERROR_COMMAND_BUFFER_CREATION_FAILURE;
    }

    commandInfo.commandPool = instance->simulationCommons.commandPool;
    if(vkAllocateCommandBuffers(instance->vulkanCore.device, &commandInfo, &instance->simulationCommons.command)) {
        return SS_ERROR_COMMAND_BUFFER_CREATION_FAILURE;
    }

    return SS_SUCCESS;
}

//la pipeline compute usa:
//due binding storage image
//alcune push constant tipo delta time ecc ecc
//la pipeline grafica usa:
//un binding image sampler
static SsResult _createDescriptorPools(SsInstance instance) {
    VkDescriptorPoolSize pools[] = {
        {
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1
        },{
            .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 2
        }
    };  
    VkDescriptorPoolCreateInfo descriptorPoolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pPoolSizes = pools,
        .poolSizeCount = 2,
        .maxSets = 2,
        
    };
    //una sola descriptor pool, i descrittori sono aggiornati ogni frame
    //tanto la libreria e' single threaded
    if(vkCreateDescriptorPool(instance->vulkanCore.device, &descriptorPoolInfo, NULL, &instance->vulkanCore.descriptorPool)) {
        return SS_ERROR_DESCRIPTOR_POOL_CREATION_FAILURE;
    }
    return SS_SUCCESS;
}

//compute: 2 immagini storage
//rendering: 1 sampler
static SsResult _createDescriptorSetLayouts(SsInstance instance) {
    //rendering
    VkDescriptorSetLayoutBinding bindings[2] = {   
        {
            .binding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        },{
            .binding = 1,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
        }

    };
    
    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = bindings,
    };

    if(vkCreateDescriptorSetLayout(instance->vulkanCore.device, &layoutInfo, NULL, &instance->rendering.descriptorLayout)) {
        return SS_ERROR_DESCRIPTOR_SET_LAYOUT_CREATION_FAILURE;
    }

    //compute, cambia solo tipo di binding e stage in cui viene usato

    layoutInfo.bindingCount = 2;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    if(vkCreateDescriptorSetLayout(instance->vulkanCore.device, &layoutInfo, NULL, &instance->simulationCommons.descriptorLayout)) {
        return SS_ERROR_DESCRIPTOR_SET_LAYOUT_CREATION_FAILURE;
    }

    return SS_SUCCESS;
}

static SsResult _createDescriptorSets(SsInstance instance) {
    VkDescriptorSetAllocateInfo set = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorSetCount = 1,
        .pSetLayouts = &instance->rendering.descriptorLayout,
        .descriptorPool = instance->vulkanCore.descriptorPool,
    };
    if(vkAllocateDescriptorSets(instance->vulkanCore.device, &set, &instance->rendering.descriptor)) {
        return SS_ERROR_DESCRIPTOR_SET_CREATION_FAILURE;
    }
    set.pSetLayouts = &instance->simulationCommons.descriptorLayout;
    if(vkAllocateDescriptorSets(instance->vulkanCore.device, &set, &instance->simulationCommons.descriptor)) {
        return SS_ERROR_DESCRIPTOR_SET_CREATION_FAILURE;
    }
    return SS_SUCCESS;
}

static SsResult _createSyncObjects(SsInstance instance) {
    VkSemaphoreCreateInfo semInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };
    VkFenceCreateInfo fenInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };
    if(vkCreateSemaphore(instance->vulkanCore.device, &semInfo, NULL, &instance->rendering.imageAvailableSemaphore) ||
        vkCreateSemaphore(instance->vulkanCore.device, &semInfo, NULL, &instance->rendering.renderSemaphore) ||
        vkCreateFence(instance->vulkanCore.device, &fenInfo, NULL, &instance->rendering.flightFence)) {
        return SS_ERROR_SEMAPHORE_CREATION_FAILURE;
    }
    return SS_SUCCESS;
}

//compute: 2 immagini storage + push Constants
//rendering: 1 immagine
static SsResult _createPipelineLayouts(SsInstance instance) {
    
    //rendering
    VkPipelineLayoutCreateInfo layout = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pSetLayouts = &instance->rendering.descriptorLayout,
        .setLayoutCount = 1,
    };
    if(vkCreatePipelineLayout(instance->vulkanCore.device, &layout, NULL, &instance->rendering.pipelineLayout)) {
        return SS_ERROR_PIPELINE_LAYOUT_CREATION_FAILURE;
    }
    
    VkPushConstantRange constants= {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .offset = 0,
        .size = sizeof(SsPushConstants)
    };

    //simulazione
    layout.pSetLayouts = &instance->simulationCommons.descriptorLayout;
    layout.pushConstantRangeCount = 1;
    layout.pPushConstantRanges = &constants;

    if(vkCreatePipelineLayout(instance->vulkanCore.device, &layout, NULL, &instance->simulationCommons.pipelineLayout)) {
        return SS_ERROR_PIPELINE_LAYOUT_CREATION_FAILURE;
    }

    return SS_SUCCESS;
}

//statica dato che la uso solo qua
static void* _readWholeFile(const char* filename, uint32_t *size) {
    FILE* file = fopen(filename, "rb");
    if(!file) 
        return NULL;

    fseek(file, 0, SEEK_END);
    uint32_t fileSize = ftell(file);
    *size = fileSize;
    fseek(file, 0, SEEK_SET);

    void* wholeFile = malloc(fileSize);
    fread(wholeFile, fileSize, 1, file);
    fclose(file);
    return wholeFile;
}


static SsResult _createShaderModule(SsInstance instance, const char* filename, VkShaderModule* shader) {
    uint32_t codeSize;
    void* shaderCode = _readWholeFile(filename, &codeSize);

    if(!shaderCode)
        return SS_ERROR_SHADER_CODE_LOADING_FAILURE;

    VkShaderModuleCreateInfo shaderInfo = {
        .codeSize = codeSize,
        .pCode = shaderCode,
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO
    };

    if(vkCreateShaderModule(instance->vulkanCore.device, &shaderInfo, NULL, shader)){
        return SS_ERROR_SHADER_MODULE_CREATION_FAILURE;
    }

    free(shaderCode);
    return SS_SUCCESS;
}

static SsResult _createSimulationPipeline(SsInstance instance) {
    
    VkComputePipelineCreateInfo computeInfo = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .layout = instance->simulationCommons.pipelineLayout,
        .stage = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = instance->simulationCommons.compute,
            .pName = "main"
        },
    };

    if(vkCreateComputePipelines(instance->vulkanCore.device, VK_NULL_HANDLE, 1, &computeInfo, NULL, &instance->simulationCommons.pipeline)) {
        return SS_ERROR_PIPELINE_CREATION_FAILURE;
    }

    return SS_SUCCESS;
}

static SsResult _createGraphicsPipeline(SsInstance instance) {
    VkPipelineShaderStageCreateInfo shaderStageInfo[2] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = instance->rendering.graphicsVert,
            .pName = "main"
        },{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = instance->rendering.graphicsFrag,
            .pName = "main"
        }
    };
    
    

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    };

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyinfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .primitiveRestartEnable = VK_FALSE,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP
    };

    VkViewport viewport = {
        0, 0, 
        instance->window.swapchainExtent.width, instance->window.swapchainExtent.height,
        0, 1
    };

    VkRect2D scissor = {
        .offset = {0, 0},
        .extent = instance->window.swapchainExtent
    };

    VkPipelineDynamicStateCreateInfo dynamicInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
    };

    VkPipelineViewportStateCreateInfo viewportInfo = {
        .scissorCount = 1,
        .viewportCount = 1,
        .pScissors = &scissor,
        .pViewports = &viewport,
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    };

    VkPipelineRasterizationStateCreateInfo rasterState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .lineWidth = 1.0f,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
    };

    VkPipelineMultisampleStateCreateInfo multisampleState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {
        .colorWriteMask = 
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,

    };

    VkPipelineColorBlendStateCreateInfo colorBlendInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment
    };

    VkGraphicsPipelineCreateInfo pipeInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .layout = instance->rendering.pipelineLayout,
        .stageCount = 2,
        .pStages = shaderStageInfo,
        .pColorBlendState = &colorBlendInfo,
        .pDynamicState = &dynamicInfo,
        .pInputAssemblyState = &inputAssemblyinfo,
        .pVertexInputState = &vertexInputInfo,
        .pMultisampleState = &multisampleState,
        .pRasterizationState = &rasterState,
        .renderPass = instance->rendering.renderPass,
        .pViewportState = &viewportInfo,
        .subpass = 0,
    };

    if(vkCreateGraphicsPipelines(instance->vulkanCore.device, VK_NULL_HANDLE, 1, &pipeInfo, NULL, &instance->rendering.pipeline)) {
        return SS_ERROR_PIPELINE_CREATION_FAILURE;
    }

    return SS_SUCCESS;
}

SsResult ssCreateInstance(const SsInstanceCreateInfo *pArgs, SsInstance *pInstance){
    if(!pArgs || !pInstance) {
        return SS_ERROR_NULLPTR_PASSED;
    }
    if(!_SSglfwHasBeenInitialized){
        return SS_ERROR_MISSED_INITIALIZATION;
    }
    
    SsResult temp;

#define ALIAS (*pInstance)
    SS_PRINT("Creating SsInstance:\n\tAllocating %lu bytes...\n", sizeof(struct SsInstance_s));
    ALIAS = calloc(1, sizeof(struct SsInstance_s));

    SS_PRINT("\tCreating VkInstance...\n");
    SS_ERROR_CHECK(temp, _createVkInstance(ALIAS));

    SS_PRINT("\tCreating a GLFW window + VkSurface...\n");
    SS_ERROR_CHECK(temp, _createWindowAndSurface(pArgs, ALIAS));
    _pickPhysDevice(ALIAS);

    SS_PRINT("\tRetrieving queue families...\n");
    SS_ERROR_CHECK(temp, _getQueueFamilies(ALIAS));
    
    SS_PRINT("\tCreating a VkDevice and retrieving queue handles...\n");
    SS_ERROR_CHECK(temp, _createVkDevice(ALIAS));

    SS_PRINT("\tCreating a VkSwapchain...\n");
    SS_ERROR_CHECK(temp, _createVkSwapchain(ALIAS));

    SS_PRINT("\tCreating images and image views...\n");
    SS_ERROR_CHECK(temp, _createSwapchainImages(ALIAS));

    SS_PRINT("\tCreating render pass...\n");
    SS_ERROR_CHECK(temp, _createRenderPass(ALIAS));

    SS_PRINT("\tCreating framebuffers...\n");
    SS_ERROR_CHECK(temp, _createFrameBuffers(ALIAS));

    SS_PRINT("\tCreating command pools and commands...\n");
    SS_ERROR_CHECK(temp, _createCommands(ALIAS));

    SS_PRINT("\tCreating descriptor pool...\n");
    SS_ERROR_CHECK(temp, _createDescriptorPools(ALIAS));

    SS_PRINT("\tCreating descriptor set layouts...\n");
    SS_ERROR_CHECK(temp, _createDescriptorSetLayouts(ALIAS));

    SS_PRINT("\tCreating descriptor sets...\n");
    SS_ERROR_CHECK(temp, _createDescriptorSets(ALIAS));

    SS_PRINT("\tCreating sync objects...\n");
    SS_ERROR_CHECK(temp, _createSyncObjects(ALIAS));

    SS_PRINT("\tCreating pipeline layouts...\n");
    SS_ERROR_CHECK(temp, _createPipelineLayouts(ALIAS));

    SS_PRINT("\tCreating shaders...\n");
    SS_ERROR_CHECK(temp, _createShaderModule(ALIAS, "shaders/Vrendering.spv", &ALIAS->rendering.graphicsVert));
    SS_ERROR_CHECK(temp, _createShaderModule(ALIAS, "shaders/Frendering.spv", &ALIAS->rendering.graphicsFrag));
    SS_ERROR_CHECK(temp, _createShaderModule(ALIAS, "shaders/simulation.spv", &ALIAS->simulationCommons.compute));

    SS_PRINT("\tCreating graphics pipeline...\n");
    SS_ERROR_CHECK(temp, _createGraphicsPipeline(ALIAS));
    SS_PRINT("\tCreating simulation pipeline...\n");
    SS_ERROR_CHECK(temp, _createSimulationPipeline(ALIAS));

    SS_PRINT("SsInstance created\n\n");
#undef ALIAS

    return SS_SUCCESS;
}

void ssDestroyInstance(SsInstance instance) {
    SS_PRINT("Destroying SsInstance:\n\tWaiting for device idle...\n");
    vkDeviceWaitIdle(instance->vulkanCore.device);

    SS_PRINT("\tDestroying sync objects...\n");
    vkDestroyFence(instance->vulkanCore.device, instance->rendering.flightFence, NULL);
    vkDestroySemaphore(instance->vulkanCore.device, instance->rendering.renderSemaphore, NULL);
    vkDestroySemaphore(instance->vulkanCore.device, instance->rendering.imageAvailableSemaphore, NULL);

    SS_PRINT("\tDestroying graphics pipeline...\n");
    vkDestroyPipeline(instance->vulkanCore.device, instance->rendering.pipeline, NULL);
    SS_PRINT("\tDestroying simulation pipeline...\n");
    vkDestroyPipeline(instance->vulkanCore.device, instance->simulationCommons.pipeline, NULL);

    SS_PRINT("\tDestroying pipeline layouts...\n");
    vkDestroyPipelineLayout(instance->vulkanCore.device, instance->rendering.pipelineLayout, NULL);
    vkDestroyPipelineLayout(instance->vulkanCore.device, instance->simulationCommons.pipelineLayout, NULL);

    SS_PRINT("\tDestroying shaders...\n");
    vkDestroyShaderModule(instance->vulkanCore.device, instance->rendering.graphicsFrag, NULL);
    vkDestroyShaderModule(instance->vulkanCore.device, instance->rendering.graphicsVert, NULL);
    vkDestroyShaderModule(instance->vulkanCore.device, instance->simulationCommons.compute, NULL);



    SS_PRINT("\tDestroying descriptor pool...\n");
    vkDestroyDescriptorPool(instance->vulkanCore.device, instance->vulkanCore.descriptorPool, NULL);

    SS_PRINT("\tDestroying descriptor set layouts...\n");
    vkDestroyDescriptorSetLayout(instance->vulkanCore.device, instance->rendering.descriptorLayout, NULL);
    vkDestroyDescriptorSetLayout(instance->vulkanCore.device, instance->simulationCommons.descriptorLayout, NULL);

    SS_PRINT("\tDestroying command pools...\n");
    vkDestroyCommandPool(instance->vulkanCore.device, instance->rendering.commandPool, NULL);
    vkDestroyCommandPool(instance->vulkanCore.device, instance->simulationCommons.commandPool, NULL);

    SS_PRINT("\tDestroying renderpass...\n");
    vkDestroyRenderPass(instance->vulkanCore.device, instance->rendering.renderPass, NULL);

    SS_PRINT("\tDestroying images:");
    for(uint32_t i = 0; i < instance->window.swapchainImageCount; ++i) {
        SS_PRINT(" %u;", i);
        vkDestroyFramebuffer(instance->vulkanCore.device, instance->rendering.framebuffers[i], NULL);
        vkDestroyImageView(instance->vulkanCore.device, instance->window.swapchainImageViews[i], NULL);
    }
    free(instance->window.swapchainImageViews);
    free(instance->window.swapchainImages);


    SS_PRINT("\n\tDestroying swapchain...\n");
    vkDestroySwapchainKHR(instance->vulkanCore.device, instance->window.swapchain, NULL);
    
    SS_PRINT("\tDestroying device and surface...\n");
    vkDestroyDevice(instance->vulkanCore.device, NULL);
    vkDestroySurfaceKHR(instance->vulkanCore.instance, instance->window.surface, NULL);
    
    SS_PRINT("\tClosing window...\n");
    glfwDestroyWindow(instance->window.window);
    SS_PRINT("\tDestroying VkInstance...\n");
    vkDestroyInstance(instance->vulkanCore.instance, NULL);
    SS_PRINT("SsInstance destroyed\n\n");
    free(instance);
}

uint32_t ssInstanceShouldClose(SsInstance instance) {
    //faccio aggiornamento eventi qui, delay di un frame massimo
    glfwPollEvents();
    return glfwWindowShouldClose(instance->window.window);
}
