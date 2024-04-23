#pragma once
#include "fake_os.h"
#include "linked_list.h"
#include <pthread.h>



// Definizione della struttura dati della CPU
typedef struct {
  FakePCB* running;         //Puntatore al processo in esecuzione
  pthread_mutex_t mutex;    //Mutex per garantire la mutua esclusione
  int num_cpu;              //Numero (ID) della CPU
  float usage;
} FakeCPU;



void FakeCPU_init(FakeCPU* cpu);