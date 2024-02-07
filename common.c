#include "common.h"
#include "internal_common.h"
#include "internal_instance.h"
#include <GLFW/glfw3.h>
#include <stdint.h>
#include <vulkan/vulkan_core.h>

int _SSglfwHasBeenInitialized = 0;

SsResult ssInit(void) {
    if(glfwInit() == GLFW_FALSE)
        return SS_ERROR_MISSED_INITIALIZATION;
    _SSglfwHasBeenInitialized = 1;
    return SS_SUCCESS;
}
void ssTerminate(void) {
    _SSglfwHasBeenInitialized = 0;
    glfwTerminate();
}

SsResult ssBeginSingleTimeCommand(SsInstance instance, SsInstanceQueueFamilies role, VkCommandBuffer *cmd) {
    VkCommandBufferAllocateInfo command = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandBufferCount = 1,
        .commandPool = 
        role == SS_QUEUE_FAMILY_COMPUTE ? instance->simulationCommons.commandPool : 
        instance->rendering.commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    };

    if(vkAllocateCommandBuffers(instance->vulkanCore.device, &command, &(*cmd))) {
        return SS_ERROR_IMAGE_CREATION_FAILURE;
    }

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };  

    if(vkBeginCommandBuffer((*cmd), &beginInfo)) {
        vkFreeCommandBuffers(instance->vulkanCore.device, role == SS_QUEUE_FAMILY_COMPUTE ? instance->simulationCommons.commandPool : 
        instance->rendering.commandPool, 1, cmd);
        return SS_ERROR_COMMAND_BUFFER_RECORDING_FAILURE;
    }

    return SS_SUCCESS;;
}

SsResult ssEndSingleTimeCommand(SsInstance instance, SsInstanceQueueFamilies role, VkCommandBuffer cmd) {
    if(vkEndCommandBuffer(cmd)) {   
        return SS_ERROR_COMMAND_BUFFER_RECORDING_FAILURE;
    }

    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
    };

    VkFence tempFence;
    VkFenceCreateInfo fenceInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO
    };

    if(vkCreateFence(instance->vulkanCore.device, &fenceInfo, NULL, &tempFence)) {
        return SS_ERROR_SEMAPHORE_CREATION_FAILURE;
    }

    if(vkQueueSubmit(instance->vulkanCore.queues[SS_QUEUE_FAMILY_GRAPHICS], 1, &submitInfo, tempFence)) {
        return SS_ERROR_QUEUE_SUBMIT_FAILURE;
    }

    vkWaitForFences(instance->vulkanCore.device, 1, &tempFence, VK_TRUE, UINT64_MAX);

    vkDestroyFence(instance->vulkanCore.device, tempFence, NULL);
    

    vkFreeCommandBuffers(instance->vulkanCore.device, instance->rendering.commandPool, 1, &cmd);
    return SS_SUCCESS;
}

SsResult ssFindMemoryTypeIndex(SsInstance instance, uint32_t memoryTypeBits, VkMemoryPropertyFlags memoryProperty, uint32_t *index) {
    *index = UINT32_MAX;
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(instance->vulkanCore.chosenDevice, &memProps);
    for(uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if((memoryTypeBits & (1 << i)) && 
            ((memProps.memoryTypes[i].propertyFlags & memoryProperty) == memoryProperty)) {
            *index = i;
        }
    }
    if(*index == UINT32_MAX) {
        return SS_ERROR_MEMORY_TYPE_NOT_AVAILABLE;
    }
    return SS_SUCCESS;  
}
