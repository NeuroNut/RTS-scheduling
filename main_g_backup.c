#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define MAX_TASKS 20
#define MAX_JOBS 1000

// Task structure
typedef struct {
    int arrival;
    int wcet;
    int period;
} Task;

// Job structure
typedef struct {
    int task_id;
    int job_id;
    int release_time;
    int deadline;
    int remaining_time;
    int period;
} Job;

// GCD function for LCM calculation
int gcd(int a, int b) {
    while (b != 0) {
        int temp = b;
        b = a % b;
        a = temp;
    }
    return a;
}

// LCM function
int lcm(int a, int b) {
    return a / gcd(a, b) * b;
}

// Comparison function for qsort
int compare_jobs(const void *a, const void *b) {
    Job *j1 = (Job *)a;
    Job *j2 = (Job *)b;
    return j1->period - j2->period; // Sort by period (ascending) for priority
}

int main() {
    FILE *input = fopen("tasks.txt", "r");
    FILE *output = fopen("schedule.txt", "w");
    if (!input || !output) {
        printf("Error opening files.\n");
        return 1;
    }

    // Read number of tasks
    int N;
    fscanf(input, "%d", &N);
    if (N <= 0 || N > MAX_TASKS) {
        printf("Invalid number of tasks.\n");
        fclose(input);
        fclose(output);
        return 1;
    }

    // Read tasks
    Task tasks[MAX_TASKS];
    for (int i = 0; i < N; i++) {
        if (fscanf(input, "%d %d %d", &tasks[i].arrival, &tasks[i].wcet, &tasks[i].period) != 3) {
            printf("Error reading task data.\n");
            fclose(input);
            fclose(output);
            return 1;
        }
    }
    fclose(input);

    // Compute hyperperiod
    int hyperperiod = tasks[0].period;
    for (int i = 1; i < N; i++) {
        hyperperiod = lcm(hyperperiod, tasks[i].period);
    }

    // Initialize data structures
    Job ready_queue[MAX_JOBS];
    int ready_count = 0;
    Job running_job;
    int is_running = 0;
    Job previous_job = {-1, -1, -1, -1, -1, -1}; // task_id = -1 indicates idle
    int context_switch_count = 0;
    int idle_time = 0;
    int start_time = 0;
    int finish_times[MAX_TASKS][MAX_JOBS];
    for (int i = 0; i < MAX_TASKS; i++) {
        for (int j = 0; j < MAX_JOBS; j++) {
            finish_times[i][j] = -1;
        }
    }

    // Simulation loop
    for (int t = 0; t < hyperperiod; t++) {
        // Add new jobs that arrive at time t
        for (int i = 0; i < N; i++) {
            if (t % tasks[i].period == tasks[i].arrival) {
                Job new_job = {
                    .task_id = i,
                    .job_id = t / tasks[i].period,
                    .release_time = t,
                    .deadline = t + tasks[i].period,
                    .remaining_time = tasks[i].wcet,
                    .period = tasks[i].period
                };
                ready_queue[ready_count++] = new_job;
            }
        }
    
        // Sort ready queue by priority (shortest period first)
        qsort(ready_queue, ready_count, sizeof(Job), compare_jobs);
    
        // Check if running job has finished
        if (is_running && running_job.remaining_time == 0) {
            finish_times[running_job.task_id][running_job.job_id] = t;
            is_running = 0;
        }
    
        // Scheduling decision
        if (is_running) {
            // Compute E and D for higher-priority ready jobs
            int E = 0;
            int D = INT_MAX;
            for (int j = 0; j < ready_count && ready_queue[j].period < running_job.period; j++) {
                E += ready_queue[j].remaining_time;
                if (ready_queue[j].deadline < D) {
                    D = ready_queue[j].deadline;
                }
            }
            if (E > 0 && t + E > D) {
                // Preempt: add running job back to queue if it has remaining time
                if (running_job.remaining_time > 0) {
                    ready_queue[ready_count++] = running_job;
                    qsort(ready_queue, ready_count, sizeof(Job), compare_jobs);
                }
                // Schedule the highest-priority job
                running_job = ready_queue[0];
                is_running = 1;
                // Remove it from ready queue
                for (int m = 0; m < ready_count - 1; m++) {
                    ready_queue[m] = ready_queue[m + 1];
                }
                ready_count--;
                // Log context switch
                if (previous_job.task_id != running_job.task_id || previous_job.job_id != running_job.job_id) {
                    context_switch_count++;
                    if (previous_job.task_id != -1) {
                        fprintf(output, "T%dj%d %d-%d\n", previous_job.task_id + 1, previous_job.job_id + 1, start_time, t);
                    }
                    start_time = t;
                    previous_job = running_job;
                }
            }
            // Else, continue running the current job
        } else if (ready_count > 0) {
            // No running job, schedule highest-priority ready job
            running_job = ready_queue[0];
            is_running = 1;
            for (int m = 0; m < ready_count - 1; m++) {
                ready_queue[m] = ready_queue[m + 1];
            }
            ready_count--;
            // Log context switch or start of new job
            if (previous_job.task_id != running_job.task_id || previous_job.job_id != running_job.job_id) {
                context_switch_count++;
                if (previous_job.task_id != -1) {
                    fprintf(output, "T%dj%d %d-%d\n", previous_job.task_id + 1, previous_job.job_id + 1, start_time, t);
                } else if (start_time < t) {
                    fprintf(output, "Idle %d-%d\n", start_time, t);
                }
                start_time = t;
                previous_job = running_job;
            }
        } else {
            // Idle
            if (previous_job.task_id != -1) {
                fprintf(output, "T%dj%d %d-%d\n", previous_job.task_id + 1, previous_job.job_id + 1, start_time, t);
                start_time = t;
                previous_job.task_id = -1;
            }
            idle_time++;
        }
    
        // Execute the running job
        if (is_running) {
            running_job.remaining_time--;
        }
    }
    // Output the last interval
    if (is_running) {
        fprintf(output, "T%dj%d %d-%d\n", running_job.task_id + 1, running_job.job_id + 1, start_time, hyperperiod);
    } else if (start_time < hyperperiod) {
        fprintf(output, "Idle %d-%d\n", start_time, hyperperiod);
    }

    // Analysis
    fprintf(output, "\nAnalysis:\n");
    fprintf(output, "Number of context switches: %d\n", context_switch_count);
    fprintf(output, "Total idle time: %d\n", idle_time);

    // Compute turnaround times
    double avg_turnaround[MAX_TASKS] = {0};
    int job_count[MAX_TASKS] = {0};
    for (int i = 0; i < N; i++) {
        int num_jobs = hyperperiod / tasks[i].period;
        double sum_turnaround = 0;
        int count = 0;
        for (int j = 0; j < num_jobs; j++) {
            if (finish_times[i][j] != -1) {
                int release = j * tasks[i].period + tasks[i].arrival;
                int turnaround = finish_times[i][j] - release;
                sum_turnaround += turnaround;
                count++;
            }
        }
        if (count > 0) {
            avg_turnaround[i] = sum_turnaround / count;
            job_count[i] = count;
        }
    }

    // Output average turnaround times
    for (int i = 0; i < N; i++) {
        if (job_count[i] > 0) {
            fprintf(output, "Task T%d: Average turnaround time = %.2f\n", i + 1, avg_turnaround[i]);
        }
    }

    fclose(output);
    printf("Simulation complete. Results written to schedule.txt\n");
    return 0;
}