#include <stdio.h>
#include "common.h"
#include "instance.h"
#include "simulator.h"
#include "snapshot.h"
#include <math.h>

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
            .potentialMapType = SS_SIMULATION_POTENTIAL_LATTICE,
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

    uint32_t resolution;
    SsSimulationCell *cells;
    ssSnapshotMap(instance, snapshot, &resolution, &cells);

    for(uint32_t x = 0; x < resolution; ++x) {
        for(uint32_t y = 0; y < resolution; ++y) {
            cells[SS_2D_INDEX(x, y, resolution)].real = sinf(x / (float)resolution * 10) * 0.2;
            cells[SS_2D_INDEX(x, y, resolution)].imag = sinf(y / (float)resolution * 10) * 0.2;

        }
    }
    ssSnapshotUnmap(instance, snapshot);


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