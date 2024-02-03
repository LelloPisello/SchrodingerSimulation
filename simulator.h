#ifndef SIMULATOR_h_
#define SIMULATOR_h_
#include "common.h"
//#include "instance.h"
//SsResult ssCreateSimulation

typedef enum {
    SS_SIM_TYPE_POTENTIAL_WELL = 0
} SsSimulationType;

typedef struct SsSimulationSnapshot_s *SsSimulationSnapshot;

typedef struct {
    //risoluzione della simulazione in quantita' di caselle nella griglia
    uint32_t resolution;
    
    //dimensione della simulazione
    float size;

    //tipo di potenziale nella simulazione
    SsSimulationType type;

    SsBool hasFiltering;

    uint32_t workgroupCount;
} SsSimulationCreateInfo;

//non passo l'istanza tra i parametri perche' sarebbe poco chiaro
//che e' necessaria anche per altro oltre che per il rendering
SsResult ssCreateSimulation(SsInstance instance, const SsSimulationCreateInfo* pInfo, SsSimulation* pSimulation);

#define SS_DELTA_TIME_AUTO -1.f

//il deltaTime puo essere SS_DELTA_TIME_AUTO o negativo, in entrambi i casi il motore usa una sua 
//funzione deltaTime
SsResult ssUpdateSimulation(SsInstance instance, float deltaTime, SsSimulation simulation);

//disegna su un quadrilatero (-1;1) (-1;1) la simulazione,
//usando una scala HSV dove H varia insieme alla fase
//S massima e V in base all'ampiezza della funzione d'onda
SsResult ssRenderSimulation(SsInstance instance, SsSimulation simulation);

void ssDestroySimulation(SsInstance instance, SsSimulation simulation);

typedef struct {
    float real;
    float complex;
} SsSimulationCell;

SsResult ssCreateSimulationSnapshot(SsInstance instance, SsSimulation simulation, SsSimulationSnapshot *snapshot);
void ssDestroySimulationSnapshot(SsInstance instance, SsSimulationSnapshot snapshot);

SsSimulationCell *ssSimulationSnapshotGetData(SsSimulationSnapshot snapshot);


#endif