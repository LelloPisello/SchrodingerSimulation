#include "simulator.h"
#include "common.h"
#include "internal_instance.h"
#include "internal_simulation.h"
#include "internal_common.h"
#include "snapshot.h"
#include <bits/time.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <vulkan/vulkan_core.h>


//TODO: 
//-passare a integrazione runge-kutta(4)
//      quindi: 3 immagini extra (k1, k2, k3) (k4 calcolata sul momento)
//-implementare immagini come mappa del potenziale (scene personalizzate)
//-aggiungere filtering cubico se disponibile

static void _updateRenderingDescriptor(SsInstance instance, SsSimulation simulation) {
    VkDescriptorImageInfo imageInfo = {
        .sampler = simulation->waveSampler,
        .imageView = simulation->waveImageViews[simulation->lastImage],
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    };

    VkWriteDescriptorSet descriptorWrite = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &imageInfo,
        .descriptorCount = 1,
        .dstSet = instance->rendering.descriptor,
    };

    vkUpdateDescriptorSets(instance->vulkanCore.device, 1, &descriptorWrite, 0, NULL);


}

static SsResult _recordRenderingCommand(uint32_t nextImage, SsInstance instance, SsSimulation simulation) {
//inizia registrazione command buffer
    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    };

    if(vkBeginCommandBuffer(instance->rendering.command, &beginInfo)) {
        return SS_ERROR_COMMAND_BUFFER_RECORDING_FAILURE;
    }

    //DA TOGLIERE SARA' INUTILE
    VkClearValue clearColor = {{0, 0, 0, 1}};

    

    VkRenderPassBeginInfo renderPassInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = instance->rendering.renderPass,
        .renderArea.offset = {0, 0},
        .renderArea.extent = instance->window.swapchainExtent,
        .clearValueCount = 1,
        .pClearValues = &clearColor,
        .framebuffer = instance->rendering.framebuffers[nextImage]
    };

    vkCmdBeginRenderPass(instance->rendering.command, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    
    vkCmdBindPipeline(instance->rendering.command, VK_PIPELINE_BIND_POINT_GRAPHICS, instance->rendering.pipeline);
    vkCmdBindDescriptorSets(instance->rendering.command, VK_PIPELINE_BIND_POINT_GRAPHICS, instance->rendering.pipelineLayout, 0, 1, &instance->rendering.descriptor, 0, NULL);
    vkCmdDraw(instance->rendering.command, 4, 1, 0, 0);

    //rendering qua
    vkCmdEndRenderPass(instance->rendering.command);

    if(vkEndCommandBuffer(instance->rendering.command)) {
        return SS_ERROR_COMMAND_BUFFER_RECORDING_FAILURE;
    }

    return SS_SUCCESS;
}

SsResult ssRenderSimulation(SsInstance instance, SsSimulation simulation) {
    if(!instance || !simulation) {
        return SS_ERROR_NULLPTR_PASSED;
    }

    //aspetta il rendering sia finito
    vkWaitForFences(instance->vulkanCore.device, 1, &instance->rendering.flightFence, VK_TRUE, UINT64_MAX);
    vkResetFences(instance->vulkanCore.device, 1, &instance->rendering.flightFence);

    SsResult temp;

    uint32_t nextImage;
    vkAcquireNextImageKHR(instance->vulkanCore.device, instance->window.swapchain, UINT64_MAX, instance->rendering.imageAvailableSemaphore, VK_NULL_HANDLE, &nextImage);

    vkResetCommandBuffer(instance->rendering.command, 0);
    
    _updateRenderingDescriptor(instance, simulation);
    SS_ERROR_CHECK(temp, _recordRenderingCommand(nextImage, instance, simulation));
    
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &instance->rendering.command,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &instance->rendering.imageAvailableSemaphore,
        .pWaitDstStageMask = &waitStage,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &instance->rendering.renderSemaphore
    };

    if(vkQueueSubmit(instance->vulkanCore.queues[SS_QUEUE_FAMILY_GRAPHICS], 1, &submitInfo, instance->rendering.flightFence)) {
        return SS_ERROR_QUEUE_SUBMIT_FAILURE;
    }

    VkPresentInfoKHR presentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &instance->rendering.renderSemaphore,
        .swapchainCount = 1,
        .pSwapchains = &instance->window.swapchain,
        .pImageIndices = &nextImage,
    };

    if(vkQueuePresentKHR(instance->vulkanCore.queues[SS_QUEUE_FAMILY_PRESENT], &presentInfo)) {
        return SS_ERROR_PRESENT_FAILURE;
    }

    return SS_SUCCESS;
}

static SsResult _transitionImageLayouts(SsInstance instance, SsSimulation simulation) {
    SsResult temp;
    SS_ERROR_CHECK(temp, ssTransitionImageLayout(instance, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, simulation->waveImages[0]));
    SS_ERROR_CHECK(temp, ssTransitionImageLayout(instance, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, simulation->waveImages[1]));
    //SS_ERROR_CHECK(temp, ssTransitionImageLayout(SsInstance instance, VkImageLayout oldLayout, VkImageLayout newLayout, VkImage image))

    return SS_SUCCESS;
}

static void _potentialHydrogen(uint32_t x, uint32_t y, uint8_t *cell) {

}

static void _potentialLattice(uint32_t x, uint32_t y, uint8_t *cell) {
    *cell = x / 8 % 2 ? 255 : 0;
}

static SsResult _fillPotentialImage(SsInstance instance, const SsSimulationCreateInfo* info, SsSimulation simulation) {
    SsResult temp;

    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(instance->vulkanCore.device, simulation->potentialImage, &req);

    

    VkBuffer stagingBuffer;
    VkBufferCreateInfo bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = req.size,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .pQueueFamilyIndices = &instance->vulkanCore.queueFamilies[SS_QUEUE_FAMILY_COMPUTE],
        .queueFamilyIndexCount = 1,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    };

    if(vkCreateBuffer(instance->vulkanCore.device, &bufferInfo, NULL, &stagingBuffer)) {
        return SS_ERROR_BUFFER_CREATION_FAILURE;
    }

    uint32_t memIndex;

    vkGetBufferMemoryRequirements(instance->vulkanCore.device, stagingBuffer, &req);
    SS_ERROR_CHECK(temp, ssFindMemoryTypeIndex(instance, req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &memIndex));

    VkDeviceMemory stagingBufferMemory;
    VkMemoryAllocateInfo stagingMemoryInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .memoryTypeIndex = memIndex,
        .allocationSize = req.size,
    };

    if(vkAllocateMemory(instance->vulkanCore.device, &stagingMemoryInfo, NULL, &stagingBufferMemory)) {
        return SS_ERROR_MEMORY_ALLOCATION_FAILURE;
    }

    if(vkBindBufferMemory(instance->vulkanCore.device, stagingBuffer, stagingBufferMemory, 0)) {
        return SS_ERROR_MEMORY_ALLOCATION_FAILURE;
    }

    uint8_t *mem;
    if(vkMapMemory(instance->vulkanCore.device, stagingBufferMemory, 0, VK_WHOLE_SIZE, 0, (void**)&mem)){
        return SS_ERROR_MEMORY_MAP_FAILURE;
    }

    void (*fillOp)(uint32_t, uint32_t, uint8_t*);

    switch(info->potentialMapType) {
        case SS_SIMULATION_POTENTIAL_LATTICE:
            fillOp = _potentialLattice;
            break;
        case SS_SIMULATION_POTENTIAL_HYDROGEN_ATOM:
            fillOp = _potentialHydrogen;
            break;
        default:    
            break;
    }

    for(uint32_t i = 0; i < simulation->resolution; ++i) {
        for(uint32_t j = 0; j < simulation->resolution; ++j) {
            fillOp(i, j, mem + j * simulation->resolution + i);
        }
    }

    vkUnmapMemory(instance->vulkanCore.device, stagingBufferMemory);
    
    SS_ERROR_CHECK(temp, ssTransitionImageLayout(instance, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, simulation->potentialImage));
    
    VkCommandBuffer singleTime;
    SS_ERROR_CHECK(temp, ssBeginSingleTimeCommand(instance, SS_QUEUE_FAMILY_COMPUTE, &singleTime));

    VkBufferImageCopy copyInfo = {
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

    vkCmdCopyBufferToImage(singleTime, stagingBuffer, simulation->potentialImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyInfo);

    SS_ERROR_CHECK(temp, ssEndSingleTimeCommand(instance, SS_QUEUE_FAMILY_COMPUTE, singleTime));

    SS_ERROR_CHECK(temp, ssTransitionImageLayout(instance, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, simulation->potentialImage));
    
    vkDestroyBuffer(instance->vulkanCore.device, stagingBuffer, NULL);
    vkFreeMemory(instance->vulkanCore.device, stagingBufferMemory, NULL);

    return SS_SUCCESS;   
}

static SsResult _createSimulationImages(SsInstance instance, SsSimulation simulation) {
    
    //non e' detto che l'implementazione supporti i float a 32 bit
    //nel caso fall back lungo la lista di preferenze
    static int float32Supported = -1;
    //formato per il potenziale, anche se non e' precisissimo fa niente
    static int uint8supported = -1;
    /*
    }*/
    //format singolo
    //con la lista di format preferti avrei dovuto avere una miriade
    //di compute shader per fare in modo funzionassero tutti

    //sulle due variabili supported 1 == LINEAR, 2 == OPTIMAL da fare
    if(float32Supported == -1) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(instance->vulkanCore.chosenDevice, VK_FORMAT_R32G32_SFLOAT, &props);
        
        if(props.linearTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT &&
            props.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) {
            float32Supported = 1;
        } else {
            float32Supported = 0;
        }
    }
    if(uint8supported == -1) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(instance->vulkanCore.chosenDevice, VK_FORMAT_R8_UINT, &props);
        
        if(props.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) {
            uint8supported = 1;
        } else {
            uint8supported = 0;
        }
    }
    if (float32Supported == 0 || uint8supported == 0) {
        return SS_ERROR_IMAGE_CREATION_FAILURE;
    }


    uint32_t queueFamilies[2] = {
        instance->vulkanCore.queueFamilies[SS_QUEUE_FAMILY_GRAPHICS],
        instance->vulkanCore.queueFamilies[SS_QUEUE_FAMILY_COMPUTE]
    };
    VkImageCreateInfo imageInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .queueFamilyIndexCount = 
            (instance->vulkanCore.queueFamilies[SS_QUEUE_FAMILY_GRAPHICS] != instance->vulkanCore.queueFamilies[SS_QUEUE_FAMILY_COMPUTE]) + 1,
        .pQueueFamilyIndices = queueFamilies,
        .arrayLayers = 1,
        .extent = {simulation->resolution, simulation->resolution, 1},
        .mipLevels = 1,
        .sharingMode = (instance->vulkanCore.queueFamilies[SS_QUEUE_FAMILY_GRAPHICS] != instance->vulkanCore.queueFamilies[SS_QUEUE_FAMILY_COMPUTE]) ?
            VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE,
        .tiling = VK_IMAGE_TILING_LINEAR,
        //i due transfer sono per poter caricare e salvare snapshot
        .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .format = VK_FORMAT_R32G32_SFLOAT
    };
    if(vkCreateImage(instance->vulkanCore.device, &imageInfo, NULL, &simulation->waveImages[0]) ||
        vkCreateImage(instance->vulkanCore.device, &imageInfo, NULL, &simulation->waveImages[1])) {
        return SS_ERROR_IMAGE_CREATION_FAILURE;
    }
    //transfer dst per caricare l'immagine iniziale
    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageInfo.format = VK_FORMAT_R8_UINT;
    if(vkCreateImage(instance->vulkanCore.device, &imageInfo, NULL, &simulation->potentialImage)) {
        return SS_ERROR_IMAGE_CREATION_FAILURE;
    }
    return SS_SUCCESS;
}

static SsResult _allocateImageMemory(SsInstance instance, SsSimulation simulation) {
    //fare un ciclo doppio verrebbe smontato in istruzioni singole
    
    VkMemoryRequirements waveReq;
    VkMemoryRequirements potentialReq;
    vkGetImageMemoryRequirements(instance->vulkanCore.device, simulation->potentialImage, &potentialReq);
    vkGetImageMemoryRequirements(instance->vulkanCore.device, simulation->waveImages[0], &waveReq);
    
    static uint32_t waveMemoryTypeIndex = UINT32_MAX / 2;
    static uint32_t potentialMemoryTypeIndex = UINT32_MAX / 2;

    //non creo una funzione per il controllo dei tipi di memoria in quanto la alloco solo qua
    if(waveMemoryTypeIndex == UINT32_MAX / 2) {
        SS_PRINT("\t\tFirst time allocating wave image memory, finding memory type: ");
        ssFindMemoryTypeIndex(instance, waveReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &waveMemoryTypeIndex);
        
        SS_PRINT("index %u chosen\n", waveMemoryTypeIndex);
    }
    if(potentialMemoryTypeIndex == UINT32_MAX / 2) {
        SS_PRINT("\t\tFirst time allocating potential image memory, finding memory type: ");
        ssFindMemoryTypeIndex(instance, potentialReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &potentialMemoryTypeIndex);
        SS_PRINT("index %u chosen\n", potentialMemoryTypeIndex);
    }
    if(waveMemoryTypeIndex == UINT32_MAX || potentialMemoryTypeIndex == UINT32_MAX) {
        SS_PRINT("\t\tA memory type was not available, quitting...\n");
        return SS_ERROR_MEMORY_TYPE_NOT_AVAILABLE;
    }

    VkMemoryAllocateInfo memInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .memoryTypeIndex = waveMemoryTypeIndex,
        .allocationSize = waveReq.size,
    };
    if(vkAllocateMemory(instance->vulkanCore.device, &memInfo, NULL, &simulation->waveMemory[0]) ||
        vkAllocateMemory(instance->vulkanCore.device, &memInfo, NULL, &simulation->waveMemory[1])) {
        return SS_ERROR_MEMORY_ALLOCATION_FAILURE;
    }
    memInfo.memoryTypeIndex = potentialMemoryTypeIndex;
    memInfo.allocationSize = potentialReq.size;
    if(vkAllocateMemory(instance->vulkanCore.device, &memInfo, NULL, &simulation->potentialMemory)) {
        return SS_ERROR_MEMORY_ALLOCATION_FAILURE;
    }

    //potrei unire le due immagini in una se avessi voglia,
    //magari con VMA ma non e' un progetto sufficentemente serio per farlo
    vkBindImageMemory(instance->vulkanCore.device, simulation->waveImages[0], simulation->waveMemory[0], 0);
    vkBindImageMemory(instance->vulkanCore.device, simulation->waveImages[1], simulation->waveMemory[1], 0);

    vkBindImageMemory(instance->vulkanCore.device, simulation->potentialImage, simulation->potentialMemory, 0);

    return SS_SUCCESS;
}

static SsResult _createSimulationFence(SsInstance instance, SsSimulation simulation) {
    VkFenceCreateInfo fenceInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    };
    if(vkCreateFence(instance->vulkanCore.device, &fenceInfo, NULL, &simulation->computeFinishedFence)) {
        return SS_ERROR_SEMAPHORE_CREATION_FAILURE;
    }
    return SS_SUCCESS;
}

static SsResult _createImageViews(SsInstance instance, SsSimulation simulation) {
    VkImageViewCreateInfo viewInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = simulation->waveImages[0],
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R32G32_SFLOAT,
        .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .subresourceRange.baseArrayLayer = 0,
        .subresourceRange.baseMipLevel = 0,
        .subresourceRange.layerCount = 1,
        .subresourceRange.levelCount = 1,
        
    };
    if(vkCreateImageView(instance->vulkanCore.device, &viewInfo, NULL, &simulation->waveImageViews[0])) {
        return SS_ERROR_IMAGE_VIEW_CREATION_FAILURE;
    }
    viewInfo.image = simulation->waveImages[1];
    if(vkCreateImageView(instance->vulkanCore.device, &viewInfo, NULL, &simulation->waveImageViews[1])) {
        return SS_ERROR_IMAGE_VIEW_CREATION_FAILURE;
    }
    viewInfo.format = VK_FORMAT_R8_UINT;
    viewInfo.image = simulation->potentialImage;
    if(vkCreateImageView(instance->vulkanCore.device, &viewInfo, NULL, &simulation->potentialImageView)) {
        return SS_ERROR_IMAGE_VIEW_CREATION_FAILURE;
    }
    return SS_SUCCESS;
}

static SsResult _createImageSamplers(SsInstance instance, SsSimulation simulation) {
    VkSamplerCreateInfo samplerInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .anisotropyEnable = VK_FALSE,
        .magFilter = simulation->hasFiltering ? VK_FILTER_LINEAR : VK_FILTER_NEAREST,
        .minFilter = simulation->hasFiltering ? VK_FILTER_LINEAR : VK_FILTER_NEAREST,
        .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
    };

    if(vkCreateSampler(instance->vulkanCore.device, &samplerInfo, NULL, &simulation->waveSampler)) {
        return SS_ERROR_IMAGE_SAMPLER_CREATION_FAILURE;
    }
    
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.magFilter = samplerInfo.minFilter = VK_FILTER_LINEAR;

    if(vkCreateSampler(instance->vulkanCore.device, &samplerInfo, NULL, &simulation->potentialSampler)) {
        return SS_ERROR_IMAGE_SAMPLER_CREATION_FAILURE;
    }

    return SS_SUCCESS;
}



SsResult ssCreateSimulation(SsInstance instance, const SsSimulationCreateInfo *info, SsSimulation *pSimulation) {
    if(!instance || !info || !pSimulation)
        return SS_ERROR_NULLPTR_PASSED;
    if(info->scale <= 0 || info->resolution == 0) {
        return SS_ERROR_BAD_PARAMETER;
    }
    SsResult temp;

    SS_PRINT("Creating SsSimulation:\n\tAllocating %lu bytes...\n", sizeof(struct SsSimulation_s));
#define ALIAS (*pSimulation)

    ALIAS = malloc(sizeof(struct SsSimulation_s));
    ALIAS->lastImage = 0;
    ALIAS->resolution = info->resolution;
    ALIAS->size = info->scale;
    ALIAS->hasFiltering = info->linearFiltering;

    SS_PRINT("\tCreating simulation images...\n");

    SS_ERROR_CHECK(temp, _createSimulationImages(instance, ALIAS));
    SS_ERROR_CHECK(temp, _allocateImageMemory(instance, ALIAS));
    SS_ERROR_CHECK(temp, _createImageViews(instance, ALIAS));

    SS_PRINT("\tCreating image sampler...\n");
    SS_ERROR_CHECK(temp, _createImageSamplers(instance, ALIAS));

    SS_PRINT("\tTransitioning image for the first time...\n");
    SS_ERROR_CHECK(temp, _transitionImageLayouts(instance, ALIAS));

    SS_PRINT("\tTransitioning and initializing potential image...\n");
    SS_ERROR_CHECK(temp, _fillPotentialImage(instance, info, ALIAS));

    SS_PRINT("\tCreating simulation fence...\n");
    SS_ERROR_CHECK(temp, _createSimulationFence(instance, ALIAS));

    SS_PRINT("SsSimulation created\n\n");
#undef ALIAS
    return SS_SUCCESS;
}

void ssDestroySimulation(SsInstance instance, SsSimulation simulation) {
    SS_PRINT("Destroying SsSimulation:\n\t");
    vkDeviceWaitIdle(instance->vulkanCore.device);

    SS_PRINT("\tDestroying simulation fence...\n");
    vkDestroyFence(instance->vulkanCore.device, simulation->computeFinishedFence, NULL);

    SS_PRINT("\tDestroying image sampler...\n");
    vkDestroySampler(instance->vulkanCore.device, simulation->waveSampler, NULL);
    vkDestroySampler(instance->vulkanCore.device, simulation->potentialSampler, NULL);

    SS_PRINT("\tDestroying images...\n");
    vkDestroyImageView(instance->vulkanCore.device, simulation->waveImageViews[0], NULL);
    vkDestroyImageView(instance->vulkanCore.device, simulation->waveImageViews[1], NULL);
    vkDestroyImageView(instance->vulkanCore.device, simulation->potentialImageView, NULL);

    vkDestroyImage(instance->vulkanCore.device, simulation->potentialImage, NULL);
    vkDestroyImage(instance->vulkanCore.device, simulation->waveImages[0], NULL);
    vkDestroyImage(instance->vulkanCore.device, simulation->waveImages[1], NULL);

    SS_PRINT("\tDestroying image memory...\n");
    vkFreeMemory(instance->vulkanCore.device, simulation->waveMemory[0], NULL);
    vkFreeMemory(instance->vulkanCore.device, simulation->waveMemory[1], NULL);
    vkFreeMemory(instance->vulkanCore.device, simulation->potentialMemory, NULL);

    SS_PRINT("SsSimulation destroyed\n\n");
    free(simulation);
}

static void _writeSimulationDescriptor(SsInstance instance, SsSimulation simulation) {
    VkDescriptorImageInfo imageInfo[2];
    imageInfo[0] = (VkDescriptorImageInfo){
        .sampler = simulation->waveSampler,
        .imageView = simulation->waveImageViews[simulation->lastImage],
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    };
    imageInfo[1] = imageInfo[0];
    imageInfo[1].imageView = simulation->waveImageViews[!simulation->lastImage];

    VkWriteDescriptorSet descriptorWrite = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .pImageInfo = imageInfo,
        .descriptorCount = 2,
        .dstSet = instance->simulationCommons.descriptor,
    };

    vkUpdateDescriptorSets(instance->vulkanCore.device, 1, &descriptorWrite, 0, NULL);

}

static SsResult _recordSimulationCommand(SsInstance instance, float deltaTime, SsSimulation simulation) {
    VkCommandBufferBeginInfo cmdBegin = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    };
    if(vkBeginCommandBuffer(instance->simulationCommons.command, &cmdBegin)) {
        return SS_ERROR_COMMAND_BUFFER_RECORDING_FAILURE;
    }
    

    vkCmdBindPipeline(instance->simulationCommons.command, VK_PIPELINE_BIND_POINT_COMPUTE, instance->simulationCommons.pipeline);
    vkCmdBindDescriptorSets(instance->simulationCommons.command, VK_PIPELINE_BIND_POINT_COMPUTE, instance->simulationCommons.pipelineLayout, 
    0, 1, &instance->simulationCommons.descriptor, 0, NULL);

    SsPushConstants pushConstant = {
        .deltaTime = deltaTime,
        .scale = simulation->size
    };


    VkImageMemoryBarrier imagebarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .image = simulation->waveImages[!simulation->lastImage],
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseArrayLayer = 0,
            .baseMipLevel = 0,
            .layerCount = 1,
            .levelCount = 1
        }
    };
        
    vkCmdPipelineBarrier(instance->simulationCommons.command, 
    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 
    0, 0, NULL, 0, NULL, 
    1, &imagebarrier);


    vkCmdPushConstants(instance->simulationCommons.command, instance->simulationCommons.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(SsPushConstants), &pushConstant);
    vkCmdDispatch(instance->simulationCommons.command, simulation->resolution / 4, 
        simulation->resolution / 4, 1);
    
    imagebarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    imagebarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(instance->simulationCommons.command, 
    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 
    0, 0, NULL, 0, NULL, 
    1, &imagebarrier);

    if(vkEndCommandBuffer(instance->simulationCommons.command)) {
        return SS_ERROR_COMMAND_BUFFER_RECORDING_FAILURE;
    }

    return SS_SUCCESS;
}

static float _internalDeltaTime() {
    float result;
#ifdef linux
    static struct timespec lastTime = {}, newTime;
    if(!lastTime.tv_sec) {
        clock_gettime(CLOCK_REALTIME, &lastTime);
    }
    clock_gettime(CLOCK_REALTIME, &newTime);
    result = (newTime.tv_sec - lastTime.tv_sec) + (newTime.tv_nsec - lastTime.tv_nsec) * 0.000000001f;
#else
    static float lastTime = -1, newTime;
    if(lastTime < 0)
        lastTime = glfwGetTime();
    newTime = glfwGetTime();
    result = newTime - lastTime;
#endif
    lastTime = newTime;
    return result;
}

SsResult ssUpdateSimulation(SsInstance instance, float deltaTime, SsSimulation simulation) {
    _writeSimulationDescriptor(instance, simulation);
    simulation->lastImage = !simulation->lastImage;
    
    if(deltaTime < 0) {
        deltaTime = _internalDeltaTime();
    }

    SsResult temp;

    vkResetCommandBuffer(instance->simulationCommons.command, 0);
    SS_ERROR_CHECK(temp, _recordSimulationCommand(instance, deltaTime, simulation));

    VkSubmitInfo submit = {
        .commandBufferCount = 1,
        .pCommandBuffers = &instance->simulationCommons.command,
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO
    };
    if(vkQueueSubmit(instance->vulkanCore.queues[SS_QUEUE_FAMILY_COMPUTE], 1, &submit, simulation->computeFinishedFence)) {
        return SS_ERROR_QUEUE_SUBMIT_FAILURE;
    }
    vkWaitForFences(instance->vulkanCore.device, 1, &simulation->computeFinishedFence, VK_FALSE, UINT64_MAX);
    vkResetFences(instance->vulkanCore.device, 1, &simulation->computeFinishedFence);

    return SS_SUCCESS;
}
