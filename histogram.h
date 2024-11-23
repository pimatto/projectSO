#pragma once

void compute_probability(int *histogram, int total_count, float *probabilities, int size);
void read_trace_and_update_histogram(const char *filename, int *cpu_histogram, int *io_histogram, int *arrival_histogram, int *priority_histogram, int *cpu_count, int *io_count, int *arrival_count, int *priority_count);
void read_traces_from_folder(const char *foldername, int *cpu_histogram, int *io_histogram,
                             int *arrival_histogram, int *priority_histogram,
                             int *cpu_count, int *io_count, int *arrival_count, int *priority_count);
void compute_cumulative(float *probabilities, float *cumulative, int size);
int sample_value(float *cumulative, int size);
void generate_trace(float *cpu_cumulative, float *io_cumulative,float *priority_cumulative, float *arrival_cumulative, int size, int num_bursts, int process_id, const char *filename);
void FakeProcess_generate(int cpu_count, int io_count, int arrival_count, int priority_count);