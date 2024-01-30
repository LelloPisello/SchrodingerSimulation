#ifndef INT_INS_h_
#define INT_INS_h_

//file esclusivamente ad uso interno che permette l'accesso ai dati interni delle istanze SS
#include "instance.h"
#include <vulkan/vulkan_core.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

typedef enum {
    SS_QUEUE_FAMILY_GRAPHICS = 0,
    SS_QUEUE_FAMILY_PRESENT = 1,
    SS_QUEUE_FAMILY_COMPUTE = 2,
    SS_QUEUE_FAMILY_MAX = 3
} SsInstanceQueueFamilies;

struct SsInstance_s {

    struct {
        VkInstance instance;
        VkPhysicalDevice chosenDevice;
        VkDevice device;
        uint32_t queueFamilies[SS_QUEUE_FAMILY_MAX];
        VkQueue queues[SS_QUEUE_FAMILY_MAX];

        //contiene tutti i descrittori delle immagini delle simulazioni
        VkDescriptorPool descriptorPool;
    } vulkanCore;

    struct {
        VkSurfaceKHR surface;
        VkSurfaceFormatKHR surfaceFormat;
        GLFWwindow* window;
        uint32_t windowSize;

        VkSwapchainKHR swapchain;
        VkExtent2D swapchainExtent;
        VkPresentModeKHR swapchainPresentMode;
        uint32_t swapchainImageCount;
        VkImage* swapchainImages;
        VkImageView *swapchainImageViews;
    } window;

    //questa struttura viene riutilizzata per ogni simulazione
    struct {
        VkFramebuffer *framebuffers;
        VkRenderPass renderPass;

        VkPipeline pipeline;
        VkPipelineLayout pipelineLayout;

        VkShaderModule graphicsVert, graphicsFrag;

        //singolo command buffer registrato ogni frame disegnato
        VkCommandBuffer command;

        //contiene un singolo comando
        VkCommandPool commandPool;
        //combined sampler
        VkDescriptorSetLayout descriptorLayout;
        VkDescriptorSet descriptor;

        VkSemaphore imageAvailableSemaphore;
        //rendering finito
        VkSemaphore renderSemaphore;

        //tra submit
        VkFence flightFence;
    } rendering;

    struct {
        VkPipeline pipeline;
        VkPipelineLayout pipelineLayout;

        VkShaderModule compute;

        VkCommandBuffer command;
        //per registrare i command buffer per le prossime simulazione
        VkCommandPool commandPool;

        VkDescriptorSetLayout descriptorLayout;
        VkDescriptorSet descriptor;        
    } simulationCommons;
};

#endif