#include "fake_process.h"
#include "linked_list.h"
#include <pthread.h>
#pragma once


typedef struct {
  ListItem list;
  int pid;
  
  ListHead events;
  pthread_mutex_t mutex;   //Mutex per il processo
  
  double predicted_burst;   //Valore predetto del burst
  double burst;             //Valore del burst attuale
  
  int readyTime;           //Variabile contenente il tempo in cui il PCB entra nella coda di ready
  int arrivalTime;

  int wait;                //Variabile contenente il tempo in attesa totale del processo
  int run;                 //Variabile contenente il tempo in esecuzione totale del processo
  int tat;                 //Variabile contenente il tempo impiegato per concludere il processo
  int wt;                  //Variabile contenente il tempo trascorso dal processo nella lista di ready

  int nominal_priority;   //Variabile contenente la priorità nominale del processo
  int time_priority;      //Variabile contenente la priorità affetta da aging del processo
  int aging;              //Variabile contenente il tempo che il processo ha trascorso nella propria coda di priorità

} FakePCB;

struct FakeOS;
typedef void (*ScheduleFn)(struct FakeOS* os, void* args);

typedef struct FakeOS{
  ListHead ready;
  ListHead waiting;
  int timer;
  ScheduleFn schedule_fn;
  void* schedule_args;

  ListHead processes;
  ListHead all_processes;

  ListHead cpu_list;          // Lista delle CPU

  ListHead* priority_queues;  //Lista delle code di priorità

  int max_priority;           //Variabile contenente la priorità massima trovata tra i vari processi

} FakeOS;

void FakeOS_init(FakeOS* os);
void FakeOS_simStep(FakeOS* os, int sel_cpu, float decay_coefficient);
void FakeOS_destroy(FakeOS* os);

int find_max_priority(FakeOS* os);


void initMutex_process(FakeProcess* process);
void lockMutex_process(FakeProcess* process);
void unlockMutex_process(FakeProcess* process);