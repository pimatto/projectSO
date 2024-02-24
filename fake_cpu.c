#include <stdlib.h>
#include <stdio.h>
#include "fake_cpu.h"

// Inizializzazione del mutex per una CPU
void initMutex(FakeCPU* cpu) {
    pthread_mutex_init(&cpu->mutex, NULL);
}

// Acquisizione del mutex prima di eseguire un processo su una CPU
void lockMutex(FakeCPU* cpu) {
    pthread_mutex_lock(&cpu->mutex);
}

// Rilascio del mutex dopo aver eseguito un processo su una CPU
void unlockMutex(FakeCPU* cpu) {
    pthread_mutex_unlock(&cpu->mutex);
}