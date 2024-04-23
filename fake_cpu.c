#include <stdlib.h>
#include <stdio.h>
#include "fake_cpu.h"

//Inizializzazione della CPU
void FakeCPU_init(FakeCPU* cpu) {
    cpu->running = NULL; //Setta il puntatore al processo in running a NULL
    cpu->num_cpu = 0;
    cpu->usage = 0;
}