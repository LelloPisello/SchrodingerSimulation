#include <stdio.h>
#include "common.h"
#include "instance.h"
#include "simulator.h"

int main() {
    ssInit();
    SsResult temp;
    SsInstance instance;
    {
        SsInstanceCreateInfo instanceInfo = {
            "SS",
            .windowSize = 512,
        };
        if((temp = ssCreateInstance(&instanceInfo, &instance))) {
            fprintf(stderr, "Failed to create instance, error %u\n", (unsigned)temp);
            return 1;
        }
    }

    SsSimulation simulation;
    {
        SsSimulationCreateInfo simInfo = {
            .resolution = 256,
            .size = 256.0f,
            .type = SS_SIM_TYPE_POTENTIAL_WELL,
            .hasFiltering = SS_FALSE,
        };
        if((temp = ssCreateSimulation(instance, &simInfo, &simulation))) {
            fprintf(stderr, "Failed to create simulation, error %u\n", (unsigned)temp);
            return 1;
        }
    }

    if((temp = ssUpdateSimulation(instance, SS_DELTA_TIME_INIT, simulation))) {
        fprintf(stderr, "Failed to update a simulation\n");
        
    }

    while(!ssInstanceShouldClose(instance)) {
        
        if((temp = ssRenderSimulation(instance, simulation))) {
            fprintf(stderr, "Failed to render a simulation, error code %u\n", (unsigned)temp);
            break;
        }
        for(uint32_t i = 0; i < 500; ++i) {
        if((temp = ssUpdateSimulation(instance, 0.0001, simulation))) {
            fprintf(stderr, "Failed to update a simulation\n");
            break;
        }
        }
    }

    ssDestroySimulation(instance, simulation);
    ssDestroyInstance(instance);
    ssTerminate();
    return 0;
}