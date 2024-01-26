#ifndef COMMON_h_
#define COMMON_h_

//da commentare una volta il progetto sia finito
#define SS_DEBUG 

#include <stdint.h>

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

//la simulazione viene renderizzata "su" un istanza con un quad a schermo intero
//usando la fase come hue e la ampiezza come value (HSV)

typedef enum {
    SS_SUCCESS = 0,
    //non usata
    SS_ERROR_GENERIC = 1,


    //utilizzo errato
    SS_ERROR_NULLPTR_PASSED = 32,
    SS_ERROR_MISSED_INITIALIZATION = 33,

    //errori vulkan
    SS_ERROR_VULKAN_NOT_SUPPORTED = 107,
    SS_ERROR_VKINSTANCE_CREATION_FAILURE = 108,
    SS_ERROR_VULKAN_PRESENTATION_NOT_SUPPORTED = 109,

    //errori di creazione
    SS_ERROR_WINDOW_CREATION_FAILURE = 205,
    SS_ERROR_WINDOW_SURFACE_CREATION_FAILURE = 206,

} SsResult;

SsResult ssInit(void);
void ssTerminate(void);

//accorciare la scrittura delle funzioni creazione
#define SS_ERROR_CHECK(var, expr) if((var=expr))return var

#endif