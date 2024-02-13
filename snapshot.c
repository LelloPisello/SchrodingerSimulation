#include "snapshot.h"
#include "common.h"
#include <stdlib.h>
#include <stdio.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include "internal_simulation.h"
#include "internal_instance.h"
#include "internal_common.h"

struct SsSnapshot_s {
    VkBuffer tempBuffer;
    VkDeviceMemory tempBufferMemory;
    uint32_t resolution;
    VkDeviceSize memorySize;
};

static SsResult _createSnapshotBuffer(SsInstance instance, SsSimulation simulation, SsSnapshot snapshot) {
    
    VkMemoryRequirements imageReq;
    vkGetImageMemoryRequirements(instance->vulkanCore.device, simulation->waveImages[0], &imageReq);

    VkBufferCreateInfo bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .pQueueFamilyIndices = &instance->vulkanCore.queueFamilies[SS_QUEUE_FAMILY_COMPUTE],
        .queueFamilyIndexCount = 1,
        .size = imageReq.size,
        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    };

    if(vkCreateBuffer(instance->vulkanCore.device, &bufferInfo, NULL, &snapshot->tempBuffer)) {
        return SS_ERROR_BUFFER_CREATION_FAILURE;
    }

    return SS_SUCCESS;
}

static SsResult _allocateSnapshotBufferMemory(SsInstance instance, SsSnapshot snapshot) {
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(instance->vulkanCore.device, snapshot->tempBuffer, &req);

    uint32_t memIndex;
    SsResult temp;
    SS_ERROR_CHECK(temp, ssFindMemoryTypeIndex(instance, req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT 
    | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &memIndex));

    snapshot->memorySize = req.size;

    VkMemoryAllocateInfo memInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = req.size,
        .memoryTypeIndex =memIndex,
    };

    if(vkAllocateMemory(instance->vulkanCore.device, &memInfo, NULL, &snapshot->tempBufferMemory)) {
        return SS_ERROR_MEMORY_ALLOCATION_FAILURE;
    }
    if(vkBindBufferMemory(instance->vulkanCore.device, snapshot->tempBuffer, snapshot->tempBufferMemory, 0)) {
        return SS_ERROR_MEMORY_ALLOCATION_FAILURE;
    }

    return SS_SUCCESS;
}

SsResult ssCreateSnapshot(SsInstance instance, SsSimulation simulation, SsSnapshot *snapshot) {
    if(!instance || !snapshot || !simulation)
        return SS_ERROR_NULLPTR_PASSED;

    SsResult temp;

#define ALIAS (*snapshot)
    SS_PRINT("Creating SsSnapshot:\n\tAllocating %lu bytes...\n", sizeof(struct SsSnapshot_s));
    ALIAS = malloc(sizeof(struct SsSnapshot_s));
    ALIAS->resolution = simulation->resolution;
    SS_PRINT("\tCreating buffer...\n");
    SS_ERROR_CHECK(temp, _createSnapshotBuffer(instance, simulation, ALIAS));
    SS_PRINT("\tAllocating buffer memory...\n");
    SS_ERROR_CHECK(temp, _allocateSnapshotBufferMemory(instance, ALIAS));

#undef ALIAS
    SS_PRINT("SsSnapshot created\n\n");

    return SS_SUCCESS;
}

void ssDestroySnapshot(SsInstance instance, SsSnapshot snapshot) {
    SS_PRINT("Destroying SsSnapshot:\n\tDestroying buffer...\n");
    vkDestroyBuffer(instance->vulkanCore.device, snapshot->tempBuffer, NULL);
    SS_PRINT("\tFreeing buffer memory...\n");
    vkFreeMemory(instance->vulkanCore.device, snapshot->tempBufferMemory, NULL);
    SS_PRINT("SsSnapshot destroyed\n\n");
}

SsResult ssSnapshotMap(SsInstance instance, SsSnapshot snapshot, uint32_t *pResolution, SsSimulationCell **ppData) {
    if(!instance || !snapshot || !pResolution || !ppData)
        return SS_ERROR_NULLPTR_PASSED;

    void *temp;
    *pResolution = snapshot->resolution;
    if(vkMapMemory(instance->vulkanCore.device, snapshot->tempBufferMemory, 0, snapshot->memorySize, 0, &temp)) {
        return SS_ERROR_MEMORY_MAP_FAILURE;
    }
    *ppData = temp;
    return SS_SUCCESS;
}

void ssSnapshotUnmap(SsInstance instance, SsSnapshot snapshot) {

    vkUnmapMemory(instance->vulkanCore.device, snapshot->tempBufferMemory);

}

SsResult ssSnapshotGet(SsInstance instance, SsSimulation simulation, SsSnapshot snapshot) {
    VkCommandBuffer singleTime;
    ssBeginSingleTimeCommand(instance, SS_QUEUE_FAMILY_COMPUTE, &singleTime);

    VkBufferImageCopy region = {
        .imageExtent = {simulation->resolution, simulation->resolution, 1},
        //.imageOffset = {},
        .imageSubresource = {
            VK_IMAGE_ASPECT_COLOR_BIT,
            .baseArrayLayer = 0,
            .layerCount = 1,
            .mipLevel = 0
        }
    };

    vkCmdCopyImageToBuffer(singleTime, simulation->waveImages[simulation->lastImage], 
    VK_IMAGE_LAYOUT_GENERAL, snapshot->tempBuffer, 1, &region);

    ssEndSingleTimeCommand(instance, SS_QUEUE_FAMILY_COMPUTE, singleTime);
    return SS_SUCCESS;
}

SsResult ssSnapshotLoad(SsInstance instance, SsSimulation simulation, SsSnapshot snapshot) {
    VkCommandBuffer singleTime;
    ssBeginSingleTimeCommand(instance, SS_QUEUE_FAMILY_COMPUTE, &singleTime);

    VkBufferImageCopy region = {
        .imageExtent = {
            simulation->resolution, simulation->resolution, 1
        },
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseArrayLayer = 0,
            .layerCount = 1,
            .mipLevel = 0
        }
    };

    vkCmdCopyBufferToImage(singleTime, snapshot->tempBuffer, simulation->waveImages[simulation->lastImage], VK_IMAGE_LAYOUT_GENERAL, 1, &region);

    ssEndSingleTimeCommand(instance, SS_QUEUE_FAMILY_COMPUTE, singleTime);

    return SS_SUCCESS;
}