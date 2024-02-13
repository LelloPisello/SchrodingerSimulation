#ifndef INTERNAL_h_
#define INTERNAL_h_

#include "common.h"
#include "internal_instance.h"
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

//necessario per varie ragioni
typedef struct {
    float deltaTime;
    float scale;
} SsPushConstants;

SsResult ssBeginSingleTimeCommand(SsInstance instance, SsInstanceQueueFamilies role, VkCommandBuffer *cmd);
SsResult ssEndSingleTimeCommand(SsInstance instance, SsInstanceQueueFamilies role, VkCommandBuffer cmd);
SsResult ssFindMemoryTypeIndex(SsInstance instance, uint32_t memoryTypeBits, VkMemoryPropertyFlags memoryProperty, uint32_t *index);
SsResult ssTransitionImageLayout(SsInstance instance, VkImageLayout oldLayout, VkImageLayout newLayout, VkImage image);


#endif