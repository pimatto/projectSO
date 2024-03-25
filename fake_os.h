#include "fake_process.h"
#include "linked_list.h"
#include <pthread.h>
#pragma once


typedef struct {
  ListItem list;
  int pid;
  ListHead events;
  pthread_mutex_t mutex; // Mutex per il processo
  int readyTime; //Variabile per non eseguire il processo appena messo in ready
} FakePCB;

struct FakeOS;
typedef void (*ScheduleFn)(struct FakeOS* os, void* args);

typedef struct FakeOS{
  FakePCB* running; //Da togliere con l'utilizzo delle CPU
  ListHead ready;
  ListHead waiting;
  int timer;
  ScheduleFn schedule_fn;
  void* schedule_args;

  ListHead processes;

  ListHead cpu_list; // Lista di CPU

  //ListHead all_processes//Lista di tutti i processi per il predicted
} FakeOS;

void FakeOS_init(FakeOS* os);
void FakeOS_simStep(FakeOS* os, int sel_cpu);
void FakeOS_destroy(FakeOS* os);



void initMutex_process(FakeProcess* process);
void lockMutex_process(FakeProcess* process);
void unlockMutex_process(FakeProcess* process);
FakeProcess* searchProcessByPid(ListHead* processes, FakePCB* pcb);