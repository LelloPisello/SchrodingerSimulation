#ifndef INT_SIM_h_
#define INT_SIM_h_

#include "common.h"
#include <vulkan/vulkan.h>

struct SsSimulation_s {
    //la simulazione avviene sulle immagini
    //al posto che sui buffer, per poterle poi disegnare meglio
    VkDeviceMemory waveMemory[2];
    VkImage waveImages[2];

    //questi servono solo per il rendering
    VkFormat imageFormat;
    VkImageView waveImageViews[2];
    //uno basta, si puo applicare a qualiasi texture
    //volendo si poteva farne uno per istanza, ma toglieva la possibilita
    //di interpolare a scelta le simulazioni
    VkSampler waveSampler;
    
    //immagine del potenziale
    //il potenziale e' sempre filtrato linearmente
    VkImage potentialImage;
    VkImageView potentialImageView;
    VkSampler potentialSampler;
    VkDeviceMemory potentialMemory;

    //la quarta e' calcolata nella fine
    //tutte senza sampler perche' tanto la risoluzione e' uguale per tutte
    VkImage rkImages[3];
    VkImageView rkImageViews[3];
    VkDeviceMemory rkMemory[3];

    //per fare double buffering sulle immagini
    uint32_t lastImage;
    uint32_t resolution;
    float size;
    SsBool hasFiltering;
    VkFence computeFinishedFence;
};

#endif