#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <termios.h>
#include "fake_os.h"
#include "fake_cpu.h"










//Struttura per memorizzare gli argomenti dello scheduler SJF
typedef struct{
  int quantum; //Lunghezza del quanto
  double alpha; //Fattore di previsione
} SchedSJFArgs; 


int running_cpu(FakeOS* os){
  ListItem* currentItem = os->cpu_list.first;
  int contatore = 0;
  while(currentItem){
    FakeCPU* cpu = (FakeCPU*) currentItem;
    if(cpu->running)
      return 1;
    currentItem = currentItem->next;
  }
  return 0;
}


//Scambia i processi da ordinare
void swapProcess(ListHead*list, ListItem* a, ListItem* b){
  printf("Scambio dei processi %d [%f] ->-->---> %d [%f]\n", ((FakeProcess*)a)->pid, ((FakeProcess*)a)->predicted_burst, ((FakeProcess*)b)->pid, ((FakeProcess*)b)->predicted_burst);
  if (a == b) // Se i due nodi sono gli stessi, non c'è bisogno di fare alcuna operazione
        return;

    // Trova il nodo precedente a 'a'
    ListItem* prev_a = NULL;
    ListItem* current = list->first;
    while (current && current != a) {
        prev_a = current;
        current = current->next;
    }

    // Trova il nodo precedente a 'b'
    ListItem* prev_b = NULL;
    current = list->first;
    while (current && current != b) {
        prev_b = current;
        current = current->next;
    }

    // Se uno dei nodi non è presente nella lista, esci dalla funzione
    if (!prev_a || !prev_b)
        return;

    // Se 'a' non è il primo nodo della lista, aggiorna il puntatore al nodo precedente
    if (prev_a != NULL)
        prev_a->next = b;
    else
        list->first = b;

    // Se 'b' non è il primo nodo della lista, aggiorna il puntatore al nodo precedente
    if (prev_b != NULL)
        prev_b->next = a;
    else
        list->first = a;

    // Scambia i puntatori al nodo successivo
    ListItem* temp = a->next;
    a->next = b->next;
    b->next = temp;
}

//Funzione per ordinare i processi in base al tempo di burst
void sortProcessesByBurstTime(ListHead* processes, int count) {
  if(processes->first){
    ListItem* head = processes->first;
    int swapped;

    for(int i = 0; i <= count; i++){
      head = processes->first;
      swapped = 0;

      for(int j = 0; j < count -i -1; j++){
        ListItem* p1 = head;
        ListItem* p2 = p1->next;
        if(((FakeProcess*)p1)->predicted_burst > ((FakeProcess*)p2)->predicted_burst){
          swapProcess(processes, p1, p2);
          swapped = 1;
        }

        head = head->next;
      }
    }   
  }
}




//Funzione di scheduling SJF
void schedSJFPREEMPTIVE(FakeOS* os, void* args_){
  SchedSJFArgs* args=(SchedSJFArgs*)args_;
  
  sortProcessesByBurstTime(&os->ready, os->ready.size);
  printf("\n\nRIORDINO I PROCESSI\n\n");
  printf("Processi nella coda di ready: ");
  ListItem* currentProc = os->ready.first;
  while (currentProc){
    printf("[PID = %d]\t", ((FakeProcess*)currentProc)->pid);
    currentProc = currentProc->next;
  }
  printf("\n\n");

  

  //MULTICPU
  //Itero per ogni CPU della lista contenuta in FakeOS
  ListItem* currentItem = os->cpu_list.first;
  while (currentItem){
    FakeCPU* cpu = (FakeCPU*) currentItem;
    currentItem = currentItem->next;
    lockMutex(cpu); //Aquisisce il mutex della CPU
    
    // look for the first process in ready
    // if none, return
    if (! os->ready.first){
      unlockMutex(cpu);
      return;
    }


    // Look at the first process in ready
    FakePCB* pcb = (FakePCB*)os->ready.first;

    // Avoid executing a process in the same epoch it's inserted into the ready queue
    if (pcb->readyTime == os->timer && pcb->readyTime != ((FakeProcess*)pcb)->arrival_time) {
        unlockMutex(cpu);
        continue;
    }

    //printf("numero CPU: %d\n\n", cpu->num_cpu);
    
    pcb = (FakePCB*) List_popFront(&os->ready);
    cpu->running = pcb;
      
    
    printf("\n\nPROCESSO SCELTO IN SCHED_SIM: %d\n\n", pcb->pid);
    
    
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
  }





}




















//Le API di termios vengono utilizzate per modificare temporaneamente la modalità di input 
//della tastiera in modo da consentire la lettura di un singolo carattere senza la necessità
//di confermare con Invio.




// Funzione per la lettura di un singolo carattere senza bisogno di premere Invio
char getch() {
    char buf = 0;
    struct termios old = {0};
    fflush(stdout);
    //Settaggio della nuova configurazione
    if (tcgetattr(0, &old) < 0)
        perror("tcsetattr()");
    old.c_lflag &= ~ICANON; //Disattivo la modalità canonica
    old.c_lflag &= ~ECHO; //Disattivo l'echo dei caratteri
    old.c_cc[VMIN] = 1;
    old.c_cc[VTIME] = 0;
    //Applico la nuova configurazione
    if (tcsetattr(0, TCSANOW, &old) < 0)
        perror("tcsetattr ICANON");
    //Legge un carattere dalla tastiera
    if (read(0, &buf, 1) < 0)
        perror ("read()");
    old.c_lflag |= ICANON; //Riattivo la modalità canonica
    old.c_lflag |= ECHO; //Riattivo la modalità canonica
    //Applico la nuova configurazione
    if (tcsetattr(0, TCSADRAIN, &old) < 0)
        perror ("tcsetattr ~ICANON");
    return buf;
}

// Funzione per disegnare la barra di configurazione
void drawBar(int numCPU, int selectedCPU) {
    printf("\nSeleziona il numero di CPU:\n");
    for (int i = 1; i <= numCPU; i++) {
        if (i == selectedCPU) {
            printf("\033[1;32m"); // Imposta il colore verde per la CPU selezionata
            printf("  \u25A0 CPU %d ", i); // Utilizza il carattere quadrato pieno Unicode per rappresentare la CPU selezionata
        } else {
            printf("\033[0m"); // Reimposta il colore di default
            printf("   \u25A1 CPU %d ", i); // Utilizza il carattere quadrato vuoto Unicode per rappresentare le CPU non selezionate
        }
        // Stampa una barra grafica per ogni CPU
        for (int j = 1; j <= 10; j++) {
            // Se la CPU è selezionata, stampa una barra piena, altrimenti una barra vuota
            if (j <= (i * 10) / numCPU) {
                printf("\u2588");
            } else {
                printf("-");
            }
        }
        printf("\n");
    }
    printf("\033[0m"); // Reimposta il colore di default alla fine
}








FakeOS os;

//Struttura per memorizzare gli argomenti dello scheduler RR
typedef struct {
  int quantum; //Lunghezza del quanto
} SchedRRArgs;

//Funzione di scheduling RR
void schedRR(FakeOS* os, void* args_){
  SchedRRArgs* args=(SchedRRArgs*)args_;
  
  
  //MULTICPU
  //Itero per ogni CPU della lista contenuta in FakeOS
  ListItem* currentItem = os->cpu_list.first;
  while (currentItem){
    FakeCPU* cpu = (FakeCPU*) currentItem;
    currentItem = currentItem->next;
    lockMutex(cpu); //Aquisisce il mutex della CPU
    // look for the first process in ready
    // if none, return
    if (! os->ready.first){
      unlockMutex(cpu);
      return;
    }

    FakePCB* pcb=(FakePCB*) List_popFront(&os->ready);
    cpu->running=pcb; //Assegnazione del processo alla CPU

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
  }
  


  /*ORIGINALE
  // look for the first process in ready
  // if none, return
  if (! os->ready.first)
    return;

  FakePCB* pcb=(FakePCB*) List_popFront(&os->ready);
  os->running=pcb;
  
  assert(pcb->events.first);
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
  }*/
};

int main(int argc, char** argv) {

  int numCPU = 10; //Numero massimo di CPU
  int selectedCPU = 1; //CPU selezionata inizialmente
  char c;
  //Disegna la barra iniziale
  drawBar(numCPU, selectedCPU);

  //Loop per la selezione del numero di CPU
  while(1) {
    c = getch(); //Leggi un singolo carattere senza bisogno di Invio
    //Codice per gestire le frecce direzionali e l'invio
    //Freccia giù e a destra: incrementa la CPU selezionata
    if (c == '\033') {
      getch(); //Leggi il secondo carattere della sequenza delle frecce
      c = getch(); //Leggi il terzo carattere della sequenza delle frecce
      if (c == 'B' || c == 'C') {
        selectedCPU = (selectedCPU == numCPU) ? 1 : selectedCPU + 1;
      }
      //Freccia su e a sinistra: decrementa la CPU selezionata
      else if (c == 'A' || c == 'D') {
        selectedCPU = (selectedCPU == 1) ? numCPU : selectedCPU - 1;
      }
    }
    //Invio: conferma la selezione e esce dal loop
    else if (c == '\n') {
      break;
    }

    //Pulisce lo schermo e ridisegna la barra con la nuova selezione
    system("clear");
    drawBar(numCPU, selectedCPU);
  }
  printf("\nHai selezionato %d CPU.\n", selectedCPU);

  
/*RR
  FakeOS_init(&os);
  SchedRRArgs srr_args;
  srr_args.quantum=5;
  os.schedule_args=&srr_args;
  os.schedule_fn=schedRR;
*/

//SJF DA CAMBIARE
  FakeOS_init(&os);
  SchedSJFArgs ssjf_args;
  ssjf_args.quantum=5;
  ssjf_args.alpha = 0.5;
  os.schedule_args=&ssjf_args;
  os.schedule_fn=schedSJFPREEMPTIVE;


  //Inizializzazione delle CPU
  int aux = 1;
  for(int i = 0; i < selectedCPU; i++){
    FakeCPU* cpu = (FakeCPU*)malloc(sizeof(FakeCPU));
    FakeCPU_init(cpu);
    cpu->num_cpu = aux;
    aux +=1;
    initMutex(cpu); //Inizializzazione del mutex per la CPU
    List_pushBack(&os.cpu_list, (ListItem*) cpu);
  }
  

  
  for (int i=1; i<argc; ++i){
    FakeProcess new_process;
    int num_events=FakeProcess_load(&new_process, argv[i]);
    printf("loading [%s], pid: %d, events:%d",
           argv[i], new_process.pid, num_events);
    if (num_events) {
      FakeProcess* new_process_ptr=(FakeProcess*)malloc(sizeof(FakeProcess));
      *new_process_ptr=new_process;
      List_pushBack(&os.processes, (ListItem*)new_process_ptr);
    }
  }
  printf("num processes in queue %d\n", os.processes.size);

  
  
  while(running_cpu(&os)
        || os.ready.first
        || os.waiting.first
        || os.processes.first){
    FakeOS_simStep(&os, selectedCPU);
  }

  //Deallocazione delle CPU
  ListItem* currentItem = os.cpu_list.first;
  while (currentItem) {
      ListItem* nextItem = currentItem->next;
      free(currentItem);
      currentItem = nextItem;
  }
}




