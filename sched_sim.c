#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <termios.h>
#include <math.h>
#include "fake_os.h"
#include "fake_cpu.h"


//Definizione dei codici per i colori di testo e di sfondo (ANSI ESCAPE)
#define ANSI_COLOR_BOLD    "\x1b[1m"
#define ANSI_COLOR_WHITE   "\x1b[47m"
#define ANSI_COLOR_RESET   "\x1b[0m"
#define ANSI_COLOR_BLACK   "\x1b[30m"
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW   "\x1b[33m"



FakeOS os;

//Struttura per memorizzare gli argomenti dello scheduler SJF
typedef struct{
  int quantum;        //Lunghezza del quanto
  double alpha;       //Fattore di previsione
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
  float shortestJob = 9999.000000;
  //Itero attraverso la lista dei processi pronti
  ListItem* currentItem = os->ready.first;
  while(currentItem){
    FakePCB* pcb = (FakePCB*) currentItem;
    currentItem = currentItem->next;
    //Se il tempo di burst del processo attuale è più breve del tempo di burst più breve trovato 
    //finora, e il processo non è passato dalla coda di waiting a quella di ready durante questa epoca
    //di tempo, aggiorna il processo più breve e il suo tempo di burst
    
    //float aux = pcb->predicted_burst-pcb->burst;
    //printf("\ndevo scegliere: %d --> %f\n", pcb->pid, aux);
    if(pcb->predicted_burst < shortestJob && (pcb->readyTime != os->timer || os->timer == 0)){
      shortestJob = pcb->predicted_burst;
      shortestJobItem = pcb;
      //printf("\n\nscelgo quello minore: %d --> %f\n\n", shortestJobItem->pid, shortestJob);
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
    if (pcb->readyTime == os->timer && pcb->readyTime != pcb->arrivalTime) {
      assert(pcb->events.first); //Assicura che il processo abbia eventi
      ProcessEvent* e = (ProcessEvent*)pcb->events.first;
      assert(e->type==CPU);

      //Assegna al quanto di tempo il valore del burst predetto
      args->quantum = (int) floor(pcb->predicted_burst);
      
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
    args->quantum = (int) floor(pcb->predicted_burst);

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
};




















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
    printf("\n"ANSI_COLOR_WHITE ANSI_COLOR_BOLD ANSI_COLOR_BLACK"Select the number of CPUs:\n\n"ANSI_COLOR_RESET"\n");
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





// Funzione per stampare il testo con effetto
void writtenPrint(const char *text) {
  const char *ptr = text;
  while (*ptr != '\0') {
    putchar(*ptr++);
    fflush(stdout);
    usleep(20000); //Aggiungi un piccolo ritardo
  }
  
}

// Funzione per stampare il testo con effetto più lento
void writtenPrints(const char *text) {
  const char *ptr = text;
  while (*ptr != '\0') {
    putchar(*ptr++);
    fflush(stdout);
    usleep(45000); //Aggiungi un piccolo ritardo
  }
  
}

//Funzione per disabilitare l'input dalla tastiera
void disableKeyboardInput() {
  struct termios term;
  tcgetattr(STDIN_FILENO, &term);
  term.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &term);
}

//Funzione per abilitare l'input dalla tastiera
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
  writtenPrints("\n"ANSI_COLOR_BLACK ANSI_COLOR_BOLD ANSI_COLOR_YELLOW"[SYSTEM BOOTING UP...]"ANSI_COLOR_RESET"\n\n");

  

  //Stampa il nome dello scheduler con uno stile accattivante
  printf("------------------------------------------------------------------------\n");
  writtenPrint("\t    >> Shortest Job First Preemptive Scheduler <<    \n");
  printf("------------------------------------------------------------------------\n\n");

  //Sistema pronto
  writtenPrints("\n"ANSI_COLOR_BLACK ANSI_COLOR_BOLD ANSI_COLOR_GREEN"[SYSTEM READY]"ANSI_COLOR_RESET"\n\n\n");

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

  //Disabilita l'input dalla tastiera
  disableKeyboardInput();

  writtenPrint("\n\nYou have selected ");
  printf("%d", selectedCPU);
  writtenPrint(" CPU.\n\n");


  //Variabile per memorizzare il decay coefficient inserito dall'utente
  float decay_coefficient;

  //Impostazione del valore di default del decay coefficient
  float default_decay_coefficient = 0.5;

  //Richiesta all'utente di inserire il valore del decay coefficient
  char input[100]; //Buffer per memorizzare l'input dell'utente

  printf(ANSI_COLOR_WHITE ANSI_COLOR_BOLD ANSI_COLOR_BLACK"\n\nEnter the decay coefficient (value between 0 and 1). Press enter by default (%f):"ANSI_COLOR_RESET" ", default_decay_coefficient);

  //Riabilita l'input dalla tastiera
  enableKeyboardInput();

  fgets(input, sizeof(input), stdin);

  //Verifica se l'utente ha premuto solo Invio
  if (input[0] == '\n') {
    decay_coefficient = default_decay_coefficient; // Utilizza il valore di default
  } else {
    sscanf(input, "%f", &decay_coefficient); // Altrimenti, converte la stringa inserita dall'utente in float
  }

  //Verifica che il valore sia compreso tra 0 e 1
  if (decay_coefficient < 0 || decay_coefficient > 1) {
    writtenPrint("The value of the decay coefficient must be between 0 and 1.\n\n");
    return 1; // Esci dal programma con codice di errore
  }

  //Disabilita l'input dalla tastiera
  disableKeyboardInput();

  writtenPrint("\nYou have selected ");
  printf("%f", decay_coefficient);
  writtenPrint(" as the value of the decay coefficient.\n\n");
  
  
  
  //Inizio della simulazione...
  writtenPrints("\n"ANSI_COLOR_BLACK ANSI_COLOR_BOLD ANSI_COLOR_RED"[START SIMULATION...]\n\n"ANSI_COLOR_RESET"\n");

  //SJF PREEMPTIVE
  FakeOS_init(&os);
  SchedSJFArgs ssjf_args;
  ssjf_args.quantum=7;
  ssjf_args.alpha = decay_coefficient;
  os.schedule_args=&ssjf_args;
  os.schedule_fn=schedSJFPREEMPTIVE;


  //Inizializzazione delle CPU
  int aux = 1;
  for(int i = 0; i < selectedCPU; i++){
    //Allocazione di una nuova CPU
    FakeCPU* cpu = (FakeCPU*)malloc(sizeof(FakeCPU));

    //Inizializzazione della CPU
    FakeCPU_init(cpu);
    
    //Impostazione del numero della CPU
    cpu->num_cpu = aux;
    
    //Aggiunta della CPU alla lista
    List_pushBack(&os.cpu_list, (ListItem*)cpu);
    
    aux += 1;
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
  writtenPrint("\n\nNumber of processes in queue ");
  printf("%d", os.processes.size);
  writtenPrint("\n\n\n\n\n");
  writtenPrints(ANSI_COLOR_BLACK ANSI_COLOR_BOLD ANSI_COLOR_GREEN"[EXECUTION... ... ... ...]"ANSI_COLOR_RESET);
  


  while(running_cpu(&os)
        || os.ready.first
        || os.waiting.first
        || os.processes.first){
    FakeOS_simStep(&os, selectedCPU, decay_coefficient);
  }

  //Fine della simulazione...
  writtenPrints("\n\n\n"ANSI_COLOR_BLACK ANSI_COLOR_BOLD ANSI_COLOR_RED"[END OF SIMULATION...]"ANSI_COLOR_RESET"\n");

  //Stampa delle statistiche
  writtenPrints("\n\n\n"ANSI_COLOR_BLACK ANSI_COLOR_BOLD ANSI_COLOR_GREEN"[FINAL REPORT...]"ANSI_COLOR_RESET"\n");

  printf("\n\n+----------------------------------------------------------------------+\n");
  printf("|" ANSI_COLOR_WHITE ANSI_COLOR_BOLD "                                                                      " ANSI_COLOR_RESET "|\n");
  printf("|" ANSI_COLOR_WHITE ANSI_COLOR_BOLD ANSI_COLOR_BLACK "                           FINAL STATISTICS                           " ANSI_COLOR_RESET "|\n");
  printf("|" ANSI_COLOR_WHITE ANSI_COLOR_BOLD "                                                                      " ANSI_COLOR_RESET "|\n");
  printf("+----------------------------------------------------------------------+\n");



  printf("\n\n\n------------------------------------------------------------------------\n\n");
  printf(ANSI_COLOR_BLACK ANSI_COLOR_BOLD ANSI_COLOR_YELLOW"Total time of the simulation:"ANSI_COLOR_RESET" \t\t\t\t\t"ANSI_COLOR_BLACK ANSI_COLOR_BOLD ANSI_COLOR_RED"%d"ANSI_COLOR_RESET"\n\n", os.timer);
  printf(ANSI_COLOR_BLACK ANSI_COLOR_BOLD ANSI_COLOR_YELLOW"Number of processes in the simulation:"ANSI_COLOR_RESET" \t\t\t\t"ANSI_COLOR_BLACK ANSI_COLOR_BOLD ANSI_COLOR_RED"%d"ANSI_COLOR_RESET"\n\n", os.all_processes.size);
  printf(ANSI_COLOR_BLACK ANSI_COLOR_BOLD ANSI_COLOR_YELLOW"Number of CPUs in the simulation:"ANSI_COLOR_RESET" \t\t\t\t"ANSI_COLOR_BLACK ANSI_COLOR_BOLD ANSI_COLOR_RED"%d"ANSI_COLOR_RESET"\n\n", os.cpu_list.size);
  printf("\n------------------------------------------------------------------------\n\n");

  writtenPrint(ANSI_COLOR_WHITE ANSI_COLOR_BOLD ANSI_COLOR_BLACK"Total process burst (CPU and IO burst):"ANSI_COLOR_RESET"\n\n");
  ListItem* currentItem = os.all_processes.first;
  printf("\t\t\t\t\tCPU-BURST\t\tIO-BURST\n\n");
  while(currentItem){
    if(((FakePCB*)currentItem)->pid != 0)
      printf("\tProcess pid [%d]\t\t\t%d\t\t\t%d\n", ((FakePCB*)currentItem)->pid, ((FakePCB*)currentItem)->run, ((FakePCB*)currentItem)->wait);
    currentItem = currentItem->next;
  }
  printf("\n------------------------------------------------------------------------\n\n");
  writtenPrint(ANSI_COLOR_WHITE ANSI_COLOR_BOLD ANSI_COLOR_BLACK"Turnaround time (time to complete a process):"ANSI_COLOR_RESET"\n\n");
  currentItem = os.all_processes.first;
  float total_tat = 0;
  while(currentItem){
    if(((FakePCB*)currentItem)->pid != 0){
      ((FakePCB*)currentItem)->tat++;
      total_tat += ((FakePCB*)currentItem)->tat;
      printf("\tprocess pid [%d]\t\t\t\t\t\t%d\n", ((FakePCB*)currentItem)->pid, ((FakePCB*)currentItem)->tat);
    }
    currentItem = currentItem->next;
  }
  printf("\n\t"ANSI_COLOR_BLACK ANSI_COLOR_BOLD ANSI_COLOR_YELLOW"Average Turnaround Time:"ANSI_COLOR_RESET"\t\t\t\t"ANSI_COLOR_BLACK ANSI_COLOR_BOLD ANSI_COLOR_RED"%0.3f"ANSI_COLOR_RESET"\n", total_tat/os.all_processes.size);
  printf("\n------------------------------------------------------------------------\n\n");
  writtenPrint(ANSI_COLOR_WHITE ANSI_COLOR_BOLD ANSI_COLOR_BLACK"Waiting time (time a process has been waiting in the ready queue):"ANSI_COLOR_RESET"\n\n");
  currentItem = os.all_processes.first;
  float total_wt = 0;
  while(currentItem){
    if(((FakePCB*)currentItem)->pid != 0){
      ((FakePCB*)currentItem)->wt = ((FakePCB*)currentItem)->tat-((FakePCB*)currentItem)->run-((FakePCB*)currentItem)->wait;
      total_wt += ((FakePCB*)currentItem)->wt;
      printf("\tprocess pid [%d]\t\t\t\t\t\t%d\n", ((FakePCB*)currentItem)->pid, ((FakePCB*)currentItem)->wt);
    }
    currentItem = currentItem->next;
  }
  printf("\n\t"ANSI_COLOR_BLACK ANSI_COLOR_BOLD ANSI_COLOR_YELLOW"Average Waiting Time:"ANSI_COLOR_RESET"\t\t\t\t\t"ANSI_COLOR_BLACK ANSI_COLOR_BOLD ANSI_COLOR_RED"%0.3f"ANSI_COLOR_RESET"\n", total_wt/os.all_processes.size);
  printf("\n------------------------------------------------------------------------\n\n");
  writtenPrint(ANSI_COLOR_WHITE ANSI_COLOR_BOLD ANSI_COLOR_BLACK"CPU utilization (fraction of the time the CPU is used by the process):"ANSI_COLOR_RESET"\n\n");
  currentItem = os.cpu_list.first;
  float total_usage = 0;
  while(currentItem){
    total_usage += (((FakeCPU*)currentItem)->usage/(os.timer))*100;
    printf("\tCPU ID [%d]\t\t\t\t\t\t%0.2f%%\n", ((FakeCPU*)currentItem)->num_cpu, (((FakeCPU*)currentItem)->usage/(os.timer))*100);
    currentItem = currentItem->next;
  }
  printf("\n\t"ANSI_COLOR_BLACK ANSI_COLOR_BOLD ANSI_COLOR_YELLOW"Average CPU Utilization:"ANSI_COLOR_RESET"\t\t\t\t"ANSI_COLOR_BLACK ANSI_COLOR_BOLD ANSI_COLOR_RED"%0.3f%%"ANSI_COLOR_RESET"\n", total_usage/os.cpu_list.size);

  printf("\n------------------------------------------------------------------------\n\n");
  float throughput = (float)(os.all_processes.size)/(float)(os.timer);
  printf(ANSI_COLOR_WHITE ANSI_COLOR_BOLD ANSI_COLOR_BLACK"Throughput (number of processes that complete in total time):"ANSI_COLOR_RESET"\t"ANSI_COLOR_BLACK ANSI_COLOR_BOLD ANSI_COLOR_RED"%f"ANSI_COLOR_RESET"\n", throughput);
  printf("\n------------------------------------------------------------------------\n\n\n\n");

  writtenPrints("\n"ANSI_COLOR_BLACK ANSI_COLOR_BOLD ANSI_COLOR_RED"[SYSTEM SHUTDOWN...]"ANSI_COLOR_RESET"\n\n\n");

  //Riabilita l'input dalla tastiera
  enableKeyboardInput();


  FakeOS_destroy(&os);
}




