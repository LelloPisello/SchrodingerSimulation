#include <stdio.h>
#include "common.h"
#include "instance.h"


int main() {
    ssInit();
    SsInstance instance;
    {
        SsInstanceCreateInfo instanceInfo = {
            "SS",
            .windowSize = 512,
        };
        if(ssCreateInstance(&instanceInfo, &instance)) {
            fprintf(stderr, "Failed to create instance: library was not initialized\n");
            return 1;
        }
    }

    while(!ssInstanceShouldClose(instance)) {
        
    }

    ssDestroyInstance(instance);
    ssTerminate();
    return 0;
}