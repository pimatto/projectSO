#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "fake_cpu.h"
#include "fake_os.h"




// Inizializzazione del mutex per una CPU
void initMutex_pcb(FakePCB* pcb) {
    pthread_mutex_init(&pcb->mutex, NULL);
}

// Acquisizione del mutex prima di eseguire un processo su una CPU
void lockMutex_pcb(FakePCB* pcb) {
    pthread_mutex_lock(&pcb->mutex);
}

// Rilascio del mutex dopo aver eseguito un processo su una CPU
void unlockMutex_pcb(FakePCB* pcb) {
    pthread_mutex_unlock(&pcb->mutex);
}

void FakeOS_init(FakeOS* os) {
  //os->running=0;
  List_init(&os->ready);
  List_init(&os->waiting);
  List_init(&os->processes);
  List_init(&os->cpu_list);
  os->timer=0;
  os->schedule_fn=0;
}

void FakeOS_createProcess(FakeOS* os, FakeProcess* p) {
  // sanity check
  assert(p->arrival_time==os->timer && "time mismatch in creation");
  // we check that in the list of PCBs there is no
  // pcb having the same pid
  //assert( (!os->running || os->running->pid!=p->pid) && "pid taken");

  ListItem* aux=os->ready.first;
  while(aux){
    FakePCB* pcb=(FakePCB*)aux;
    assert(pcb->pid!=p->pid && "pid taken");
    aux=aux->next;
  }

  aux=os->waiting.first;
  while(aux){
    FakePCB* pcb=(FakePCB*)aux;
    assert(pcb->pid!=p->pid && "pid taken");
    aux=aux->next;
  }

  // all fine, no such pcb exists
  FakePCB* new_pcb=(FakePCB*) malloc(sizeof(FakePCB));
  new_pcb->list.next=new_pcb->list.prev=0;
  new_pcb->pid=p->pid;
  new_pcb->events=p->events;
  ((FakeProcess*) new_pcb)->burst = 0.000000;
  ((FakeProcess*) new_pcb)->predicted_burst = 5.000000;
  //Inizializzo il mutex
  initMutex_pcb(new_pcb);


  assert(new_pcb->events.first && "process without events");

  // depending on the type of the first event
  // we put the process either in ready or in waiting
  ProcessEvent* e=(ProcessEvent*)new_pcb->events.first;
  switch(e->type){
  case CPU:
    List_pushBack(&os->ready, (ListItem*) new_pcb);
    break;
  case IO:
    List_pushBack(&os->waiting, (ListItem*) new_pcb);
    break;
  default:
    assert(0 && "illegal resource");
    ;
  }
}




void FakeOS_simStep(FakeOS* os, int sel_cpu){
  
  //Stampo il tempo attuale
  printf("\033[0;32m=====================================\033[0m\n");
  printf("\033[0;32m*********** TIME: %08d **********\033[0m\n", os->timer);
  printf("\033[0;32m=====================================\033[0m\n\n");
  
  //Scansione dei processi nella lista di waiting e creo tutti i processi partiti ora
  ListItem* aux=os->processes.first;
  if(aux){
    printf("=====================================\n");
    while (aux){
      FakeProcess* proc=(FakeProcess*)aux;
      FakeProcess* new_process=0;
      if (proc->arrival_time==os->timer){
        new_process=proc;
      }
      aux=aux->next;
      if (new_process) {
        printf("\tcreate pid:%d\n", new_process->pid);
        new_process=(FakeProcess*)List_detach(&os->processes, (ListItem*)new_process);
        FakeOS_createProcess(os, new_process);
        free(new_process);
      }
    }
    printf("=====================================\n\n");
  }

  //Scansione della lista di waiting e sposto nella lista di ready
  //tutti gli elementi i quali eventi (IO) sono terminati
  aux=os->waiting.first;
  if(aux)
    printf("=====================================\n");
  while(aux) {
    FakePCB* pcb=(FakePCB*)aux;
    aux=aux->next;
    ProcessEvent* e=(ProcessEvent*) pcb->events.first;
    printf("=====================================\n");
    printf("\twaiting pid: %d\n", pcb->pid);
    assert(e->type==IO);
    e->duration--;
    printf("\t\tremaining time:%d\n",e->duration);
    if (e->duration==0){
      unlockMutex_pcb(pcb);
      printf("\t\tend burst\n");
      List_popFront(&pcb->events);
      free(e);
      List_detach(&os->waiting, (ListItem*)pcb);
      if (!pcb->events.first) {
        //Termina il processo se non ci sono più eventi
        printf("\t\tend process\n");
        if (aux == os->waiting.last){
          printf("=====================================\n");
        }
        free(pcb);
      } else {
        //Gestisce il prossimo evento
        e=(ProcessEvent*) pcb->events.first;
        switch (e->type){
          case CPU:
            printf("\t\tmove to ready\n");
            //Aggiorno il predicted burst del processo al primo ingresso in CPU
            if(((FakeProcess*)pcb)->arrival_time == os->timer){
              printf("\n\nPredicted burst del processo %d: %f X %f [pid = %d] +  (1 - %f) X %f [pid = %d]\n\n", ((FakeProcess*)pcb)->pid, 0.5, ((FakeProcess*)pcb)->burst, ((FakeProcess*)pcb)->pid, 0.5, ((FakeProcess*)pcb)->predicted_burst, ((FakeProcess*)pcb)->pid);
              ((FakeProcess*)pcb)->predicted_burst = 0.5 * ((FakeProcess*)pcb)->predicted_burst + (1 - 0.5) * ((FakeProcess*)pcb)->burst;
            }
            printf("\t\tpredicted burst: %f\n", ((FakeProcess*)pcb)->predicted_burst);
            if (aux == os->waiting.last){
              printf("=====================================\n");
            }
            pcb->readyTime = os->timer;
            List_pushBack(&os->ready, (ListItem*) pcb);
            break;
          case IO:
            printf("\t\tmove to waiting\n");
            if (aux == os->waiting.last){
              printf("=====================================\n");
            }
            List_pushBack(&os->waiting, (ListItem*) pcb);
            break;
        }
      }
    } 
  }
  
  printf("-------------------------------------\n");

  

  //Itero per ogni CPU
  aux = os->cpu_list.first;
  while(aux){
    FakeCPU* cpu = (FakeCPU*) aux;
    aux = aux->next;

    //Se il processo in esecuzione sulla CPU ha pid == 0 viene settato a NULL
    if(cpu->running){
      if(cpu->running->pid == 0)
        cpu->running = NULL;
    }
    
    

    //printf("\033[0;30m");
    printf("CPU in utilizzo: ");
    for(int i = 1; i <= sel_cpu; i++){
      if(cpu->num_cpu == i){
        printf("\033[0;42m CPU%d \033[0;30m ", i);}
      else{
        printf("\033[0;47m CPU%d \033[0;30m ", i);
      }   
    }
    printf("\033[0m \n");

    //Chiama la funzione di scheduling se definita
    if (os->schedule_fn){
      printf("\n\navvio la funzione di scheduling...\n\n");
      (*os->schedule_fn)(os, os->schedule_args); 
    }


    //Decremento la durata del processo in running e se è finito lo distruggo
    //I processi sono rischedulati e se è l'ultimo evento, il processo in running viene distrutto
    FakePCB* running=cpu->running;
    if(cpu->running)
      printf("\n\nENTRO IO: %d\n\n", cpu->running->pid);
    printf("\trunning pid: %d\n", running?running->pid:-1);
    if (running) {
      lockMutex_pcb(running);
      if(running->events.first){
        ProcessEvent* e=(ProcessEvent*) running->events.first;
        assert(e->type==CPU);
        e->duration--;
        //Predicted burst del processo in esecuzione
        printf("\t\tpredicted burst: %f\n", ((FakeProcess*)running)->predicted_burst);
        //Aumento il tempo di burst ad ogni CPU BURST
        FakeProcess* runningProcess = (FakeProcess*) running;
        runningProcess->burst++;
        printf("\t\tremaining time:%d\n",e->duration);
        if (e->duration==0){
          printf("\t\tend burst\n");
          unlockMutex_pcb(running);
          List_popFront(&running->events);
          free(e);

          //Se non ci sono più eventi, il processo è terminato
          if (! running->events.first) {
            printf("\t\tend process\n");
            free(running); // kill process
          } 

          //Altrimenti gestisce il prossimo evento
          else {
            e=(ProcessEvent*) running->events.first;
            switch (e->type){
            case CPU:
              printf("\t\tmove to ready\n\t\tburst time del processo: %f\n", ((FakeProcess*)running)->burst);
              printf("\t\tpredicted burst: %f\n", ((FakeProcess*)running)->predicted_burst);
              //printf("\n\nPredicted burst del processo %d: %f X %f [pid = %d] +  (1 - %f) X %f [pid = %d]\n\n", ((FakeProcess*)running)->pid, 0.5, ((FakeProcess*)running)->burst, ((FakeProcess*)running)->pid, 0.5, ((FakeProcess*)running)->predicted_burst, ((FakeProcess*)running)->pid);
              //((FakeProcess*)running)->predicted_burst = 0.5 * ((FakeProcess*)running)->predicted_burst + (1 - 0.5) * ((FakeProcess*)running)->burst;
              //((FakeProcess*)running)->burst = 0;
              List_pushBack(&os->ready, (ListItem*) running);
              break;
            case IO:
              printf("\t\tmove to waiting\n\t\tburst time del processo: %f\n", ((FakeProcess*)running)->burst);
              printf("\n\nPredicted burst del processo %d: %f = %f X %f [pid = %d] +  (1 - %f) X %f [pid = %d]\n\n", ((FakeProcess*)running)->pid, 0.5 * ((FakeProcess*)running)->predicted_burst + (1 - 0.5) * ((FakeProcess*)running)->burst, 0.5, ((FakeProcess*)running)->burst, ((FakeProcess*)running)->pid, 0.5, ((FakeProcess*)running)->predicted_burst, ((FakeProcess*)running)->pid);
              ((FakeProcess*)running)->predicted_burst = 0.5 * ((FakeProcess*)running)->predicted_burst + (1 - 0.5) * ((FakeProcess*)running)->burst;
              ((FakeProcess*)running)->burst = 0;
              List_pushBack(&os->waiting, (ListItem*) running);
              break;
            }
          }
          cpu->running = 0;
          
        }
      }
    }

    //Chiamo la funzione di scheduling, se definita
    if (os->schedule_fn && !cpu->running && os->ready.first && ((FakeProcess*)os->ready.first)->predicted_burst < ((ProcessEvent*)((FakePCB*)os->ready.first)->events.first)->duration){
      (*os->schedule_fn)(os, os->schedule_args);
    }

    //Se non c'è un processo in esecuzione sulla CPU e la lista dei processi in ready non è vuota
    //metto il primo processo della lista ready in esecuzione sulla CPU
    if (! cpu->running && os->ready.first && ((FakePCB*)os->ready.first)->readyTime != os->timer){
      cpu->running=(FakePCB*) List_popFront(&os->ready);
    }
    
  }
  printf("=====================================\n\n");
  ++os->timer;
}



void FakeOS_destroy(FakeOS* os) {
}
