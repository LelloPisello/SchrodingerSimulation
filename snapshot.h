#ifndef SNAPSHOT_h_
#define SNAPSHOT_h_

#include "common.h"

//uno snapshot deve essere per forza targettato da una immagine per una questione di dimensioni
//potrebbe funzionare

//per chi non sa se e' row o column major
#define SS_2D_INDEX(x, y, resolution) (y * resolution + x)

SsResult ssCreateSnapshot(SsInstance instance, SsSimulation simulation, SsSnapshot* snapshot);

void ssDestroySnapshot(SsInstance instance, SsSnapshot snapshot);

//carica dalla simulazione allo snapshot, che si puo poi mappare
SsResult ssSnapshotGet(SsInstance instance, SsSimulation simulation, SsSnapshot snapshot);

//carica dallo snapshot alla sitauzione iniziale, tipo condizione iniziale
SsResult ssSnapshotLoad(SsInstance instance, SsSimulation simulation, SsSnapshot snapshot);

//una cella e' costituita da due float (reale, imm)
typedef struct {
    float real, imag;
} SsSimulationCell;

//write e read
//pResolution DEVE puntare ad un uint32_t  valido, dove verra' inserita la risoluzione (unidimensionale ) della simulazione
//ppData DEVE puntare ad un uint32_T 
SsResult ssSnapshotMap(SsInstance instance, SsSnapshot snapshot, uint32_t *pResolution, SsSimulationCell** ppData);
void ssSnapshotUnmap(SsInstance instance, SsSnapshot snapshot);

#endif