#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "fake_os.h"
#include "fake_cpu.h"


//Struttura per memorizzare gli argomenti dello scheduler SJF
typedef struct{
  int quantum; //Lunghezza del quanto
  double alpha; //Fattore di previsione
} SchedSJFArgs; 


//Funzione per ordinare i processi in base al tempo di burst
void sortProcessesByBurstTime(FakeOS* os) {
  //Conto il numero di processi all'interno della lista dei processi presa in argomento dalla 
  //funzione
  int count = 0;
  ListItem* processItem = os->processes.first;
  while(processItem){
    count++;
    FakeProcess* currentProcess = (FakeProcess*) processItem;
    processItem = processItem->next;
  }
  //Inizializzo l'array contenente i processi
  FakeProcess* arrayProcess[count];
  if(arrayProcess == NULL)
    printf("Errore allocazione memoria.");
    return;
  //Riempio l'array dei processi con i processi della lista
  processItem = os->processes.first;
  for (int i = 0; i < count; i++){
    FakeProcess* currentProcess = (FakeProcess*)processItem;
    arrayProcess[i] = currentProcess;
    processItem = processItem->next;
  }
  //Ordino l'array popolato in base al tempo di burst
  for (int i = 0; i < count - 1; i++){
    for (int j = 0; j < count - i - 1; j++){
      if (arrayProcess[j]->burst > arrayProcess[j + 1]->burst){
        // Scambia i due processi
        FakeProcess* temp = arrayProcess[j];
        arrayProcess[j] = arrayProcess[j + 1];
        arrayProcess[j + 1] = temp;
      }
    }
  }
  //Aggiorno la lista dei processi con l'array ordinato
  processItem = os->processes.first;
  for(int i = 0; i < count; i++){
    FakeProcess* currentProcess = (FakeProcess*)processItem;
    currentProcess->pid = arrayProcess[i]->pid;
    currentProcess->arrival_time = arrayProcess[i]->arrival_time;
    currentProcess->burst = arrayProcess[i]->burst;
    currentProcess->events = arrayProcess[i]->events;
    currentProcess->predicted_burst = arrayProcess[i]->predicted_burst;
    currentProcess->list = arrayProcess[i]->list;

    processItem = processItem->next;
  }
  //Libero la memoria allocata per l'array
  free(arrayProcess);
}




//Funzione per calcolare la previsione del prossimo burst (EXPONENTIAL AVERAGING)
double nextBurstPrediction(ListHead processes, float alpha){
  ListItem* processItem = processes.first;
  while (processItem){
    FakeProcess* currentProcess = (FakeProcess*) processItem;
    processItem = processItem->next;
    currentProcess->predicted_burst = currentProcess->burst;
  }
  ListItem* processItem = processes.first;
  FakeProcess* currentProcess = (FakeProcess*) processItem;
  //Inizializzo il predicted burst per il primo processo della lista
  currentProcess->predicted_burst = currentProcess->burst;
  //Itero la lista dei processi per calcolare il predicted burst per ogni processo
  while (processItem->next){
    //Calcolo il predicted burst del prossimo processo della lista attraverso il metodo
    //exponential averaging
    FakeProcess* nextProcess = (FakeProcess*) processItem->next;
    nextProcess->predicted_burst = alpha*currentProcess->burst + (1-alpha)*currentProcess->predicted_burst;
    FakeProcess* currentProcess = (FakeProcess*) processItem;
    processItem = processItem->next;
    
  }
}


//Funzione di scheduling SJF
void schedSJFPREEMPTIVE(FakeOS* os, void* args_){
  SchedSJFArgs* args=(SchedSJFArgs*)args_;
  // look for the first process in ready
  // if none, return
  if (! os->ready.first)
    return;

  //Calcolo il tempo di burst predetto con metodo exponential averaging
  nextBurstPrediction(os->processes, 0.5); //Alpha impostato a 0.5
  //Ciclo per l'esecuzione del SJF
  while(1){
    FakeProcess* shortestProcess = NULL; //Processo più breve
    float shortestBurstTime = 9999; //Variabile contenente il tempo di burst più breve
    //Ricerco il processo che ha il burst predetto più breve
    ListItem* currentItem = os->processes.first;
    FakeProcess* currentProcess = (FakeProcess*) currentItem;
    while(currentProcess){
      //Verifico se il processo è arrivato, se il burst predetto è minore del burst più breve
      //finora, e se il processo non è stato completato
      if(currentProcess->arrival_time <= os->timer && currentProcess->burst < shortestBurstTime){
        shortestProcess = currentProcess; //Memorizza il processo con burst più breve
        shortestBurstTime = currentProcess->burst; //Memorizza il burst più breve
      }
      currentItem = currentItem->next;
      FakeProcess* currentProcess = (FakeProcess*) currentItem;
    } 
    //Eseguo il processo con job più breve
    FakePCB* pcb=(FakePCB*) currentProcess;
    os->running=pcb;
    
        
    assert(pcb->events.first); //Assicura che il processo abbia eventi
    ProcessEvent* e = (ProcessEvent*)pcb->events.first;
    assert(e->type==CPU);
    // look at the first event
    // if duration>quantum
    // push front in the list of event a CPU event of duration quantum
    // alter the duration of the old event subtracting quantum
    if (e->duration>args->quantum) {
    ProcessEvent* qe=(ProcessEvent*)malloc(sizeof(ProcessEvent));
    qe->list.prev=qe->list.next=0;
    qe->type=CPU;
    qe->duration=args->quantum;
    e->duration-=args->quantum;
    List_pushFront(&pcb->events, (ListItem*)qe);
    }
  }
  







  /*
  //MULTICPU
  //Itero per ogni CPU della lista contenuta in FakeOS
  ListItem* currentItem = os->cpu_list.first;
  while (currentItem){
    FakeCPU* cpu = (FakeCPU*) currentItem;
    currentItem = currentItem->next;
    lockMutex(cpu); //Aquisisce il mutex della CPU
    // look for the first process in ready
    // if none, release the mutex and return
    if (! os->ready.first){
      unlockMutex(cpu);
      return;
    }
    
    //Inizializza il minimo burst con una durata stimata molto grande
    double min_predicted_burst = __DBL_MAX__;
    FakePCB* min_pcb = NULL;


    //Scansiono tutti i processi pronti per trovare quello con durata minore
    ListItem* item = os->ready.first;
    while (item){
        FakePCB* pcb = (FakePCB*) item;
        item = item->next;

        int pid = pcb->pid; //ID del pid del processo associato al relativo pcb
        //Cerco il processo nella lista dei processi del sistema attraverso il pid
        FakeProcess* process = NULL;
        ListItem* processItem = os->processes.first;
        while(processItem){
            FakeProcess* currentProcess = (FakeProcess*) processItem;
            processItem = processItem->next;
            if(currentProcess->pid == pid){
                process = currentProcess;
                break;
            }
        }

        //Se il processo non esiste, ignoro il pcb in questione
        if (!process)
            continue;



        //Calcolo la previsione del prossimo burst
        double next_burst = next_burst_prediction(process->burst, process->predicted_burst, args->alpha);
        process->burst += 1.0;

        //Aggiorno il minimo burst se trovo una durata stimata più breve
        if(next_burst < min_predicted_burst){
            min_predicted_burst = next_burst;
            min_pcb = pcb;
        }
        
        //Assegno il processo con la durata stimata più breve alla CPU
        cpu->running = min_pcb; //Assegnazione del proocesso alla CPU

        //Azzero il burst stimato dopo l'aumento del burst
        process->burst = 0; // Azzera il burst del processo dopo averlo eseguito
        process->predicted_burst = 0; // Azzera anche il burst stimato

        assert(pcb->events.first); //Assicura che il processo abbia eventi
        ProcessEvent* e = (ProcessEvent*)pcb->events.first;
        assert(e->type==CPU);
        // look at the first event
        // if duration>quantum
        // push front in the list of event a CPU event of duration quantum
        // alter the duration of the old event subtracting quantum
        if (e->duration>args->quantum) {
        ProcessEvent* qe=(ProcessEvent*)malloc(sizeof(ProcessEvent));
        qe->list.prev=qe->list.next=0;
        qe->type=CPU;
        qe->duration=args->quantum;
        e->duration-=args->quantum;
        List_pushFront(&pcb->events, (ListItem*)qe);
        }

    }
    

/*

    FakePCB* pcb=(FakePCB*) List_popFront(&os->ready);
    cpu->running=pcb; //Assegnazione del proocesso alla CPU

    assert(pcb->events.first); //Assicura che il processo abbia eventi
    ProcessEvent* e = (ProcessEvent*)pcb->events.first;
    assert(e->type==CPU);

    // look at the first event
    // if duration>quantum
    // push front in the list of event a CPU event of duration quantum
    // alter the duration of the old event subtracting quantum
    if (e->duration>args->quantum) {
      ProcessEvent* qe=(ProcessEvent*)malloc(sizeof(ProcessEvent));
      qe->list.prev=qe->list.next=0;
      qe->type=CPU;
      qe->duration=args->quantum;
      e->duration-=args->quantum;
      List_pushFront(&pcb->events, (ListItem*)qe);
    }
    unlockMutex(cpu); //Rilascia il mutex della CPU
  }*/
}