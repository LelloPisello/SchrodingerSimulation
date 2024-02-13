#include <stdio.h>
#include "common.h"
#include "instance.h"
#include "simulator.h"
#include "snapshot.h"

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
            .scale = 256.0f,
            .potentialMapType = SS_SIMULATION_POTENTIAL_HYDROGEN_ATOM,
            .linearFiltering = SS_TRUE,
        };
        if((temp = ssCreateSimulation(instance, &simInfo, &simulation))) {
            fprintf(stderr, "Failed to create simulation, error %u\n", (unsigned)temp);
            return 1;
        }
    }

    SsSnapshot snapshot;
    if((temp = ssCreateSnapshot(instance, simulation, &snapshot))) {
        fprintf(stderr, "Failed to create snapshot, error %u\n", (unsigned)temp);
        return 1;
    }

    

    if((temp = ssUpdateSimulation(instance, SS_DELTA_TIME_INIT, simulation))) {
        fprintf(stderr, "Failed to update a simulation\n");
        
    }

    if((temp = ssSnapshotGet(instance, simulation, snapshot))) {
        fprintf(stderr, "Failed to get a snapshot\n");
    }
    if((temp = ssSnapshotLoad(instance, simulation, snapshot))) {
        fprintf(stderr, "Failed to load a snapshot into a simulation\n");
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

    ssDestroySnapshot(instance, snapshot);
    ssDestroySimulation(instance, simulation);
    ssDestroyInstance(instance);
    ssTerminate();
    return 0;
}