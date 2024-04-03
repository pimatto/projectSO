#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <termios.h>
#include <math.h>
#include "fake_os.h"
#include "fake_cpu.h"










//Struttura per memorizzare gli argomenti dello scheduler SJF
typedef struct{
  int quantum; //Lunghezza del quanto
  double alpha; //Fattore di previsione
} SchedSJFArgs; 

//Funzione che verific se ci sono CPU in esecuzione
int running_cpu(FakeOS* os){
  ListItem* currentItem = os->cpu_list.first;
  while(currentItem){
    FakeCPU* cpu = (FakeCPU*) currentItem;
    //Se una CPU ha un processo inesecuzione restituisce 1
    if(cpu->running)
      return 1;
    currentItem = currentItem->next;
  }
  //Se nessuna CPU ha processi in esecuzione restituisce 0
  return 0;
}



//Funzione che restituisce il processo con il burst più breve nella coda dei processi pronti
FakePCB* shortestJobPCB(FakeOS* os, int count) {
  //Se non ci sono processi nella coda di ready restituisce NULL
  if(!os->ready.first)
    return NULL;

  //Iniziallizza il PCB relativo al processo più breve con il primo processo della coda di ready
  FakePCB* shortestJobItem = (FakePCB*) os->ready.first;
  //Inizializza il tempo di burst più breve con un valore abbastanza grande
  int shortestJob = 9999;
  //Itero attraverso la lista dei processi pronti
  ListItem* currentItem = os->ready.first;
  while(currentItem){
    FakePCB* pcb = (FakePCB*) currentItem;
    currentItem = currentItem->next;
    //Se il tempo di burst del processo attuale è più breve del tempo di burst più breve trovato 
    //finora, e il processo non è passato dalla coda di waiting a quella di ready durante questa epoca
    //di tempo, aggiorna il processo più breve e il suo tempo di burst
    if(((ProcessEvent*)pcb->events.first)->duration < shortestJob && (pcb->readyTime != os->timer || os->timer == 0)){
      shortestJob = ((ProcessEvent*)shortestJobItem->events.first)->duration;
      shortestJobItem = pcb;
    }
  }

  //Restituisce il PCB con la durata di burst minore trovato
  return shortestJobItem;
}



//Algoritmo di scheduling SJF preemptive
void schedSJFPREEMPTIVE(FakeOS* os, void* args_){
  SchedSJFArgs* args=(SchedSJFArgs*)args_;

  //Itera attraverso la lista delle CPU
  ListItem* currentItem = os->cpu_list.first;
  while (currentItem){
    FakeCPU* cpu = (FakeCPU*) currentItem;
    currentItem = currentItem->next;
    lockMutex(cpu); //Aquisisce il mutex della CPU
    
    //Se non ci sono processi nella coda di ready rilascia il mutex e ritorna
    if (! os->ready.first){
      unlockMutex(cpu);
      return;
    }

    //Inizializza il pcb con il primo processo della coda di ready
    FakePCB* pcb = (FakePCB*)os->ready.first;
    
    //Evita di eseguire un processo nella stessa epoca in cui viene inserito nella coda di ready
    //se non si tratta dell'epoca in cui il processo viene creato
    if (pcb->readyTime == os->timer && pcb->readyTime != ((FakeProcess*)pcb)->arrival_time) {
      assert(pcb->events.first); //Assicura che il processo abbia eventi
      ProcessEvent* e = (ProcessEvent*)pcb->events.first;
      assert(e->type==CPU);

      //Assegna al quanto di tempo il valore del burst predetto
      args->quantum = (int) floor(((FakeProcess*)pcb)->predicted_burst);
      
      //Se la durata dell'evento è maggiore del quantum, creo un nuovo evento con durata quantum
      //e aggiorno la durata dell'evento originale
      if (e->duration>args->quantum) {
        ProcessEvent* qe=(ProcessEvent*)malloc(sizeof(ProcessEvent));
        qe->list.prev=qe->list.next=0;
        qe->type=CPU;
        qe->duration=args->quantum;
        e->duration-=args->quantum;
        List_pushFront(&pcb->events, (ListItem*)qe);
      }
      
      unlockMutex(cpu);
      continue;
    }

    //Se c'è un processo in esecuzione sulla CPU, lo toglie alla CPU reinserisce nella coda dei
    //processi pronti
    if (cpu->running) {
      List_pushFront(&os->ready, (ListItem*)cpu->running);
      cpu->running = NULL;
    }

  

    //Assegno al PCB il processo con durata di burst più breve
    pcb = shortestJobPCB(os, os->ready.size);
    if(pcb){
      List_detach(&os->ready, (ListItem*) pcb);
      cpu->running = pcb;
    }

    assert(pcb->events.first); //Assicura che il processo abbia eventi
    ProcessEvent* e = (ProcessEvent*)pcb->events.first;
    assert(e->type==CPU);

    //Assegna al quanto di tempo il valore del burst predetto
    args->quantum = (int) floor(((FakeProcess*)pcb)->predicted_burst);

    //Se la durata dell'evento è maggiore del quantum, creo un nuovo evento con durata quantum
    //e aggiorno la durata dell'evento originale
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



// Funzione per stampare il testo con effetto "hacker"
void writtenPrint(const char *text) {
  const char *ptr = text;
  while (*ptr != '\0') {
      putchar(*ptr++);
      fflush(stdout);
      usleep(20000); // Aggiungi un piccolo ritardo per uno stile distinto
  }
  putchar('\n');
}

// Funzione per disabilitare l'input dalla tastiera
void disableKeyboardInput() {
    struct termios term;
    tcgetattr(STDIN_FILENO, &term);
    term.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &term);
}

// Funzione per abilitare l'input dalla tastiera
void enableKeyboardInput() {
    struct termios term;
    tcgetattr(STDIN_FILENO, &term);
    term.c_lflag |= ICANON | ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &term);
}



int main(int argc, char** argv) {

  system("clear");

  //Disabilita l'input dalla tastiera
  disableKeyboardInput();

  //Avvio del sistema...
  writtenPrint("\n[SYSTEM BOOTING UP...]\n");

  //Stampa il nome dello scheduler con uno stile accattivante
  printf("---------------------------------------------------\n");
  writtenPrint("    >> Shortest Job First Preemptive Scheduler <<    ");
  printf("---------------------------------------------------\n\n");

  //Sistema pronto
  writtenPrint("[SYSTEM READY]");

  //Riabilita l'input dalla tastiera
  enableKeyboardInput();


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
  printf("\nHai selezionato %d CPU.\n\n", selectedCPU);


  //Variabile per memorizzare il decay coefficient inserito dall'utente
  float decay_coefficient;

  //Impostazione del valore di default del decay coefficient
  float default_decay_coefficient = 0.5;

  //Richiesta all'utente di inserire il valore del decay coefficient
  char input[100]; //Buffer per memorizzare l'input dell'utente

  printf("Inserisci il decay coefficient (valore compreso tra 0 e 1). Premi invio per default (%f): ", default_decay_coefficient);
  fgets(input, sizeof(input), stdin);

  //Verifica se l'utente ha premuto solo Invio
  if (input[0] == '\n') {
    decay_coefficient = default_decay_coefficient; // Utilizza il valore di default
  } else {
    sscanf(input, "%f", &decay_coefficient); // Altrimenti, converte la stringa inserita dall'utente in float
  }

  //Verifica che il valore sia compreso tra 0 e 1
  if (decay_coefficient < 0 || decay_coefficient > 1) {
    printf("Il valore del decay coefficient deve essere compreso tra 0 e 1.\n");
    return 1; // Esci dal programma con codice di errore
  }

  printf("\nHai selezionato %f come valore del decay coefficient.\n\n", decay_coefficient);
  
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
  ssjf_args.quantum=7;
  ssjf_args.alpha = decay_coefficient;
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
    printf("loading [%s], pid: %d, events:%d\n",
           argv[i], new_process.pid, num_events);
    if (num_events) {
      FakeProcess* new_process_ptr=(FakeProcess*)malloc(sizeof(FakeProcess));
      *new_process_ptr=new_process;
      List_pushBack(&os.processes, (ListItem*)new_process_ptr);
    }
  }
  printf("number of processes in queue %d\n\n", os.processes.size);

  


  while(running_cpu(&os)
        || os.ready.first
        || os.waiting.first
        || os.processes.first){
    FakeOS_simStep(&os, selectedCPU, decay_coefficient);
  }

  printf("\n\nFINAL STATS:\n");
  
  ListItem* currentItem = os.all_processes.first;
  while(currentItem){
    if(((FakePCB*)currentItem)->pid != 0){
      ((FakePCB*)currentItem)->wt = ((FakePCB*)currentItem)->tat-((FakePCB*)currentItem)->run-((FakePCB*)currentItem)->wait;
      printf("process pid: [%d]\t\tIO/BURST: %d\t\tCPU/BURST: %d\n\t\t\tturnaround time: %d\t\twaiting time: %d\n\n", ((FakePCB*)currentItem)->pid, ((FakePCB*)currentItem)->wait, ((FakePCB*)currentItem)->run, ((FakePCB*)currentItem)->tat, ((FakePCB*)currentItem)->wt);
    }
    currentItem = currentItem->next;
  }




  //Deallocazione delle CPU
  currentItem = os.cpu_list.first;
  while (currentItem) {
      ListItem* nextItem = currentItem->next;
      free(currentItem);
      currentItem = nextItem;
  }
}




