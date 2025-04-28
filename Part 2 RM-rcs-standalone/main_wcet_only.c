#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define MAX_TASKS 10
#define MAX_JOBS 100
#define QUANTUM 1 

typedef struct {
    int id;
    int arrival;
    int wcet;
    int period;
} Task;

typedef struct {
    int task_id;
    int job_id;
    int release;
    int deadline;
    int remaining;
} Job;

typedef struct {
    int start;
    int end;
    int task_id;
    int job_id;
    int context_switch;
} ScheduleEntry;


Task tasks[MAX_TASKS];
Job jobs[MAX_JOBS];
ScheduleEntry schedule[MAX_JOBS*2];
int schedule_idx = 0;
int context_switches = 0;
int idle_time = 0;
int task_count = 0;
int job_count = 0;
int hyperperiod = 0;


int gcd(int a, int b) { return b == 0 ? a : gcd(b, a % b); }
int lcm(int a, int b) { return a * b / gcd(a, b); }

// Calculate hyperperiod
void calculate_hyperperiod() {
    hyperperiod = tasks[0].period;
    for (int i = 1; i < task_count; i++) {
        hyperperiod = lcm(hyperperiod, tasks[i].period);
    }
}


void generate_jobs() {
    job_count = 0;
    for (int i = 0; i < task_count; i++) {
        int num_jobs = (hyperperiod - tasks[i].arrival + tasks[i].period - 1) / tasks[i].period;
        for (int j = 0; j < num_jobs; j++) {
            if (tasks[i].arrival + j * tasks[i].period >= hyperperiod) continue;
            
            jobs[job_count].task_id = i + 1;
            jobs[job_count].job_id = j + 1;
            jobs[job_count].release = tasks[i].arrival + j * tasks[i].period;
            jobs[job_count].deadline = jobs[job_count].release + tasks[i].period;
            jobs[job_count].remaining = tasks[i].wcet;
            job_count++;
        }
    }
}


int has_higher_priority(int t1, int t2) {
    return tasks[t1-1].period < tasks[t2-1].period;
}

// Checking if extending the current job keeps all jobs schedulable
int is_extension_feasible(int current_job_idx, int current_time, int quantum) {
  
    Job sim_jobs[MAX_JOBS];
    for (int i = 0; i < job_count; i++) {
        sim_jobs[i] = jobs[i];
    }
    
    
    int extension = quantum;
    if (extension > sim_jobs[current_job_idx].remaining) {
        extension = sim_jobs[current_job_idx].remaining;
    }
    sim_jobs[current_job_idx].remaining -= extension;
    
    // Simulateing RM scheduling from current_time + extension to hyperperiod
    int time = current_time + extension;
    
    while (time < hyperperiod) {
      
        int next_job = -1;
        for (int i = 0; i < job_count; i++) {
            if (sim_jobs[i].release <= time && sim_jobs[i].remaining > 0) {
                if (next_job == -1 || has_higher_priority(sim_jobs[i].task_id, sim_jobs[next_job].task_id)) {
                    next_job = i;
                }
            }
        }
        
        if (next_job == -1) {
            
            int next_release = hyperperiod;
            for (int i = 0; i < job_count; i++) {
                if (sim_jobs[i].release > time && sim_jobs[i].release < next_release && 
                    sim_jobs[i].remaining > 0) {
                    next_release = sim_jobs[i].release;
                }
            }
            
            if (next_release == hyperperiod) break;
            time = next_release;
            continue;
        }
        
        // Find next release time
        int next_release = hyperperiod;
        for (int i = 0; i < job_count; i++) {
            if (sim_jobs[i].release > time && sim_jobs[i].release < next_release) {
                next_release = sim_jobs[i].release;
            }
        }
        
        // Execute job until next release or completion
        int execute_time;
        if (next_release == hyperperiod) {
            execute_time = sim_jobs[next_job].remaining;
        } else {
            execute_time = next_release - time;
            if (execute_time > sim_jobs[next_job].remaining) {
                execute_time = sim_jobs[next_job].remaining;
            }
        }
        
        time += execute_time;
        sim_jobs[next_job].remaining -= execute_time;
        
        // Check deadline
        if (sim_jobs[next_job].remaining == 0 && time > sim_jobs[next_job].deadline) {
            return 0; // Deadline missed
        }
    }
    
    return 1; // All jobs completed within deadlines
}


void simulate_rmrcs() {
    int current_time = 0;
    int current_job_idx = -1;
    
    while (current_time < hyperperiod) {
        
        int next_job_idx = -1;
        for (int i = 0; i < job_count; i++) {
            if (jobs[i].release <= current_time && jobs[i].remaining > 0) {
                if (next_job_idx == -1 || has_higher_priority(jobs[i].task_id, jobs[next_job_idx].task_id)) {
                    next_job_idx = i;
                }
            }
        }
        
        if (next_job_idx == -1) {
         
            int next_release = hyperperiod;
            for (int i = 0; i < job_count; i++) {
                if (jobs[i].release > current_time && jobs[i].release < next_release && 
                    jobs[i].remaining > 0) {
                    next_release = jobs[i].release;
                }
            }
            
            idle_time += (next_release - current_time);
            current_time = next_release;
            continue;
        }
        
      
        if (current_job_idx != -1 && current_job_idx != next_job_idx && 
            jobs[current_job_idx].remaining > 0 && 
            has_higher_priority(jobs[next_job_idx].task_id, jobs[current_job_idx].task_id)) {
            
            // Try to extend current job by QUANTUM
            if (is_extension_feasible(current_job_idx, current_time, QUANTUM)) {
                int extend_time = QUANTUM;
                if (extend_time > jobs[current_job_idx].remaining) {
                    extend_time = jobs[current_job_idx].remaining;
                }
                
                // Add to schedule
                schedule[schedule_idx].task_id = jobs[current_job_idx].task_id;
                schedule[schedule_idx].job_id = jobs[current_job_idx].job_id;
                schedule[schedule_idx].start = current_time;
                schedule[schedule_idx].end = current_time + extend_time;
                schedule[schedule_idx].context_switch = 0;
                schedule_idx++;
                
                
                jobs[current_job_idx].remaining -= extend_time;
                current_time += extend_time;
                
                continue;
            }
        }
        
        // Normal RM scheduling - context switch if task changes
        int context_switch = 0;
        if (current_job_idx != -1 && jobs[current_job_idx].task_id != jobs[next_job_idx].task_id) {
            context_switches++;
            context_switch = 1;
        }
        current_job_idx = next_job_idx;
        
        
        int next_release = hyperperiod;
        for (int i = 0; i < job_count; i++) {
            if (jobs[i].release > current_time && jobs[i].release < next_release) {
                next_release = jobs[i].release;
            }
        }
        
        int execute_time = jobs[current_job_idx].remaining;
        if (next_release < current_time + execute_time) {
            execute_time = next_release - current_time;
        }
        
        schedule[schedule_idx].task_id = jobs[current_job_idx].task_id;
        schedule[schedule_idx].job_id = jobs[current_job_idx].job_id;
        schedule[schedule_idx].start = current_time;
        schedule[schedule_idx].end = current_time + execute_time;
        schedule[schedule_idx].context_switch = context_switch;
        schedule_idx++;
        
        jobs[current_job_idx].remaining -= execute_time;
        current_time += execute_time;
    }
}


void optimize_schedule() {
    if (schedule_idx <= 1) return;
    
    int write_idx = 0;
    for (int read_idx = 1; read_idx < schedule_idx; read_idx++) {
        if (schedule[read_idx].task_id == schedule[write_idx].task_id &&
            schedule[read_idx].job_id == schedule[write_idx].job_id &&
            schedule[read_idx].start == schedule[write_idx].end) {
            
            schedule[write_idx].end = schedule[read_idx].end;
        } else {
         
            write_idx++;
            if (write_idx != read_idx) {
                schedule[write_idx] = schedule[read_idx];
            }
        }
    }
    
    schedule_idx = write_idx + 1;
}


void calculate_metrics(FILE* fp) {
    fprintf(fp, "Turnaround Times:\n");
    
    for (int t = 1; t <= task_count; t++) {
        float avg_turnaround = 0;
        int count = 0;
        
      
        for (int j = 1; j <= hyperperiod / tasks[t-1].period; j++) {
            int job_id = j;
            int release_time = tasks[t-1].arrival + (j-1) * tasks[t-1].period;
            int completion_time = -1;
            
           
            for (int i = 0; i < schedule_idx; i++) {
                if (schedule[i].task_id == t && schedule[i].job_id == job_id) {
                    if (completion_time < schedule[i].end) {
                        completion_time = schedule[i].end;
                    }
                }
            }
            
            if (completion_time != -1) {
                int turnaround = completion_time - release_time;
                fprintf(fp, "  T%d Job %d: %d\n", t, job_id, turnaround);
                avg_turnaround += turnaround;
                count++;
            }
        }
        
        if (count > 0) {
            fprintf(fp, "  Average for T%d: %.2f\n", t, avg_turnaround/count);
        }
    }
}


void print_schedule(char* filename) {
    optimize_schedule();
    
    FILE* fp = fopen(filename, "w");
    if (!fp) {
        printf("Error opening output file\n");
        return;
    }
    
    fprintf(fp, "Schedule (Hyperperiod: %d):\n", hyperperiod);
    fprintf(fp, "TaskJob | Start-End | Context Switch\n");
    
    for (int i = 0; i < schedule_idx; i++) {
        fprintf(fp, "T%dj%d | %d-%d", 
               schedule[i].task_id, schedule[i].job_id, 
               schedule[i].start, schedule[i].end);
        
        if (schedule[i].context_switch) {
            fprintf(fp, " | CS");
        }
        fprintf(fp, "\n");
    }
    
    fprintf(fp, "\nAnalysis:\n");
    fprintf(fp, "Total Context Switches: %d\n", context_switches);
    fprintf(fp, "Total Idle Time: %d\n", idle_time);
    
    calculate_metrics(fp);
    
    fclose(fp);
}

int main() {
    FILE* fp = fopen("tasks.txt", "r");
    if (!fp) {
        printf("Error opening tasks.txt\n");
        return 1;
    }
    
    // Read tasks
    while (fscanf(fp, "%d %d %d", &tasks[task_count].arrival, &tasks[task_count].wcet, &tasks[task_count].period) == 3) {
        tasks[task_count].id = task_count + 1;
        task_count++;
    }
    fclose(fp);
    
    calculate_hyperperiod();
    generate_jobs();
    simulate_rmrcs();
    print_schedule("schedule.txt");
    
    printf("Simulation complete. Results written to schedule.txt\n");
    return 0;
}
