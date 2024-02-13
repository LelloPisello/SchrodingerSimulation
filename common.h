#ifndef COMMON_h_
#define COMMON_h_

//da commentare una volta il progetto sia finito
#define SS_DEBUG 

#ifdef SS_DEBUG
#define SS_PRINT(...) fprintf(stderr, __VA_ARGS__)

#else
#define SS_PRINT(...) (0)
#endif

#include <stdint.h>

#define SS_CLAMP(val, min, max)((val) < (min) ? (min) : (val) > (max) ? (max) : (val))

#define SS_MIN(a, b)((a) > (b) ? (b) : (a))

#define SS_MAX(a, b)((a) < (b) ? (b) : (a))

#define SS_VERSION_MAJOR 0
#define SS_VERSION_MINOR 1
#define SS_VERSION_PATCH 0

//il codice rispetta camelcase e la convenzione di scrittura usata in vulkan
//il codice e' inoltre completamente opaco per permetterne la vendita 

//l'applicazione e' single thread in quanto non esiste lavoro su CPU oltre alla gestione dell'input

//contiene una finestra (GLFW) e gli oggetti core di vulkan
//{VkInstance, VkPhyisicalDevice(serve ogni tanto), VkDevice, due VkQueue, una swapchain (con le sue immagini)...}
typedef struct SsInstance_s *SsInstance;

//una simulazione contiene: 
//due immagini (R parte reale funzione G parte complessa)
typedef struct SsSimulation_s *SsSimulation;

//uno snapshot della simulazione:
//immagine trasferita dalla simulaione in un istante qulasias
//puo anche essere caricata nella simulazione come stato iniziale
typedef struct SsSnapshot_s *SsSnapshot;

//la simulazione viene renderizzata "su" un istanza con un quad a schermo intero
//usando la fase come hue e la ampiezza come value (HSV)

//importante: le funzioni iniziali tendono ad essere molto lunghe
//dato che molte delle cose che fanno non servono altrove, e separare alcune cose sarebbe un overhead inutile

//ho lasciato alcuni dati che potrebbero essere omessi dalle strutture (temporanei)
//per rendere possibile una futura estensione del progetto

typedef enum {
    SS_SUCCESS = 0,
    //non usata
    SS_ERROR_GENERIC = 1,


    //utilizzo errato
    SS_ERROR_BAD_USAGE_BIT = 32,
    SS_ERROR_NULLPTR_PASSED = 32,
    SS_ERROR_MISSED_INITIALIZATION = 33,
    SS_ERROR_BAD_PARAMETER = 34,

    //errori vulkan
    SS_ERROR_VULKAN_BIT = 128,
    SS_ERROR_VULKAN_NOT_SUPPORTED = 128,
    SS_ERROR_VKINSTANCE_CREATION_FAILURE = 129,
    SS_ERROR_VULKAN_PRESENTATION_NOT_SUPPORTED = 130,
    SS_ERROR_DEVICE_CREATION_FAILURE = 131,
    SS_ERROR_NECESSARY_EXTENSION_NOT_SUPPORTED =  132,
    SS_ERROR_SWAPCHAIN_SUPPORTED_BUT_INADEQUATE = 133,
    SS_ERROR_SWAPCHAIN_CREATION_FAILURE = 134,
    SS_ERROR_SWAPCHAIN_IMAGE_VIEW_CREATION_FAILURE = 135,
    SS_ERROR_RENDERPASS_CREATION_FAILURE = 136,
    SS_ERROR_FRAMEBUFFER_CREATION_FAILURE = 137,
    SS_ERROR_COMMAND_POOL_CREATION_FAILURE = 138,
    SS_ERROR_COMMAND_BUFFER_CREATION_FAILURE = 139,
    SS_ERROR_DESCRIPTOR_POOL_CREATION_FAILURE = 140,
    SS_ERROR_DESCRIPTOR_SET_LAYOUT_CREATION_FAILURE = 141,
    SS_ERROR_DESCRIPTOR_SET_CREATION_FAILURE = 142,
    SS_ERROR_COMMAND_BUFFER_RECORDING_FAILURE = 143,
    SS_ERROR_SEMAPHORE_CREATION_FAILURE = 144,
    SS_ERROR_QUEUE_SUBMIT_FAILURE = 145,
    SS_ERROR_PRESENT_FAILURE = 146,
    SS_ERROR_IMAGE_CREATION_FAILURE = 147,
    SS_ERROR_MEMORY_ALLOCATION_FAILURE = 148,
    SS_ERROR_MEMORY_TYPE_NOT_AVAILABLE = 149,
    SS_ERROR_IMAGE_VIEW_CREATION_FAILURE = 150,
    SS_ERROR_IMAGE_SAMPLER_CREATION_FAILURE = 151,
    SS_ERROR_PIPELINE_LAYOUT_CREATION_FAILURE = 152,
    SS_ERROR_SHADER_MODULE_CREATION_FAILURE = 153,
    SS_ERROR_PIPELINE_CREATION_FAILURE = 154,
    SS_ERROR_BUFFER_CREATION_FAILURE = 155,
    SS_ERROR_MEMORY_MAP_FAILURE = 156,
    

    //errori di creazione
    SS_ERROR_CREATION_ERROR_BIT = 256,
    SS_ERROR_WINDOW_CREATION_FAILURE = 256,
    SS_ERROR_WINDOW_SURFACE_CREATION_FAILURE = 257,

    //errori interni libreria
    SS_ERROR_INTERNAL_BIT = 512,
    SS_ERROR_SHADER_CODE_LOADING_FAILURE = 512,
    
} SsResult;

SsResult ssInit(void);
void ssTerminate(void);

//32 bit perche le dword sono piu veloci da allocare solitamente
typedef uint32_t SsBool;

#define SS_TRUE 1u
#define SS_FALSE 0u

//accorciare la scrittura delle funzioni creazione
#define SS_ERROR_CHECK(var, expr) if((var=expr))return var

#endif