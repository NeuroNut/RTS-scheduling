#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define MAX_TASKS 10
#define MAX_JOBS 100
#define QUANTUM 0.5  

typedef struct {
    int id;
    int arrival;
    int wcet;
    int period;
    float actual;
} Task;

typedef struct {
    int task_id;
    int job_id;
    int release;
    int deadline;
    float remaining;  // Actual remaining time
    int wcet;         // Original WCET
    int first_job;    // Flag for first job
} Job;

typedef struct {
    float start;
    float end;
    int task_id;
    int job_id;
    int context_switch;
} ScheduleEntry;

Task tasks[MAX_TASKS];
Job jobs[MAX_JOBS];
ScheduleEntry schedule[MAX_JOBS*2];
int schedule_idx = 0;
int context_switches = 0;
float idle_time = 0;
int task_count = 0;
int job_count = 0;
int hyperperiod = 0;

int gcd(int a, int b) { return b == 0 ? a : gcd(b, a % b); }
int lcm(int a, int b) { return a * b / gcd(a, b); }

void calculate_hyperperiod() {
    hyperperiod = tasks[0].period;
    for (int i = 1; i < task_count; i++) {
        hyperperiod = lcm(hyperperiod, tasks[i].period);
    }
}

void generate_jobs() {
    job_count = 0;
    for (int i = 0; i < task_count; i++) {
        int num_jobs = hyperperiod / tasks[i].period;
        for (int j = 0; j < num_jobs; j++) {
            int release = tasks[i].arrival + j * tasks[i].period;
            if (release >= hyperperiod) continue;
            
            jobs[job_count].task_id = i + 1;
            jobs[job_count].job_id = j + 1;
            jobs[job_count].release = release;
            jobs[job_count].deadline = release + tasks[i].period;
            jobs[job_count].wcet = tasks[i].wcet;
            
            // First job uses WCET, subsequent jobs use actual time
            if (j == 0) {
                jobs[job_count].remaining = tasks[i].wcet;
                jobs[job_count].first_job = 1;
            } else {
                jobs[job_count].remaining = tasks[i].actual;
                jobs[job_count].first_job = 0;
            }
            
            job_count++;
        }
    }
}

int has_higher_priority(int t1, int t2) {
    return tasks[t1-1].period < tasks[t2-1].period;
}

// Checking if extending the current job by a specific amount is feasible
int is_extension_feasible(int current_job_idx, float current_time, float extension_time) {

    Job sim_jobs[MAX_JOBS];
    for (int i = 0; i < job_count; i++) {
        sim_jobs[i] = jobs[i];
    }
    
    
    sim_jobs[current_job_idx].remaining -= extension_time;
    if (sim_jobs[current_job_idx].remaining < -0.001) return 0; // Over-execution
    
    
    float time = current_time + extension_time;
    
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
          
            float next_release = hyperperiod;
            for (int i = 0; i < job_count; i++) {
                if (sim_jobs[i].release > time && sim_jobs[i].release < next_release) {
                    next_release = sim_jobs[i].release;
                }
            }
            
            if (next_release == hyperperiod) break;
            time = next_release;
            continue;
        }
        
       
        if (next_job != current_job_idx && sim_jobs[current_job_idx].remaining > 0 &&
            has_higher_priority(sim_jobs[next_job].task_id, sim_jobs[current_job_idx].task_id)) {
            float E = 0;
            float D = hyperperiod;
            for (int i = 0; i < job_count; i++) {
                if (sim_jobs[i].release <= time && sim_jobs[i].remaining > 0 &&
                    has_higher_priority(sim_jobs[i].task_id, sim_jobs[current_job_idx].task_id)) {
                    E += sim_jobs[i].remaining;
                    if (sim_jobs[i].deadline < D) {
                        D = sim_jobs[i].deadline;
                    }
                }
            }
            if (E > 0 && time + E <= D) {
                // Extend current job
                float slack = D - (time + E);
                float ext_time = fmin(sim_jobs[current_job_idx].remaining, slack + E);
                if (ext_time > 0.001) {
                    time += ext_time;
                    sim_jobs[current_job_idx].remaining -= ext_time;
                    continue;
                }
            }
        }
        
        // Execute highest-priority job
        float exec_time = sim_jobs[next_job].remaining;
        float next_event = hyperperiod;
        for (int i = 0; i < job_count; i++) {
            if (sim_jobs[i].release > time && sim_jobs[i].release < next_event) {
                next_event = sim_jobs[i].release;
            }
            if (sim_jobs[i].deadline > time && sim_jobs[i].deadline < next_event) {
                next_event = sim_jobs[i].deadline;
            }
        }
        
        if (next_event != hyperperiod) {
            exec_time = fmin(exec_time, next_event - time);
        }
        
        time += exec_time;
        sim_jobs[next_job].remaining -= exec_time;
        
        // Check deadline
        if (time > sim_jobs[next_job].deadline && sim_jobs[next_job].remaining > 0) {
            return 0; // Deadline missed
        }
    }
    
    // Check remaining jobs
    for (int i = 0; i < job_count; i++) {
        if (sim_jobs[i].remaining > 0 && sim_jobs[i].deadline <= hyperperiod) {
            return 0; // Incomplete job
        }
    }
    
    return 1; // Feasible schedule
}


float find_max_extension(int current_job_idx, float current_time) {
   
    float E = 0;
    float D = hyperperiod;
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].release <= current_time && jobs[i].remaining > 0 &&
            has_higher_priority(jobs[i].task_id, jobs[current_job_idx].task_id)) {
            E += jobs[i].remaining;
            if (jobs[i].deadline < D) {
                D = jobs[i].deadline;
            }
        }
    }
    
   
    float max_ext = jobs[current_job_idx].remaining;
    if (E > 0) {
        float slack = D - (current_time + E);
        if (slack <= 0) return 0; 
        max_ext = fmin(max_ext, slack); 
    }
    
   
    float min_ext = 0;
    float best_ext = 0;
    
    while (max_ext - min_ext > 0.001) {
        float mid_ext = (min_ext + max_ext) / 2;
        if (is_extension_feasible(current_job_idx, current_time, mid_ext)) {
            best_ext = mid_ext;
            min_ext = mid_ext;
        } else {
            max_ext = mid_ext;
        }
    }
    
    return best_ext;
}

void simulate_rmrcs() {
    float current_time = 0;
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
            
            float next_release = hyperperiod;
            for (int i = 0; i < job_count; i++) {
                if (jobs[i].release > current_time && jobs[i].release < next_release) {
                    next_release = jobs[i].release;
                }
            }
            
            if (next_release != hyperperiod) {
                float idle_duration = next_release - current_time;
                idle_time += idle_duration;
                schedule[schedule_idx].task_id = 0; // Idle
                schedule[schedule_idx].job_id = 0;
                schedule[schedule_idx].start = current_time;
                schedule[schedule_idx].end = next_release;
                schedule[schedule_idx].context_switch = current_job_idx != -1;
                if (current_job_idx != -1) context_switches++;
                schedule_idx++;
                current_time = next_release;
            } else {
                current_time = hyperperiod;
            }
            current_job_idx = -1;
            continue;
        }
        
        // Try to extend lower priority job (RM-RCS core)
        if (current_job_idx != -1 && current_job_idx != next_job_idx && 
            jobs[current_job_idx].remaining > 0 && 
            has_higher_priority(jobs[next_job_idx].task_id, jobs[current_job_idx].task_id)) {
            
         
            float max_ext = find_max_extension(current_job_idx, current_time);
            
            if (max_ext > 0.001) {
                
                schedule[schedule_idx].task_id = jobs[current_job_idx].task_id;
                schedule[schedule_idx].job_id = jobs[current_job_idx].job_id;
                schedule[schedule_idx].start = current_time;
                schedule[schedule_idx].end = current_time + max_ext;
                schedule[schedule_idx].context_switch = 0;
                schedule_idx++;
                
                jobs[current_job_idx].remaining -= max_ext;
                current_time += max_ext;
                continue;
            }
        }
        
        // Regular RM scheduling
        int context_switch = 0;
        if (current_job_idx != -1 && jobs[current_job_idx].task_id != jobs[next_job_idx].task_id) {
            context_switches++;
            context_switch = 1;
        }
        
        current_job_idx = next_job_idx;
        float exec_time = jobs[current_job_idx].remaining;
        
        // Find next event (release or deadline)
        float next_event = hyperperiod;
        for (int i = 0; i < job_count; i++) {
            if (jobs[i].release > current_time && jobs[i].release < next_event) {
                next_event = jobs[i].release;
            }
            if (jobs[i].deadline > current_time && jobs[i].deadline < next_event) {
                next_event = jobs[i].deadline;
            }
        }
        
        if (next_event != hyperperiod) {
            exec_time = fmin(exec_time, next_event - current_time);
        }
        
        if (exec_time > 0.001) {
            schedule[schedule_idx].task_id = jobs[current_job_idx].task_id;
            schedule[schedule_idx].job_id = jobs[current_job_idx].job_id;
            schedule[schedule_idx].start = current_time;
            schedule[schedule_idx].end = current_time + exec_time;
            schedule[schedule_idx].context_switch = context_switch;
            schedule_idx++;
            
            jobs[current_job_idx].remaining -= exec_time;
            current_time += exec_time;
        }
        
        if (jobs[current_job_idx].remaining <= 0.001) {
            current_job_idx = -1; // Job completed
        }
    }
}

void optimize_schedule() {
    if (schedule_idx <= 1) return;
    
    int write_idx = 0;
    for (int read_idx = 1; read_idx < schedule_idx; read_idx++) {
        if (schedule[read_idx].task_id == schedule[write_idx].task_id &&
            schedule[read_idx].job_id == schedule[write_idx].job_id &&
            fabs(schedule[read_idx].start - schedule[write_idx].end) < 0.001) {
            // Merge entries
            schedule[write_idx].end = schedule[read_idx].end;
        } else {
            // Move to next position
            write_idx++;
            if (write_idx != read_idx) {
                schedule[write_idx] = schedule[read_idx];
            }
        }
    }
    
    schedule_idx = write_idx + 1;
}

void print_schedule(char* filename) {
    optimize_schedule();
    
    FILE* fp = fopen(filename, "w");
    if (!fp) {
        printf("Error opening output file %s\n", filename);
        return;
    }
    
    fprintf(fp, "Schedule (Hyperperiod: %d):\n", hyperperiod);
    fprintf(fp, "TaskJob | Start-End | Context Switch\n");
    
    for (int i = 0; i < schedule_idx; i++) {
        if (schedule[i].task_id == 0) {
            fprintf(fp, "Idle | %.1f-%.1f\n", schedule[i].start, schedule[i].end);
            continue;
        }
        fprintf(fp, "T%dj%d | %.1f-%.1f", 
               schedule[i].task_id, schedule[i].job_id, 
               schedule[i].start, schedule[i].end);
        
        if (schedule[i].context_switch) {
            fprintf(fp, " | CS");
        }
        fprintf(fp, "\n");
    }
    
    fprintf(fp, "\nAnalysis:\n");
    fprintf(fp, "Total Context Switches: %d\n", context_switches);
    fprintf(fp, "Total Idle Time: %.1f\n", idle_time);
    
    fclose(fp);
}

int main() {
    // Read tasks.txt
    FILE* fp = fopen("tasks.txt", "r");
    if (!fp) {
        printf("Error opening tasks.txt\n");
        return 1;
    }
    
    while (fscanf(fp, "%d %d %d", &tasks[task_count].arrival, 
                &tasks[task_count].wcet, &tasks[task_count].period) == 3) {
        tasks[task_count].id = task_count + 1;
        tasks[task_count].actual = tasks[task_count].wcet; // Default to WCET
        task_count++;
    }
    fclose(fp);
    
    // Read actual.txt
    FILE* act_fp = fopen("actual.txt", "r");
    if (act_fp) {
        float actual;
        int i = 0;
        while (fscanf(act_fp, "%f", &actual) == 1 && i < task_count) {
            tasks[i].actual = actual;
            printf("Task %d actual execution time: %.1f (WCET: %d)\n", 
                   i+1, tasks[i].actual, tasks[i].wcet);
            i++;
        }
        fclose(act_fp);
    }
    
    calculate_hyperperiod();
    printf("Hyperperiod: %d\n", hyperperiod);
    
    generate_jobs();
    printf("Generated %d jobs\n", job_count);
    
    simulate_rmrcs();
    printf("Simulation completed\n");
    
    print_schedule("schedule3.txt");
    printf("Schedule written to schedule3.txt\n");
    
    return 0;
}