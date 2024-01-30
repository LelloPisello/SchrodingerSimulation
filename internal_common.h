#ifndef INTERNAL_h_
#define INTERNAL_h_

#include "common.h"
#include "internal_instance.h"
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

//necessario per varie ragioni
typedef struct {
    float deltaTime;
} SsPushConstants;

SsResult ssBeginSingleTimeCommand(SsInstance instance, SsInstanceQueueFamilies role, VkCommandBuffer *cmd);
SsResult ssEndSingleTimeCommand(SsInstance instance, SsInstanceQueueFamilies role, VkCommandBuffer cmd);

#endif