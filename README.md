# Part 1 FreeRTOS EDF Scheduler Demo (Sensor Tasks)

## Overview

This project demonstrates an implementation of the Earliest Deadline First (EDF) scheduling algorithm using the FreeRTOS real-time kernel. The simulation runs within the FreeRTOS Windows port using Visual Studio.

The goal is to fulfill the requirements of the assignment:
1.  Create periodic tasks corresponding to simulated sensor readings (Temperature, Pressure, Height).
2.  Implement an EDF scheduler task that dynamically adjusts the priorities of the sensor tasks based on their deadlines.
3.  Log the execution sequence, including task starts/ends, priority changes, and preemptions, up to one hyperperiod.

## How it Works

### EDF Concept
Earliest Deadline First (EDF) is a dynamic priority scheduling algorithm. The core principle is simple: **the task whose absolute deadline is closest in the future gets the highest priority**. Priorities are re-evaluated whenever scheduling decisions need to be made (in this implementation, periodically by a dedicated scheduler task). This policy is optimal for uniprocessor systems, meaning if a task set can be scheduled by any algorithm, EDF can schedule it.

### Implementation (`edf_demo.c`)
1.  **Sensor Tasks:**
    *   Three tasks (`vTemperatureTask_EDF`, `vPressureTask_EDF`, `vHeightTask_EDF`) simulate periodic work.
    *   Each task corresponds to a sensor type and uses functions from `sensors.c` (`getTemperature`, etc.) to get a simulated reading.
    *   They use `vTaskDelayUntil()` to achieve precise periodic execution based on periods defined in `edf_config.h`.
    *   Before blocking with `vTaskDelayUntil()`, each task calculates its *next* absolute deadline (`xLastWakeTime + xFrequency`) and updates this value in a shared data structure (`s_xEdfTasks`) protected by a mutex (`s_xEdfMutex`).
    *   They log `START Job` and `END Job` messages, including the current tick, job number, and calculated deadline for the job instance.

2.  **EDF Scheduler Task (`vEdfSchedulerTask_EDF`):**
    *   This task (`EDFSched`) runs at the highest application priority (`EDF_SCHEDULER_PRIORITY`).
    *   It executes periodically (defined by `EDF_CHECK_PERIOD_MS`).
    *   In each cycle, it acquires the mutex (`s_xEdfMutex`) to safely access the shared task data.
    *   It reads the `xNextDeadline` for all sensor tasks.
    *   It **sorts** the tasks based on their `xNextDeadline` (earliest deadline first).
    *   It **assigns priorities dynamically** using `vTaskPrioritySet()`. The task with the earliest deadline gets the highest available priority (`EDF_BASE_PRIORITY + NUM_EDF_TASKS - 1`), the next earliest gets the next highest, and so on, down to `EDF_BASE_PRIORITY`.
    *   It logs **priority changes** when they occur, showing the task, old priority, new priority, and deadline.
    *   It attempts to detect and log **context switches/preemptions** by comparing the task handle that *should* have the highest priority in the current cycle (`xNewHighestTaskHandle`) with the one that had the highest priority in the *previous* cycle (`s_highestPrioTaskHandle`).

3.  **Simulation End:**
    *   The hyperperiod (LCM of all task periods) is calculated.
    *   The scheduler task monitors the current tick time.
    *   When the tick count exceeds the hyperperiod, it sets a flag (`s_simulationComplete`) and deletes itself.
    *   The sensor tasks check this flag in their loops and delete themselves when it's set, cleanly ending the simulation after one hyperperiod.

### Understanding Ticks
*   FreeRTOS uses a discrete time unit called a **tick**.
*   The duration of a tick is determined by `configTICK_RATE_HZ` in `FreeRTOSConfig.h`. Typically, for the Windows simulator, this is set to `1000`, meaning `configTICK_RATE_HZ = 1000Hz`, so **1 tick = 1 millisecond**.
*   All time-related operations in FreeRTOS (delays, periods, timeouts, deadlines) are measured in these ticks (`TickType_t`).
*   The function `pdMS_TO_TICKS()` converts milliseconds (used for configuration) into the equivalent number of ticks based on `configTICK_RATE_HZ`.
*   `xTaskGetTickCount()` returns the number of ticks that have elapsed since the scheduler started.

## Project Setup

1.  **FreeRTOS Version:** Download and extract FreeRTOS **v202012.00** (or 202012.04).
2.  **Visual Studio:** Use **Microsoft Visual Studio Community Edition** (ensure "Desktop development with C++" workload is installed).
3.  **Open Solution:**
    *   Start Visual Studio.
    *   Go to `File -> Open -> Project/Solution`.
    *   Navigate to the extracted FreeRTOS folder, then into `FreeRTOS\Demo\WIN32-MSVC`.
    *   Select and open the `Win32.sln` file.
    *   If prompted, allow Visual Studio to retarget the project.

## File Placement

1.  **Inside the `WIN32-MSVC` Directory:** Place the following files directly into this directory (the same level as the original `main.c`, `.vcxproj` file, etc.):
    *   `edf_demo.c` 
    *   `edf_demo.h` 
    *   `edf_config.h` 
    *   `sensors.c` 
    *   `sensors.h`
   
2.  **Replace `main.c`:** Replace the existing `main.c` file in the `WIN32-MSVC` directory with the **launcher `main.c`** 
3.  **Add Files to Project:**
    *   In Visual Studio's **Solution Explorer**, right-click on the project name (e.g., "RTOSDemo" or "WIN32").
    *   Select `Add -> Existing Item...`.
    *   Select all the `.c` and `.h` files you just copied into the directory (`edf_demo.c`, `edf_demo.h`, `edf_config.h`, `sensors.c`, `sensors.h`, and the RM-RCS files if applicable).
    *   Click `Add`.

## Building and Running

1.  **Exclude Unused Demos:**
    *   In Solution Explorer, find standard FreeRTOS full demo files like and exclude them from the build of this project by going in to their properties
    *   **Crucially:** Ensure `main.c` (your launcher), `edf_demo.c`, `sensors.c` are **NOT** excluded.
2.  **Clean:** Go to `Build -> Clean Solution`.
3.  **Build:** Go to `Build -> Build Solution`
4.  **Run:** Go to `Debug -> Start Without Debugging`
5.  **Select Demo:** The console window will appear with a menu. Press `1` and Enter to run the EDF demo.

## FreeRTOS API Functions Used (EDF Demo)

This implementation primarily uses the following FreeRTOS API functions:

*   `xTaskCreate()`: To create the sensor tasks and the EDF scheduler task.
*   `vTaskDelayUntil()`: To ensure periodic execution of sensor tasks.
*   `xTaskGetTickCount()`: To get the current time in ticks for logging and deadline calculations.
*   `pdMS_TO_TICKS()`: To convert periods defined in milliseconds to ticks.
*   `xSemaphoreCreateMutex()`: To create the mutex protecting the shared task data.
*   `xSemaphoreTake()`: To acquire the mutex before accessing shared data.
*   `xSemaphoreGive()`: To release the mutex after accessing shared data.
*   `uxTaskPriorityGet()`: To read the current priority of a task (for logging and scheduler checks).
*   `vTaskPrioritySet()`: To dynamically change the priority of sensor tasks based on EDF rules.
*   `eTaskGetState()`: To check if a task handle is valid (not deleted) before operating on it.
*   `vTaskDelete()`: To cleanly terminate tasks at the end of the simulation hyperperiod.
*   `vTaskStartScheduler()`: (Called in `main.c`) To start the FreeRTOS scheduler.
*   `pvPortMalloc()`: (Used internally by FreeRTOS for creating tasks, mutexes, etc.)

# Part 2 RM-RCS Scheduling Simulator

This project contains two C implementations of the Rate-Monotonic with Reduced Context Switches (RM-RCS) scheduling algorithm for real-time systems. RM-RCS is an enhancement of the Rate-Monotonic (RM) scheduling algorithm, designed to reduce context switches by deferring preemptions of lower-priority tasks when feasible, while ensuring all tasks meet their deadlines.

## RM-RCS Algorithm Overview

**Rate-Monotonic (RM)** is a fixed-priority scheduling algorithm where tasks are assigned priorities based on their periods: shorter periods get higher priority. A higher-priority task preempts a lower-priority task immediately upon arrival.

**RM-RCS** improves RM by allowing a lower-priority task to continue running when a higher-priority task arrives, provided it doesn't cause any task to miss its deadline. It evaluates whether the current task can extend its execution by checking if `t + E ≤ D`, where:
- `t`: Current time.
- `E`: Sum of remaining execution times of higher-priority ready tasks.
- `D`: Earliest deadline of higher-priority ready tasks.

If the condition holds, the lower-priority task extends, reducing context switches. If `t + E > D`, the higher-priority task preempts to ensure deadlines are met. This approach minimizes overhead while maintaining schedulability.

## Implementations

### 1. `main_g_backup.c` (WCET for All Invocations)
- **Description**: Implements RM-RCS using Worst-Case Execution Time (WCET) for all job invocations of each task, as specified in `tasks.txt`.
- **Input**:
  - `tasks.txt`: Format is one task per line with `arrival wcet period` (e.g., `0 1 4` for Task 1).
- **Output**: Writes the schedule to `schedule.txt`, including task execution intervals (e.g., `T1j1 0-1`), context switches, idle time, and average turnaround times per task.
- **Behavior**: Uses discrete time steps (integer ticks). All jobs of a task use the WCET, leading to a fully utilized schedule with minimal or no idle time, as WCET assumes maximum execution duration.
- **Example**: For tasks `T1 (WCET=1, period=4)`, `T2 (WCET=2, period=5)`, `T3 (WCET=7, period=20)`, it produces a schedule with ~9 context switches and typically 0 idle time, as WCET fills the hyperperiod.

### 2. `main_actual_time.c` (WCET for First Invocation, Actual Times for Subsequent)
- **Description**: Implements RM-RCS with WCET for the first job of each task and actual execution times (shorter than WCET) for subsequent jobs, read from `actual.txt`.
- **Input**:
  - `tasks.txt`: Same format as above.
  - `actual.txt`: One actual execution time per line (e.g., `0.5` for Task 1), applied to jobs after the first.
- **Output**: Writes the schedule to `schedule3.txt` in floating-point format (e.g., `T1j2 6.0-6.5`), including task execution intervals, context switches, and idle time.
- **Behavior**: Uses continuous time with floating-point precision. The first job uses WCET, while subsequent jobs use actual times, which are less than WCET, introducing idle time due to early job completion. For example, with `T1 (WCET=1, actual=0.5)`, `T2 (WCET=2, actual=1.5)`, `T3 (WCET=7, actual=5.0)`, it produces ~9 context switches and ~3.5 units of idle time from savings (T1: 2.0, T2: 1.5).
- **Example**: T1’s first job takes 1 unit, subsequent jobs take 0.5 units, creating idle periods (e.g., 10.5-12.0, 18.0-20.0).

## Running the Programs

Both programs can be compiled and run using GCC with the provided input files (`tasks.txt` and, for `main_actual_time.c`, `actual.txt`). Example commands:
```bash
gcc main_wcet_actual.c -o rmrcs_wcet
./rmrcs_wcet
gcc main_actual_time.c -o rmrcs_actual -lm
./rmrcs_actual