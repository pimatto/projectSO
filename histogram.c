#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <dirent.h> 
#include <string.h> 
#include <sys/stat.h>
#include <sys/types.h>

#define MAX_BURST 31
#define MAX_ARRIVAL 11
#define MAX_PRIORITY 5



// Funzione che calcola le probabilità a partire dall'istogramma (array contenente i conteggi dei valori)
void compute_probability(int *histogram, int total_count, float *probabilities, int size) {
    // Iterazione per ogni valore dell'istogramma (size, numero valori)
    for (int i = 0; i < size; i++) {
        // La probabilità è il conteggio dell'intervallo 'i' diviso per il conteggio totale
        // Se il conteggio totale è zero, imposta la probabilità a zero
        probabilities[i] = (total_count > 0) ? (float)histogram[i] / total_count : 0;
    }
}

// Funzione che legge una traccia di processo e aggiorna gli istogrammi
void read_trace_and_update_histogram(const char *filename, int *cpu_histogram, int *io_histogram, int *arrival_histogram, int *priority_histogram, int *cpu_count, int *io_count, int *arrival_count, int *priority_count) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Errore nell'apertura del file");
        return;
    }
    char line[15];
    
    int arrival_time, priority;
    
    // Lettura file riga per riga
    while (fgets(line, sizeof(line), file)){
        //Variabile che contiene i valori numerici letti
        int value;

        // Lettura del tempo di arrivo e della priorità se la riga contiene informazioni sul processo
        if (sscanf(line, "PROCESS %*d %d %d", &arrival_time, &priority) == 2) {
            // Aggiornamento dell'istogramma del tempo di arrivo se il valore è valido
            // e incremento del conteggio totale dei tempi di arrivo
            if (arrival_time >= 0 && arrival_time < MAX_ARRIVAL) {
                arrival_histogram[arrival_time]++;
                (*arrival_count)++;
            }
            // Aggiornamento dell'istogramma delle priorità se il valore è valido e incremento
            // del conteggio totale delle priorità
            if (priority >= 0 && priority < MAX_PRIORITY) {
                priority_histogram[priority]++;
                (*priority_count)++;
            }
        // Lettura del valore BURST se la riga contiene informazioni sul CPU BURST 
        } else if (sscanf(line, "CPU_BURST %d", &value) == 1) {
            // Aggiornamento dell'istogramma dei CPU burst se il valore è valido e
            // incremento del conteggio totale dei CPU BURST
            if (value >= 0 && value < MAX_BURST) {
                cpu_histogram[value]++;
                (*cpu_count)++;
            }
        // Lettura del valore BURST se la riga contiene informazioni sul IO BURST 
        } else if (sscanf(line, "IO_BURST %d", &value) == 1) {
            // Aggiornamento dell'istogramma dei IO burst se il valore è valido e
            // incremento del conteggio totale dei IO BURST
            if (value >= 0 && value < MAX_BURST) {
                io_histogram[value]++;
                (*io_count)++;
            }
        }
    }

    fclose(file);
}

// Funzione che legge tutte le tracce nella cartella "input_trace" e aggiorna gli istogrammi
void read_traces_from_folder(const char *foldername, int *cpu_histogram, int *io_histogram,
                             int *arrival_histogram, int *priority_histogram,
                             int *cpu_count, int *io_count, int *arrival_count, int *priority_count) {
    DIR *folder = opendir(foldername); 
    if (folder == NULL) {
        perror("Errore nell'apertura della cartella");
        return;
    }

    struct dirent *entry;
    char filepath[512];

    // Iterazione su tutti gli elementi della directory
    while ((entry = readdir(folder)) != NULL) {
        // Ignora le directory speciali "." e ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Verifica dell'estensione ".txt" (validità tracce)
        if (strstr(entry->d_name, ".txt") == NULL) {
            continue; // Salta file senza estensione ".txt"
        }

        // Costruzione del percorso completo del file
        snprintf(filepath, sizeof(filepath), "%s/%s", foldername, entry->d_name);

        // Lettura file e aggiornamento degli istogrammi
        read_trace_and_update_histogram(filepath, cpu_histogram, io_histogram,
                                        arrival_histogram, priority_histogram,
                                        cpu_count, io_count, arrival_count, priority_count);
    }

    closedir(folder);
}

// Funzione che calcola la distribuzione cumulativa a partire da probabilità (array di probabilità)
void compute_cumulative(float *probabilities, float *cumulative, int size){
    // Primo valore dell'array cumulativo uguale al primo valore delle probabilità
    cumulative[0] = probabilities[0];
    //printf("Cumulativa [0]: %.4f\n", cumulative[0]);
    // Iterazione dall'indice 1 a size-1
    for(int i=1; i<size; i++){
        // Ogni elemento cumulativo è la somma del valore precedente e della probabilità corrente
        cumulative[i] = cumulative[i-1] + probabilities[i];
    //    printf("Cumulativa [%d]: %.4f\n", i, cumulative[i]);
    }
}

// Funzione che genera un valore campionato a partire da una distribuzione cumulativa
// e un valore casuale
int sample_value(float *cumulative, int size){
    // Generazione numero casuale uniforme nell'intervallo [0, 1]
    double random_value = drand48();
    //printf("Valore casuale generato: %.4f\n", random_value);

    // Inizializzazione limite inferiore e superiore per la ricerca binaria
    int left = 0;
    int right = size-1;
    // Ricerca binaria
    // Continua a cercare finché la finestra di ricerca non si chiude
    while(left <= right){
        // Calcola l'indice centrale
        int mid = (left+right)/2;
        // Se il valore casuale è minore o uguale del valore cumulativo centrale
        if(random_value <= cumulative[mid]){
            // Se sono al primo elemento o il valore casuale è maggiore del valore cumulativo 
            // precedente sono nell'intervallo corretto
            if(mid == 0 || random_value > cumulative[mid-1]){
    //            printf("Valore campionato: %d\n", mid);
    //            printf("Valore pizzicato: %.4f\n\n", cumulative[mid]);
                return mid;
            }
            // Continua la ricerca spostando il limite superiore
            right = mid-1;
        // Se il valore casuale è maggiore del valore cumulativo centrale continua la ricerca 
        // spostando il limite inferiore
        } else{
            left = mid+1;
        }
    }

    // Restituzione dell'ultimo indice se non viene trovato altro
    return size;
}

//Funzione che genera fie di processi
void generate_trace(float *cpu_cumulative, float *io_cumulative,float *priority_cumulative, float *arrival_cumulative, int size, int num_bursts, int process_id, const char *filename){
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s", filename);
    
    FILE *file = fopen(filepath, "w");
    if(file == NULL){
        perror("Errore nell'apertura del file");
        return;
    }

    // Campionamento del tempo di arrivo del processo usando la distribuzione cumulativa
    int arrival_time = sample_value(arrival_cumulative, MAX_ARRIVAL);
    // Campionamento della priorità del processo usando la distribuzione cumulativa
    int priority = sample_value(priority_cumulative, MAX_PRIORITY);
    // Scrittura nel file dell'intestazione del processo con ID, del tempo di arrivo e della priorità
    fprintf(file, "PROCESS\t\t%d %d %d\n", process_id, arrival_time, priority);

    //Generazione BURST del processo
    for(int i = 0; i< num_bursts; i++){
        //Campionamento della durata del CPU BURST utilizzando la distribuzione cumulativa
        int cpu_burst = sample_value(cpu_cumulative, MAX_BURST);
        fprintf(file, "CPU_BURST\t%d\n", cpu_burst);
        
        // Campionamento della durata del IO BURST utilizzando la distribuzione cumulativa 
        // (se non è l'ultimo BURST)
        if(i != num_bursts-1){
            int io_burst = sample_value(io_cumulative, MAX_BURST);
            fprintf(file, "IO_BURST\t%d\n", io_burst);
        }
    }

    fclose(file);
}

//Funzione che genera file contenenti tracce dei processi utili alla simulazione di scheduling
void FakeProcess_generate(int cpu_count, int io_count, int arrival_count, int priority_count){
    int cpu_histogram[MAX_BURST] = {0};
    int io_histogram[MAX_BURST] = {0};
    int arrival_histogram[MAX_ARRIVAL] = {0};
    int priority_histogram[MAX_PRIORITY] = {0};

    float cpu_probabilities[MAX_BURST] = {0};
    float io_probabilities[MAX_BURST] = {0};
    float arrival_probabilities[MAX_ARRIVAL] = {0};
    float priority_probabilities[MAX_PRIORITY] = {0};

    // Lettura delle tracce dalla cartella "trace_input" e aggiornamento degli istogrammi
    //printf("Leggo i file di traccia dalla cartella 'trace_input'\n");
    const char *foldername = "../input_trace";
    read_traces_from_folder(foldername, cpu_histogram, io_histogram,
                            arrival_histogram, priority_histogram,
                            &cpu_count, &io_count, &arrival_count, &priority_count);

    // Verifica contatori diversi da zero (validità file)
    if (cpu_count == 0 || io_count == 0 || arrival_count == 0 || priority_count == 0) {
        printf("Errore: nessun dato valido trovato nei file di traccia.\n");
        return;
    }

    // Calcola le probabilità dagli istogrammi
    compute_probability(cpu_histogram, cpu_count, cpu_probabilities, MAX_BURST);
    compute_probability(io_histogram, io_count, io_probabilities, MAX_BURST);
    compute_probability(arrival_histogram, arrival_count, arrival_probabilities, MAX_ARRIVAL);
    compute_probability(priority_histogram, priority_count, priority_probabilities, MAX_PRIORITY);

    /*
    // Stampa le probabilità per CPU burst
    printf("\nProbabilità CPU burst:\n");
    for (int i = 0; i < MAX_BURST; i++) {
        if (cpu_probabilities[i] > 0) {
            printf("Valore %d: %.4f\n", i, cpu_probabilities[i]);
        }
    }

    // Stampa le probabilità per I/O burst
    printf("\nProbabilità I/O burst:\n");
    for (int i = 0; i < MAX_BURST; i++) {
        if (io_probabilities[i] > 0) {
            printf("Valore %d: %.4f\n", i, io_probabilities[i]);
        }
    }

    // Stampa le probabilità per il tempo di arrivo
    printf("\nProbabilità Tempo di Arrivo:\n");
    for (int i = 0; i < MAX_ARRIVAL; i++) {
        if (arrival_probabilities[i] > 0) {
            printf("Valore %d: %.4f\n", i, arrival_probabilities[i]);
        }
    }

    // Stampa le probabilità per la priorità
    printf("\nProbabilità Priorità:\n");
    for (int i = 0; i < MAX_PRIORITY; i++) {
        if (priority_probabilities[i] > 0) {
            printf("Valore %d: %.4f\n", i, priority_probabilities[i]);
        }
    }
    */

    //Creazione array per le distribuzioni cumulative calcolate
    float cpu_cumulative[MAX_BURST];
    float io_cumulative[MAX_BURST];
    float priority_cumulative[MAX_PRIORITY];
    float arrival_cumulative[MAX_ARRIVAL];

    //Calcolo della distribuzione cumulativa per CPU burst, I/O burst, priorità e tempo di arrivo
    //printf("\nCumulativa CPU Burst:\n");
    compute_cumulative(cpu_probabilities, cpu_cumulative, MAX_BURST);
    //printf("\nCumulativa IO Burst:\n");
    compute_cumulative(io_probabilities, io_cumulative, MAX_BURST);
    //printf("\nCumulativa priorità:\n");
    compute_cumulative(priority_probabilities, priority_cumulative, MAX_PRIORITY);
    //printf("\nCumulativa arrivi:\n");
    compute_cumulative(arrival_probabilities, arrival_cumulative, MAX_ARRIVAL);


    srand48(time(NULL));

    //Generazione traccia per 5 processi
    for (int i = 1; i <= 5; i++) {
        char filename[20];
        snprintf(filename, sizeof(filename), "p%d.txt", i);
        generate_trace(cpu_cumulative, io_cumulative, priority_cumulative, arrival_cumulative, MAX_BURST, 4, i, filename);
    //    printf("Traccia per il processo %d generata in '%s'.\n", i, filename);
    }
    
    //printf("Traccia generata con successo in 'p*.txt'.\n");
}
