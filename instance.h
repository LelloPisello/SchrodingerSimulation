#ifndef INSTANCE_h_
#define INSTANCE_h_

#include "common.h"

typedef struct {
    const char* pWindowName;
    //la finestra e' quadrata per ora
    uint32_t windowSize;
} SsInstanceCreateInfo;

SsResult ssCreateInstance(const SsInstanceCreateInfo* pArgs, SsInstance* pInstance);
void ssDestroyInstance(SsInstance instance);
uint32_t ssInstanceShouldClose(SsInstance instance);


#endif