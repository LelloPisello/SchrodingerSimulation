#include "simulator.h"
#include "common.h"
#include "internal_instance.h"
#include "internal_common.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <vulkan/vulkan_core.h>


//serve per poter creare piu simulazioni
//non e' necessariamente globale ma in questo modo diminuisce il numero di push
static VkFormat CHOSEN_IMAGE_FORMAT;


struct SsSimulation_s {
    //la simulazione avviene sulle immagini
    //al posto che sui buffer, per poterle poi disegnare meglio
    VkDeviceMemory imageMemory[2];
    VkImage images[2];

    //questi servono solo per il rendering
    VkFormat imageFormat;
    VkImageView imageViews[2];
    //uno basta, si puo applicare a qualiasi texture
    //volendo si poteva farne uno per istanza, ma toglieva la possibilita
    //di interpolare a scelta le simulazioni
    VkSampler imageSampler;
    
    //per fare double buffering sulle immagini
    uint32_t lastImage;
    uint32_t resolution;
    float size;
    SsSimulationType type;
    SsBool hasFiltering;
};

static void _updateRenderingDescriptor(SsInstance instance, SsSimulation simulation) {
    VkDescriptorImageInfo imageInfo = {
        .sampler = simulation->imageSampler,
        .imageView = simulation->imageViews[simulation->lastImage],
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
    VkCommandBuffer singleTime;
    ssBeginSingleTimeCommand(instance, SS_QUEUE_FAMILY_GRAPHICS, &singleTime);

    /*
    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .image = simulation->images[!simulation->lastImage],
        .newLayout = isCompute ? VK_IMAGE_LAYOUT_GENERAL :
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,

        .oldLayout = firstTime ? (firstTime = 0, VK_IMAGE_LAYOUT_UNDEFINED) :
            !isCompute ?  VK_IMAGE_LAYOUT_GENERAL :
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,

        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .subresourceRange.baseArrayLayer = 0,
        .subresourceRange.baseMipLevel = 0,
        .subresourceRange.levelCount = 1,
        .subresourceRange.layerCount = 1,
        
    };*/
    VkImageMemoryBarrier barriers[2];
    barriers[0] = (VkImageMemoryBarrier){
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .image = simulation->images[0],
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,

        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,

        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .subresourceRange.baseArrayLayer = 0,
        .subresourceRange.baseMipLevel = 0,
        .subresourceRange.levelCount = 1,
        .subresourceRange.layerCount = 1,
        
    };
    barriers[1] = barriers[0];
    barriers[1].image = simulation->images[1];
    //barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    vkCmdPipelineBarrier(singleTime, 
    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    0, 
    0, NULL, 
    0, NULL, 
    2, barriers);

    ssEndSingleTimeCommand(instance, SS_QUEUE_FAMILY_GRAPHICS, singleTime);
    return SS_SUCCESS;
}

static SsResult _createSimulationImages(SsInstance instance, SsSimulation simulation) {
    //non e' detto che l'implementazione supporti i float a 32 bit
    //nel caso fall back lungo la lista di preferenze
    static int float32Supported = -1;
    //lista di preferenze sui formati, importa solo per la precisione
    //e non sprecare memoria
    static VkFormat preferredFormats[] = {

        VK_FORMAT_R32G32_SFLOAT,
        VK_FORMAT_R32G32_UINT,

        VK_FORMAT_R64G64_SFLOAT,
        VK_FORMAT_R64G64_UINT,

        VK_FORMAT_R16G16_SFLOAT,
        VK_FORMAT_R16G16_UINT,

        VK_FORMAT_R64G64B64_SFLOAT,
        VK_FORMAT_R64G64B64_UINT,

        VK_FORMAT_R32G32B32_SFLOAT,
        VK_FORMAT_R32G32B32_UINT,

        VK_FORMAT_R16G16B16_SFLOAT,
        VK_FORMAT_R16G16B16_UINT,

        VK_FORMAT_R8G8_UINT,
        VK_FORMAT_R8G8_SNORM,

        VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_FORMAT_R32G32B32A32_UINT,


        VK_FORMAT_R64G64B64A64_SFLOAT,
        VK_FORMAT_R64G64B64A64_UINT,


        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_FORMAT_R16G16B16A16_UINT,
        
    };
    static uint32_t chosenFormat;

    if(float32Supported == -1) {
        VkFormatProperties props;
        for(uint32_t i = 0; i < 3; ++i) {
            vkGetPhysicalDeviceFormatProperties(instance->vulkanCore.chosenDevice, preferredFormats[i], &props);
            if((props.linearTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) &&
                (props.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) {
                SS_PRINT("\tFirst time image creation: format chosen %c, index %u\n",
                chosenFormat % 2 ? 'u' : 'f', chosenFormat);
                CHOSEN_IMAGE_FORMAT = preferredFormats[i];
                break;
            }
        }
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
        .extent = {simulation->size, simulation->size, 1},
        .mipLevels = 1,
        .sharingMode = (instance->vulkanCore.queueFamilies[SS_QUEUE_FAMILY_GRAPHICS] != instance->vulkanCore.queueFamilies[SS_QUEUE_FAMILY_COMPUTE]) ?
            VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE,
        .tiling = VK_IMAGE_TILING_LINEAR,
        .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .format = CHOSEN_IMAGE_FORMAT
    };
    if(vkCreateImage(instance->vulkanCore.device, &imageInfo, NULL, &simulation->images[0]) ||
        vkCreateImage(instance->vulkanCore.device, &imageInfo, NULL, &simulation->images[1])) {
        return SS_ERROR_IMAGE_CREATION_FAILURE;
    }
    return SS_SUCCESS;
}

static SsResult _allocateImageMemory(SsInstance instance, SsSimulation simulation) {
    //fare un ciclo doppio verrebbe smontato in istruzioni singole
    
    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(instance->vulkanCore.device, simulation->images[0], &req);
    
    static uint32_t memoryTypeIndex = UINT32_MAX;

    //non creo una funzione per il controllo dei tipi di memoria in quanto la alloco solo qua
    if(memoryTypeIndex == UINT32_MAX) {
        SS_PRINT("\tFirst time allocating image memory, finding memory type...\n");
        VkPhysicalDeviceMemoryProperties memProps;
        vkGetPhysicalDeviceMemoryProperties(instance->vulkanCore.chosenDevice, &memProps);
        for(uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
            if((req.memoryTypeBits & (1 << i)) && 
                memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
                memoryTypeIndex = i;
            }
        }
        if(memoryTypeIndex == UINT32_MAX) {
            SS_PRINT("\tFailed to find a suitable memory type\n");
            return SS_ERROR_MEMORY_TYPE_NOT_AVAILABLE;
        }
    }

    VkMemoryAllocateInfo memInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .memoryTypeIndex = memoryTypeIndex,
        .allocationSize = req.size,
    };
    if(vkAllocateMemory(instance->vulkanCore.device, &memInfo, NULL, &simulation->imageMemory[0]) ||
        vkAllocateMemory(instance->vulkanCore.device, &memInfo, NULL, &simulation->imageMemory[1])) {
        return SS_ERROR_MEMORY_ALLOCATION_FAILURE;
    }

    //potrei unire le due immagini in una se avessi voglia,
    //magari con VMA ma non e' un progetto sufficentemente serio per farlo
    vkBindImageMemory(instance->vulkanCore.device, simulation->images[0], simulation->imageMemory[0], 0);
    vkBindImageMemory(instance->vulkanCore.device, simulation->images[1], simulation->imageMemory[1], 0);


    return SS_SUCCESS;
}

static SsResult _createImageViews(SsInstance instance, SsSimulation simulation) {
    VkImageViewCreateInfo viewInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = simulation->images[0],
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = CHOSEN_IMAGE_FORMAT,
        .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .subresourceRange.baseArrayLayer = 0,
        .subresourceRange.baseMipLevel = 0,
        .subresourceRange.layerCount = 1,
        .subresourceRange.levelCount = 1,
    };
    if(vkCreateImageView(instance->vulkanCore.device, &viewInfo, NULL, &simulation->imageViews[0])) {
        return SS_ERROR_IMAGE_VIEW_CREATION_FAILURE;
    }
    viewInfo.image = simulation->images[1];
    if(vkCreateImageView(instance->vulkanCore.device, &viewInfo, NULL, &simulation->imageViews[1])) {
        return SS_ERROR_IMAGE_VIEW_CREATION_FAILURE;
    }
    return SS_SUCCESS;
}

static SsResult _createImageSamplers(SsInstance instance, SsSimulation simulation) {
    VkSamplerCreateInfo samplerInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .anisotropyEnable = VK_FALSE,
        .magFilter = simulation->hasFiltering ? VK_FILTER_LINEAR : VK_FILTER_NEAREST,
        .minFilter = simulation->hasFiltering ? VK_FILTER_LINEAR : VK_FILTER_NEAREST,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        
    };

    if(vkCreateSampler(instance->vulkanCore.device, &samplerInfo, NULL, &simulation->imageSampler)) {
        return SS_ERROR_IMAGE_SAMPLER_CREATION_FAILURE;
    }

    return SS_SUCCESS;
}

SsResult ssCreateSimulation(SsInstance instance, const SsSimulationCreateInfo *info, SsSimulation *pSimulation) {
    if(!instance || !info || !pSimulation)
        return SS_ERROR_NULLPTR_PASSED;
    
    SsResult temp;

    SS_PRINT("Creating SsSimulation:\n\tAllocating %lu bytes...\n", sizeof(struct SsSimulation_s));
#define ALIAS (*pSimulation)

    ALIAS = malloc(sizeof(struct SsSimulation_s));
    ALIAS->lastImage = 0;
    ALIAS->resolution = info->resolution;
    ALIAS->size = info->size;
    ALIAS->type = info->type;
    ALIAS->hasFiltering = SS_TRUE;

    SS_PRINT("\tCreating simulation images...\n");

    SS_ERROR_CHECK(temp, _createSimulationImages(instance, ALIAS));
    SS_ERROR_CHECK(temp, _allocateImageMemory(instance, ALIAS));
    SS_ERROR_CHECK(temp, _createImageViews(instance, ALIAS));

    SS_PRINT("\tCreating image sampler...\n");
    SS_ERROR_CHECK(temp, _createImageSamplers(instance, ALIAS));

    SS_PRINT("\tTransitioning image for the first time...\n");
    SS_ERROR_CHECK(temp, _transitionImageLayouts(instance, ALIAS));

    SS_PRINT("SsSimulation created\n\n");
#undef ALIAS
    return SS_SUCCESS;
}

void ssDestroySimulation(SsInstance instance, SsSimulation simulation) {
    SS_PRINT("Destroying SsSimulation:\n\t");
    vkDeviceWaitIdle(instance->vulkanCore.device);

    SS_PRINT("\tDestroying image sampler...\n");
    vkDestroySampler(instance->vulkanCore.device, simulation->imageSampler, NULL);

    SS_PRINT("\tDestroying images...\n");
    vkDestroyImageView(instance->vulkanCore.device, simulation->imageViews[0], NULL);
    vkDestroyImageView(instance->vulkanCore.device, simulation->imageViews[1], NULL);

    vkDestroyImage(instance->vulkanCore.device, simulation->images[0], NULL);
    vkDestroyImage(instance->vulkanCore.device, simulation->images[1], NULL);

    SS_PRINT("\tDestroying image memory...\n");
    vkFreeMemory(instance->vulkanCore.device, simulation->imageMemory[0], NULL);
    vkFreeMemory(instance->vulkanCore.device, simulation->imageMemory[1], NULL);

    SS_PRINT("SsSimulation destroyed\n\n");
    free(simulation);
}

static void _writeSimulationDescriptor(SsInstance instance, SsSimulation simulation) {
    VkDescriptorImageInfo imageInfo[2];
    imageInfo[0] = (VkDescriptorImageInfo){
        .sampler = simulation->imageSampler,
        .imageView = simulation->imageViews[simulation->lastImage],
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    };
    imageInfo[1] = imageInfo[0];
    imageInfo[1].imageView = simulation->imageViews[!simulation->lastImage];

    VkWriteDescriptorSet descriptorWrite = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = imageInfo,
        .descriptorCount = 2,
        .dstSet = instance->rendering.descriptor,
    };

    vkUpdateDescriptorSets(instance->vulkanCore.device, 1, &descriptorWrite, 0, NULL);

}

SsResult ssUpdateSimulation(SsInstance instance, float deltaTime, SsSimulation simulation) {
    _writeSimulationDescriptor(instance, simulation);
    return SS_SUCCESS;
}
