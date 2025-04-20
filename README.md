# RTS-scheduling
Scheduling demos on FreeRTOS

# FreeRTOS Earliest Deadline First (EDF) Scheduler Demo

## Overview

This project demonstrates the implementation of an Earliest Deadline First (EDF) scheduling algorithm on top of the standard FreeRTOS fixed-priority pre-emptive scheduler. It uses the FreeRTOS Windows Simulator (via Visual Studio) to run three simulated periodic sensor tasks (Temperature, Pressure, Height) and a custom EDF scheduler task that dynamically adjusts the priorities of the sensor tasks based on their upcoming deadlines.

The core idea is to assign the highest runtime priority to the task with the earliest absolute deadline. This is achieved by a high-priority EDF scheduler task that periodically examines the deadlines of the sensor tasks and uses the `vTaskPrioritySet()` FreeRTOS API function.

## Features

*   Three periodic tasks simulating sensor readings (Temperature, Pressure, Height).
*   Configurable periods for each sensor task defined in `task_config.h`.
*   A custom EDF scheduler task (`vEdfSchedulerTask`).
*   Dynamic priority assignment of sensor tasks based on their absolute deadlines.
*   Mutex protection (`xEdfMutex`) for safe access to shared deadline information.
*   Clear console output showing task execution (start/end of jobs), current priorities, deadlines, and EDF scheduler decisions.
*   Built and tested using FreeRTOS v202012.00 and Visual Studio Community Edition on Windows.

## Prerequisites

1.  **Microsoft Visual Studio:** Community Edition (free) is sufficient.
    *   Ensure the **"Desktop development with C++"** workload is installed.
2.  **FreeRTOS Source Code:** Download the FreeRTOS zip file (e.g., `FreeRTOSv202012.00.zip`). This project was developed using v202012.00.

## Building the Project

1.  Extract the FreeRTOS zip file to a location on your computer (e.g., `C:\FreeRTOSv202012.00`).
2.  Open **Visual Studio**.
3.  Go to `File -> Open -> Project/Solution`.
4.  Navigate to the `FreeRTOSv202012.00\FreeRTOS\Demo\WIN32-MSVC` directory within the extracted files.
5.  Select the solution file `Win32.sln` and click **Open**.
6.  Visual Studio might ask to retarget the project if it was created with an older toolset. Accept the default options.
7.  Select the build configuration (usually **Debug** from the dropdown menu in the toolbar).
8.  Go to `Build -> Build Solution` (or press **F7**).
9.  The project should compile without errors.

## Running the Project

1.  After a successful build, go to `Debug -> Start Without Debugging` (or press **Ctrl+F5**).
2.  A console window will appear, displaying the output from the running tasks and the EDF scheduler.
3.  The output shows task execution timings, sensor values, current priorities, deadlines, and EDF scheduling decisions.
4.  Close the console window to stop the simulation.

## Project Components

The key files created or modified for this EDF demonstration are:

*   `main.c`:
    *   Contains the `main()` function which initializes the system, creates tasks, and starts the scheduler.
    *   Defines the three sensor task functions (`vTemperatureTask`, `vPressureTask`, `vHeightTask`).
    *   Defines the EDF scheduler task function (`vEdfSchedulerTask`).
    *   Manages the shared data structure (`xEdfTasks`) and mutex (`xEdfMutex`).
*   `sensors.h` / `sensors.c`:
    *   Define and implement the simple sensor simulation functions (`getTemperature`, `getPressure`, `getHeight`) which return random values within specified ranges.
    *   Include `initializeSensors()` to seed the random number generator.
*   `task_config.h`:
    *   Contains `#define` constants for configuring task periods (e.g., `TEMP_TASK_PERIOD_MS`), stack sizes (`SENSOR_TASK_STACK_SIZE`), EDF priorities (`EDF_BASE_PRIORITY`, `EDF_SCHEDULER_PRIORITY`), and the EDF check rate (`EDF_CHECK_PERIOD_MS`).
*   `FreeRTOSConfig.h`:
    *   Standard FreeRTOS configuration file. Key settings adjusted for this demo include:
        *   `configMAX_PRIORITIES`: Set high enough to accommodate EDF range (e.g., 5).
        *   `configUSE_PREEMPTION`: Enabled (1).
        *   `configUSE_MUTEXES`: Enabled (1).
        *   `configUSE_TRACE_FACILITY`: Disabled (0) to prevent conflicts/crashes.
        *   `configGENERATE_RUN_TIME_STATS`: Disabled (0).
        *   `configTICK_RATE_HZ`: Set to 1000 for 1ms tick resolution.
*   `WIN32-MSVC/Win32.sln` & `.vcxproj`: Visual Studio solution and project files used for building. (Note: Standard demo task files like `main_full.c`, `main_blinky.c`, `semtest.c`, etc., were excluded from the build in the project settings to avoid linker errors).

## EDF Implementation Details (`vEdfSchedulerTask`)

The core EDF logic resides within the `vEdfSchedulerTask` function in `main.c`. This task runs periodically at the highest priority (`EDF_SCHEDULER_PRIORITY`) to ensure it can preempt the sensor tasks and adjust their priorities.

Here's a breakdown of its operation within its main loop:

1.  **Periodic Execution:**
    ```c
    vTaskDelayUntil( &xLastCheckTime, xCheckFrequency );
    ```
    The task blocks until its next scheduled execution time, determined by `EDF_CHECK_PERIOD_MS` defined in `task_config.h`.

2.  **Acquire Mutex:**
    ```c
    if (xSemaphoreTake( xEdfMutex, portMAX_DELAY ) == pdTRUE) {
        // ... access shared data ...
        xSemaphoreGive( xEdfMutex );
    }
    ```
    Before accessing the shared `xEdfTasks` array (which contains task handles and deadlines updated by the sensor tasks), the scheduler *must* acquire the `xEdfMutex`. This prevents race conditions where a sensor task might try to update its deadline while the scheduler is reading it. `portMAX_DELAY` means it will wait indefinitely if the mutex isn't available (it should always become available quickly).

3.  **Populate Index Array:**
    ```c
    for(int i=0; i < NUM_EDF_TASKS; i++) { sortedIndices[i] = i; }
    ```
    An integer array `sortedIndices` is filled with the original indices (0, 1, 2) of the tasks in the `xEdfTasks` array. This is done so we can sort these *indices* based on the deadlines without modifying the order of the main `xEdfTasks` array directly within the critical section.

4.  **Sort Indices by Deadline:**
    ```c
    // Using simple Bubble Sort for 3 elements
    for (int i = 0; i < NUM_EDF_TASKS - 1; i++) {
        for (int j = 0; j < NUM_EDF_TASKS - i - 1; j++) {
            // Compare deadlines of tasks at sortedIndices[j] and sortedIndices[j+1]
            if (xEdfTasks[sortedIndices[j]].xNextDeadline > xEdfTasks[sortedIndices[j+1]].xNextDeadline) {
                // Swap indices
                int temp = sortedIndices[j]; sortedIndices[j] = sortedIndices[j+1]; sortedIndices[j+1] = temp;
            } else if (xEdfTasks[sortedIndices[j]].xNextDeadline == xEdfTasks[sortedIndices[j+1]].xNextDeadline) {
                 if (sortedIndices[j] > sortedIndices[j+1]) { // Tie-break by index
                      int temp = sortedIndices[j]; sortedIndices[j] = sortedIndices[j+1]; sortedIndices[j+1] = temp;
                 }
            }
        }
    }
    ```
    A simple Bubble Sort is used to reorder the `sortedIndices` array. The comparison logic looks at the `xNextDeadline` values stored in the `xEdfTasks` array for the tasks referenced by the current indices being compared. The index corresponding to the task with the *earlier* deadline "bubbles up" towards the beginning of the `sortedIndices` array. A tie-breaking rule is included: if deadlines are equal, the task with the lower original index (0, 1, or 2) gets higher priority (this is arbitrary but consistent).

5.  **Assign Priorities:**
    ```c
    for(int i = 0; i < NUM_EDF_TASKS; i++) {
        // Assign priorities from high down to low based on sorted deadline order
        UBaseType_t uxNewPriority = uxHighestEdfPriority - i; // Highest prio for i=0

        // Get the actual task index and handle from the sorted list
        int actualTaskIndex = sortedIndices[i];
        TaskHandle_t taskToChange = xEdfTasks[actualTaskIndex].xHandle;

        // Check task still exists (good practice) and priority needs changing
        if (eTaskGetState(taskToChange) != eDeleted)
        {
            UBaseType_t currentPriority = uxTaskPriorityGet(taskToChange);
            if (currentPriority != uxNewPriority) { // Only change if needed
                vTaskPrioritySet( taskToChange, uxNewPriority );
                // ... print log message ...
                bPriorityChanged = pdTRUE; // Mark that a change occurred
            }
        }
    }
    ```
    The code iterates through the *sorted* `sortedIndices` array.
    *   For `i = 0` (the index pointing to the task with the earliest deadline), the `uxNewPriority` is calculated as `uxHighestEdfPriority - 0`.
    *   For `i = 1` (next earliest deadline), the priority is `uxHighestEdfPriority - 1`.
    *   ...and so on. This maps the earliest deadline to the highest available priority level below the scheduler itself.
    *   It retrieves the actual task handle (`taskToChange`) using the sorted index.
    *   It checks the task's *current* priority using `uxTaskPriorityGet()`.
    *   Crucially, it only calls `vTaskPrioritySet()` *if* the calculated `uxNewPriority` is different from the task's `currentPriority`. This avoids unnecessary system calls.
    *   Logging messages are printed only when a priority is actually changed.

6.  **Release Mutex:**
    ```c
    xSemaphoreGive( xEdfMutex );
    ```
    After checking/updating all priorities, the mutex is released, allowing sensor tasks to update their deadlines again.

## Task Creation and Deadline Updates

*   In `main()`, the three sensor tasks (`vTemperatureTask`, `vPressureTask`, `vHeightTask`) are created using `xTaskCreate()`. Their initial priority is set to `SENSOR_TASK_INITIAL_PRIORITY` (which equals `EDF_BASE_PRIORITY`). Their unique index (0, 1, or 2) is passed as the task parameter.
*   The `xEdfTasks` array is populated with the handles, periods, initial deadlines, and names *after* the tasks are created.
*   Inside each sensor task's loop:
    *   *Before* calling `vTaskDelayUntil()`, the task calculates its deadline for the *next* period (`xDeadlineForNextPeriod = xLastWakeTime + xFrequency`).
    *   It takes the `xEdfMutex`, updates its corresponding entry in the `xEdfTasks` array with this new deadline, and gives the mutex back.
    *   It then calls `vTaskDelayUntil()`, which blocks the task until the start of its next period. The work (calling `getTemperature()`, etc.) is performed *after* this call returns.

## Configuration

The primary configuration parameters are in `task_config.h`:

*   `TEMP_TASK_PERIOD_MS`, `PRESSURE_TASK_PERIOD_MS`, `HEIGHT_TASK_PERIOD_MS`: Define the execution period for each sensor task in milliseconds.
*   `EDF_CHECK_PERIOD_MS`: How often (in milliseconds) the EDF scheduler runs to re-evaluate priorities. A smaller value leads to more responsive priority changes but higher scheduler overhead.
*   Other parameters define priority levels and stack sizes, usually determined based on `configMAX_PRIORITIES` and `configMINIMAL_STACK_SIZE` from `FreeRTOSConfig.h`.

## Example Output Interpretation

[EDFSched ] Tick=51 --- EDF Priority Updates --- <- EDF Scheduler runs

[EDFSched ] Set TempTask Prio: 3 (Deadline: 501) <- Temp has earliest deadline -> highest priority (3)

[EDFSched ] Set HeightTask Prio: 2 (Deadline: 751) <- Height is next -> medium priority (2)

[EDFSched ] New Order (Prio): TempTask(3) > HeightTask(2) > PressureTask(1) <- Pressure is lowest (1)
...
[TempTask ] Tick=501 START Job (Prio:3, Deadline:501) <- Temp task starts its job, running at Prio 3

[TempTask ] Tick=501 END Job (Value:51) <- Temp task finishes its job

[EDFSched ] Tick=551 --- EDF Priority Updates --- <- EDF Scheduler runs again

[EDFSched ] Set HeightTask Prio: 3 (Deadline: 751) <- Height now has earliest deadline -> Prio 3

[EDFSched ] Set TempTask Prio: 2 (Deadline: 1001) <- Temp's next deadline is later -> Prio 2

[EDFSched ] New Order (Prio): HeightTask(3) > TempTask(2) > PressureTask(1)
...
[HeightTask] Tick=751 START Job (Prio:3, Deadline:751) <- Height starts, running at Prio 3 (highest)

[HeightTask] Tick=751 END Job (Value:141) <- Height finishes
...
[TempTask ] Tick=1001 START Job (Prio:3, Deadline:1001) <- Temp starts (Prio 3 now)

[Pressure ] Tick=1001 START Job (Prio:2, Deadline:1001) <- Pressure also ready, runs after Temp (Prio 2)

[TempTask ] Tick=1001 END Job (Value:90) <- Temp finishes

[Pressure ] Tick=1001 END Job (Value:7) <- Pressure finishes

The output clearly shows the dynamic priority adjustments made by the EDF scheduler based on the calculated deadlines and how tasks execute according to their assigned priorities. Tasks with earlier deadlines preempt those with later deadlines when they become ready to run.
