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
  List_init(&os->ready);
  List_init(&os->waiting);
  List_init(&os->processes);
  List_init(&os->cpu_list);
  List_init(&os->all_processes);
  os->timer=0;
  os->schedule_fn=0;
  os->max_priority = -1;
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
  new_pcb->arrivalTime = p->arrival_time;
  new_pcb->wait = 0;
  new_pcb->run = 0;
  new_pcb->tat = 0;
  new_pcb->wt = 0;

  new_pcb->burst = 0.000000;
  new_pcb->predicted_burst = 10.000000;
  //Inizializzo il mutex
  initMutex_pcb(new_pcb);

  new_pcb->nominal_priority = p->nominal_priority;
  new_pcb->time_priority = p->nominal_priority;
  new_pcb->aging = 0;


  assert(new_pcb->events.first && "process without events");

  // depending on the type of the first event
  // we put the process either in ready or in waiting
  ProcessEvent* e=(ProcessEvent*)new_pcb->events.first;
  switch(e->type){
  case CPU:
    //Se lo scheduler utilizza code di priorità, inserisce il PCB nella coda relativa alla
    //sua priorità
    if(os->max_priority != -1){
      List_pushBack(&os->priority_queues[new_pcb->nominal_priority], (ListItem*) new_pcb);
    }
    //Altrimenti il PCB viene inserito nell'unica coda di ready
    else
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




void FakeOS_simStep(FakeOS* os, int sel_cpu, float decay_coefficient){
  
  //Stampa il tempo attuale
  printf("\033[0;32m+-----------------------------------------+\033[0m\n");
  printf("\033[0;32m|              TIME: %08d             |\033[0m\n", os->timer);
  printf("\033[0;32m+-----------------------------------------+\033[0m\n\n");
  
  //Scansione dei processi e creazione dei processi appena arrivati
  ListItem* aux=os->processes.first;
  if(aux){
    printf("+-----------------------------------------+\n");
    while (aux){
      FakeProcess* proc=(FakeProcess*)aux;
      FakeProcess* new_process=0;
      if (proc->arrival_time==os->timer){
        new_process=proc;
      }
      aux=aux->next;
      if (new_process) {
        printf("|              create pid: %d              |\n", new_process->pid);
        new_process=(FakeProcess*)List_detach(&os->processes, (ListItem*)new_process);
        FakeOS_createProcess(os, new_process);
        free(new_process);
      }
    }
    printf("+-----------------------------------------+\n\n");
  }


  //Scansione della lista di waiting e spostamento dei processi nella lista di ready
  //quando i loro eventi (IO) terminano
  aux=os->waiting.first;
  if(aux)
    printf("+-----------------------------------------+\n");
  while(aux) {
    FakePCB* pcb=(FakePCB*)aux;
    aux=aux->next;
    ProcessEvent* e=(ProcessEvent*) pcb->events.first;
    printf("|             waiting pid: %d              |\n", pcb->pid);
    assert(e->type==IO);
    e->duration--;

    //Aumento del tempo di attesa totale del processo
    pcb->wait++;

    printf("|\t\tremaining time: %d",e->duration);
    
    if(e->duration < 10)
      printf("         |\n");
    else
      printf("        |\n");
    if(e->duration != 0 && aux !=NULL){
      printf("|-----------------------------------------|\n");
    }else if(e->duration != 0 && aux ==NULL){
      printf("+-----------------------------------------+\n");
    }
    if (e->duration==0){
      unlockMutex_pcb(pcb);
      printf("|\t\tend burst                 |\n");
      List_popFront(&pcb->events);
      free(e);
      List_detach(&os->waiting, (ListItem*)pcb);
      if (!pcb->events.first) {
        printf("|\t\tpredicted burst: %f |\n", decay_coefficient * pcb->predicted_burst + (1 - decay_coefficient) * pcb->burst);
            
        //Termina il processo se non ci sono più eventi
        printf("|\t\tend process               |\n");
        pcb->tat = os->timer-pcb->arrivalTime;
        List_pushBack(&os->all_processes, (ListItem*)pcb);
        //free(pcb);
      } else {
        //Gestisce il prossimo evento
        e=(ProcessEvent*) pcb->events.first;
        switch (e->type){
          case CPU:
            printf("|\t\tmove to ready             |\n");
            if(pcb->predicted_burst<10.000000)
              printf("|\t\tpredicted burst: %f |\n", pcb->predicted_burst);  
            else
              printf("|\t\tpredicted burst: %f|\n", pcb->predicted_burst);  
            if(aux != NULL){
              printf("|-----------------------------------------|\n");
            }else{
              printf("+-----------------------------------------+\n");
            }
            pcb->readyTime = os->timer;
            //Se lo scheduler utilizza code di priorità, inserisce il PCB nella coda relativa alla
            //sua priorità
            if(os->max_priority != -1)
              List_pushBack(&os->priority_queues[pcb->nominal_priority], (ListItem*) pcb);
            //Altrimenti il PCB viene inserito nell'unica coda di ready
            else
              List_pushBack(&os->ready, (ListItem*) pcb);
            break;
          case IO:
            printf("|\t\tmove to waiting           |\n");
            if (aux != NULL){
              printf("|-----------------------------------------|\n");
            }else{
              printf("+-----------------------------------------+\n");
            }
            List_pushBack(&os->waiting, (ListItem*) pcb);
            break;
        }
      }
    } 
  }
  
  printf("\n+-----------------------------------------+\n");



  //Itera per ogni CPU
  aux = os->cpu_list.first;
  while(aux){
    FakeCPU* cpu = (FakeCPU*) aux;
    aux = aux->next;

    //Se il processo in esecuzione sulla CPU ha pid == 0 viene settato a NULL
    if(cpu->running){
      if(cpu->running->pid == 0)
        cpu->running = NULL;
    }
    
    //Stampa l'ID della CPU in utilizzo
    printf("| CPU in utilizzo: ");
    for(int i = 1; i <= sel_cpu; i++){
      if(cpu->num_cpu == i && i != 10)
        printf("\033[0;42m CPU%d \033[0;30m ", i);
      else if(cpu->num_cpu == i && i == 10)
        printf("\033[0;42m CPU%d \033[0;30m", i);
    }
    printf("\033[0m                |\n");

    //Chiama la funzione di scheduling se definita
    if (os->schedule_fn){
      (*os->schedule_fn)(os, os->schedule_args); 
    }
    

    //Gestisce il processo in esecuzione sulla CPU:
    //decrementa la durata del processo in running e se è finito lo distrugge.
    //I processi sono rischedulati e se è l'ultimo evento, il processo in running viene distrutto
    FakePCB* running=cpu->running;
    if(running && running->pid < 10)
      printf("|\trunning pid: %d                    |\n", running->pid);
    else
      printf("|\trunning pid: %d                   |\n", running?running->pid:-1);
    if (running) {
      lockMutex_pcb(running);
      if(running->events.first){
        ProcessEvent* e=(ProcessEvent*) running->events.first;
        assert(e->type==CPU);
        e->duration--;

        //Aumento l'utilizzo della CPU
        cpu->usage++;

        if(running->readyTime == os->timer-1)
          running->wt--;

        //Aumento del tempo di esecuzione totale del processo
        running->run++;
        //Se lo scheduler utilizza code di priorità, viene stampata la priorità del PCB
        if(os->max_priority != -1)
          printf("|\t\tpriority: %d\t\t  |\n", running->time_priority);

        //Predicted burst del processo in esecuzione
        if(e->duration != 0){
          if(running->predicted_burst<10.000000)
            printf("|\t\tpredicted burst: %f |\n", running->predicted_burst);
          else
            printf("|\t\tpredicted burst: %f|\n", running->predicted_burst);
        }
        //Aumento il tempo di burst ad ogni CPU BURST
        running->burst++;
        printf("|\t\tremaining time:%d",e->duration);
        if(e->duration < 10)
          printf("          |\n");
        else
          printf("         |\n");
        if(e->duration != 0 && aux !=NULL){
          printf("|-----------------------------------------|\n");
        }else if(e->duration != 0 && aux ==NULL){
          printf("+-----------------------------------------+\n");
        }
        if (e->duration==0){
          printf("|\t\tend burst                 |\n");
          unlockMutex_pcb(running);
          List_popFront(&running->events);
          free(e);

          //Azzeramento tempo di attesa nella coda di ready di appertenza e reimpostazione
          //della priorità al valore di default
          running->aging = 0;
          running->time_priority = running->nominal_priority;

          //Se non ci sono più eventi, il processo è terminato
          if (! running->events.first) {
            if((decay_coefficient * running->predicted_burst + (1 - decay_coefficient) * running->burst)<10.000000)
              printf("|\t\tpredicted burst: %f |\n", decay_coefficient * running->predicted_burst + (1 - decay_coefficient) * running->burst);
            else
              printf("|\t\tpredicted burst: %f|\n", decay_coefficient * running->predicted_burst + (1 - decay_coefficient) * running->burst);
            printf("|\t\tend process               |\n");
            if (aux != NULL)
              printf("|-----------------------------------------|\n");
            else
              printf("+-----------------------------------------+\n");
            running->tat = os->timer-running->arrivalTime;
            List_pushBack(&os->all_processes, (ListItem*)running);
            //free(running); // kill process
          } 

          //Altrimenti gestisce il prossimo evento
          else {
            e=(ProcessEvent*) running->events.first;
            switch (e->type){
            case CPU:
              printf("|\t\tmove to ready             |\n");
              if(running->burst<10)
                printf("|\t\tburst time: %f      |\n", running->burst);
              else
                printf("|\t\tburst time: %f     |\n", running->burst);
              if(running->predicted_burst<10.000000)
                printf("|\t\tpredicted burst: %f |\n", running->predicted_burst);
              else
                printf("|\t\tpredicted burst: %f|\n", running->predicted_burst);
              
              //printf("|\t\tpredicted burst: %f |\n", running->predicted_burst);
              if(&os->priority_queues[0])
                List_pushBack(&os->priority_queues[running->nominal_priority], (ListItem*) running);
              else
                List_pushBack(&os->ready, (ListItem*) running);
              if (aux != NULL)
                printf("|-----------------------------------------|\n");
              else
                printf("+-----------------------------------------+\n");
              break;
            case IO:
              printf("|\t\tmove to waiting           |\n");
              if(running->burst<10.000000)
                printf("|\t\tburst time: %f      |\n", running->burst);
              else
                printf("|\t\tburst time: %f     |\n", running->burst);
              if(running->predicted_burst<10.000000)
                printf("|\t\tpredicted burst %f |\n", decay_coefficient * running->predicted_burst + (1 - decay_coefficient) * running->burst);
              else
                printf("|\t\tpredicted burst %f |\n", decay_coefficient * running->predicted_burst + (1 - decay_coefficient) * running->burst);
              running->predicted_burst = decay_coefficient * running->predicted_burst + (1 - decay_coefficient) * running->burst;
              running->burst = 0;
              List_pushBack(&os->waiting, (ListItem*) running);
              if (aux != NULL)
                printf("|-----------------------------------------|\n");
              else
                printf("+-----------------------------------------+\n");
              break;
            }
          }
          cpu->running = 0;
        }
      }
    }else if(!running && aux != NULL){
      printf("|-----------------------------------------|\n");
    }else{
      printf("+-----------------------------------------+\n");
    }

    //Chiamo la funzione di scheduling, se definita
    if (os->schedule_fn && !cpu->running && os->ready.first && ((FakePCB*)os->ready.first)->predicted_burst < ((ProcessEvent*)((FakePCB*)os->ready.first)->events.first)->duration){
      (*os->schedule_fn)(os, os->schedule_args);
    }

    //Se non c'è un processo in esecuzione sulla CPU e la lista dei processi in ready non è vuota
    //metto il primo processo della lista ready in esecuzione sulla CPU
    if (! cpu->running && os->ready.first && ((FakePCB*)os->ready.first)->readyTime != os->timer){
      cpu->running=(FakePCB*) List_popFront(&os->ready);
    }
  }
  if(os->max_priority == -1){
    printf("ready processes:\t");
    aux = os->ready.first;
    if(!aux)
      printf("-");
    while (aux){
      printf("[%d] ", ((FakePCB*)aux)->pid);
      aux = aux->next;
    }
    printf("\n\n");
  } 
  //Se vengono utilizzate code di priorità, se il tempo nella rispettiva coda di un PCB è pari a 10
  //la priorità del processo viene aumentata di 1, inserendolo così nella giusta coda
  else{
    for(int i = 0; i <= os->max_priority; i++){
      printf("ready processes in queue with priority %d:\t", i);
      aux = os->priority_queues[i].first;
      if(!aux)
        printf(" -");
      while (aux){
        if(((FakePCB*)aux)->readyTime != os->timer)
          ((FakePCB*)aux)->aging ++;
        if(((FakePCB*)aux)->aging == 10){
          List_detach(&os->priority_queues[((FakePCB*)aux)->time_priority], aux);
          ((FakePCB*)aux)->aging = 0;
          if(((FakePCB*)aux)->time_priority > 0)
            ((FakePCB*)aux)->time_priority --;
          else 
            ((FakePCB*)aux)->time_priority = 0;
          
          List_pushBack(&os->priority_queues[((FakePCB*)aux)->time_priority], aux);
          
        }
        printf("[%d (%d)] ", ((FakePCB*)aux)->pid, ((FakePCB*)aux)->aging);
        aux = aux->next;
      }
      printf("\n");
    }
    printf("\n");
  }

  printf("pending processes:\t");
  aux = os->waiting.first;
  if(!aux)
    printf("-");
  while (aux){
    printf("[%d] ", ((FakePCB*)aux)->pid);
    aux = aux->next;
  }
  printf("\n");


  
  ++os->timer;
}



void FakeOS_destroy(FakeOS* os) {

  // Libera i processi della lista contenente tutti i processi
  ListItem* currentItem = os->all_processes.first;
  while (currentItem) {
    FakePCB* pcb = (FakePCB*)currentItem;
    ListItem* nextItem = currentItem->next;
    free(pcb); // Liberare l'elemento della lista e il suo contenuto
    currentItem = nextItem;
  }

  // Deallocazione delle CPU
  currentItem = os->cpu_list.first;
  while (currentItem) {
    // Effettua il cast dell'elemento corrente a FakeCPU
    FakeCPU* cpu = ((FakeCPU*)currentItem);
    ListItem* nextItem = currentItem->next;
    
    // Dealloca l'elemento corrente
    free(cpu);
    
    // Avanza all'elemento successivo nella lista
    currentItem = nextItem;
  }

  //Libera lo spazio delle code di ready allocate se sono state utilizzate code di priorità
  for(int i = 0; i <= os->max_priority; i++){
    free(&os->priority_queues[i]);
  }
}
