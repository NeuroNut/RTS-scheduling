#include "edf_demo.h" 
#include <stdio.h>
#include <stdlib.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "sensors.h"
#include "edf_demo_config.h"
#include "stdbool.h"

// --- Module-static variables ---
static TaskHandle_t s_highestPrioTaskHandle = NULL;
static TickType_t s_hyperperiod = 0;
static bool s_simulationComplete = false;

// Structure to hold EDF task info
typedef struct {
    TaskHandle_t xHandle;
    TickType_t xPeriodTicks;
    TickType_t xNextDeadline;
    const char* pcTaskName;
    int taskIndex;
    UBaseType_t currentPriority;
    int jobCount;  // Track job number for better logging
} EdfTaskInfo_t;

static EdfTaskInfo_t s_xEdfTasks[NUM_EDF_TASKS];
static SemaphoreHandle_t s_xEdfMutex = NULL;

// Calculate LCM for hyperperiod
static TickType_t gcd(TickType_t a, TickType_t b) {
    while (b) {
        TickType_t temp = b;
        b = a % b;
        a = temp;
    }
    return a;
}

static TickType_t lcm(TickType_t a, TickType_t b) {
    return (a / gcd(a, b)) * b;
}

static TickType_t calculate_hyperperiod() {
    TickType_t result = s_xEdfTasks[0].xPeriodTicks;
    for (int i = 1; i < NUM_EDF_TASKS; i++) {
        result = lcm(result, s_xEdfTasks[i].xPeriodTicks);
    }
    return result;
}

// --- Task Functions (EDF Specific Names) ---
static void vTemperatureTask_EDF(void* pvParameters) {
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(TEMP_TASK_PERIOD_MS);
    int tempValue;
    int taskIndex = (int)pvParameters;

    xLastWakeTime = xTaskGetTickCount();

    // Initialize job count
    s_xEdfTasks[taskIndex].jobCount = 1;

    for (;;) {
        // Check if simulation is complete
        if (s_simulationComplete) {
            vTaskDelete(NULL);
            return;
        }

        // Update deadline for next period
        TickType_t xDeadlineForNextPeriod = xLastWakeTime + xFrequency;

        if (xSemaphoreTake(s_xEdfMutex, portMAX_DELAY) == pdTRUE) {
            s_xEdfTasks[taskIndex].xNextDeadline = xDeadlineForNextPeriod;
            xSemaphoreGive(s_xEdfMutex);
        }

        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        // --- Job Execution START ---
        TickType_t currentTick = xTaskGetTickCount();

        // Check if we've reached the hyperperiod
        if (currentTick > s_hyperperiod) {
            s_simulationComplete = true;
            continue;
        }

        UBaseType_t currentPriority = uxTaskPriorityGet(NULL);

        printf("[%-12s] Tick=%-5lu START Job %d (Deadline:%lu)\n",
            s_xEdfTasks[taskIndex].pcTaskName,
            (unsigned long)currentTick,
            s_xEdfTasks[taskIndex].jobCount,
            (unsigned long)xDeadlineForNextPeriod);
        fflush(stdout);

        // --- Perform Task Work ---
        tempValue = getTemperature();

        // --- Job Execution END ---
        printf("[%-12s] Tick=%-5lu END Job %d (Value:%d)\n",
            s_xEdfTasks[taskIndex].pcTaskName,
            (unsigned long)xTaskGetTickCount(),
            s_xEdfTasks[taskIndex].jobCount,
            tempValue);
        fflush(stdout);

        // Increment job count for next iteration
        s_xEdfTasks[taskIndex].jobCount++;
    }
}

static void vPressureTask_EDF(void* pvParameters) {
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(PRESSURE_TASK_PERIOD_MS);
    int pressureValue;
    int taskIndex = (int)pvParameters;

    xLastWakeTime = xTaskGetTickCount();

    // Initialize job count
    s_xEdfTasks[taskIndex].jobCount = 1;

    for (;;) {
        // Check if simulation is complete
        if (s_simulationComplete) {
            vTaskDelete(NULL);
            return;
        }

        TickType_t xDeadlineForNextPeriod = xLastWakeTime + xFrequency;

        if (xSemaphoreTake(s_xEdfMutex, portMAX_DELAY) == pdTRUE) {
            s_xEdfTasks[taskIndex].xNextDeadline = xDeadlineForNextPeriod;
            xSemaphoreGive(s_xEdfMutex);
        }

        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        // Check if we've reached the hyperperiod
        TickType_t currentTick = xTaskGetTickCount();
        if (currentTick > s_hyperperiod) {
            s_simulationComplete = true;
            continue;
        }

        UBaseType_t currentPriority = uxTaskPriorityGet(NULL);

        printf("[%-12s] Tick=%-5lu START Job %d (Deadline:%lu)\n",
            s_xEdfTasks[taskIndex].pcTaskName,
            (unsigned long)currentTick,
            s_xEdfTasks[taskIndex].jobCount,
            (unsigned long)xDeadlineForNextPeriod);
        fflush(stdout);

        pressureValue = getPressure();

        printf("[%-12s] Tick=%-5lu END Job %d (Value:%d)\n",
            s_xEdfTasks[taskIndex].pcTaskName,
            (unsigned long)xTaskGetTickCount(),
            s_xEdfTasks[taskIndex].jobCount,
            pressureValue);
        fflush(stdout);

        // Increment job count for next iteration
        s_xEdfTasks[taskIndex].jobCount++;
    }
}

static void vHeightTask_EDF(void* pvParameters) {
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(HEIGHT_TASK_PERIOD_MS);
    int heightValue;
    int taskIndex = (int)pvParameters;

    xLastWakeTime = xTaskGetTickCount();

    // Initialize job count
    s_xEdfTasks[taskIndex].jobCount = 1;

    for (;;) {
        // Check if simulation is complete
        if (s_simulationComplete) {
            vTaskDelete(NULL);
            return;
        }

        TickType_t xDeadlineForNextPeriod = xLastWakeTime + xFrequency;

        if (xSemaphoreTake(s_xEdfMutex, portMAX_DELAY) == pdTRUE) {
            s_xEdfTasks[taskIndex].xNextDeadline = xDeadlineForNextPeriod;
            xSemaphoreGive(s_xEdfMutex);
        }

        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        // Check if we've reached the hyperperiod
        TickType_t currentTick = xTaskGetTickCount();
        if (currentTick > s_hyperperiod) {
            s_simulationComplete = true;
            continue;
        }

        UBaseType_t currentPriority = uxTaskPriorityGet(NULL);

        printf("[%-12s] Tick=%-5lu START Job %d (Deadline:%lu)\n",
            s_xEdfTasks[taskIndex].pcTaskName,
            (unsigned long)currentTick,
            s_xEdfTasks[taskIndex].jobCount,
            (unsigned long)xDeadlineForNextPeriod);
        fflush(stdout);

        heightValue = getHeight();

        printf("[%-12s] Tick=%-5lu END Job %d (Value:%d)\n",
            s_xEdfTasks[taskIndex].pcTaskName,
            (unsigned long)xTaskGetTickCount(),
            s_xEdfTasks[taskIndex].jobCount,
            heightValue);
        fflush(stdout);

        // Increment job count for next iteration
        s_xEdfTasks[taskIndex].jobCount++;
    }
}

// EDF Scheduler Task (Improved Logging)
static void vEdfSchedulerTask_EDF(void* pvParameters) {
    TickType_t xLastCheckTime;
    const TickType_t xCheckFrequency = pdMS_TO_TICKS(EDF_CHECK_PERIOD_MS);
    const UBaseType_t uxHighestEdfPriority = EDF_BASE_PRIORITY + NUM_EDF_TASKS - 1;
    int sortedIndices[NUM_EDF_TASKS];
    BaseType_t bPriorityChanged = pdFALSE;
    TaskHandle_t xCurrentHighestTaskHandle = NULL;

    printf("\n===== EDF SCHEDULING SIMULATION =====\n");
    printf("Task Information:\n");
    printf("- TempTask:     Period=%lu ms\n", (unsigned long)TEMP_TASK_PERIOD_MS);
    printf("- PressureTask: Period=%lu ms\n", (unsigned long)PRESSURE_TASK_PERIOD_MS);
    printf("- HeightTask:   Period=%lu ms\n", (unsigned long)HEIGHT_TASK_PERIOD_MS);
    printf("Hyperperiod: %lu ticks\n", (unsigned long)s_hyperperiod);
    printf("=====================================\n\n");

    printf("[Scheduler] Tick=%-5lu EDF Scheduler Started\n", (unsigned long)xTaskGetTickCount());
    fflush(stdout);

    xLastCheckTime = xTaskGetTickCount();

    // Keep track of the current execution sequence for Gantt chart
    printf("\n----- EDF EXECUTION SEQUENCE -----\n");

    for (;;) {
        vTaskDelayUntil(&xLastCheckTime, xCheckFrequency);
        TickType_t currentTick = xTaskGetTickCount();

        // Check if simulation is complete
        if (s_simulationComplete || currentTick > s_hyperperiod) {
            printf("\n----- END OF SIMULATION (Hyperperiod: %lu) -----\n", (unsigned long)s_hyperperiod);
            printf("\nEDF Schedule Summary:\n");

            // Print a simple summary of the schedule
            printf("The EDF algorithm scheduled tasks based on earliest deadline:\n");
            printf("- Tasks with earlier deadlines received higher priorities\n");
            printf("- Preemption occurred when a task with an earlier deadline became ready\n");

            printf("\nSimulation complete.\n");
            fflush(stdout);

            s_simulationComplete = true;
            vTaskDelete(NULL);
            return;
        }

        bPriorityChanged = pdFALSE;
        xCurrentHighestTaskHandle = NULL;

        if (xSemaphoreTake(s_xEdfMutex, portMAX_DELAY) == pdTRUE) {
            // 1. Populate index array & update current priorities in struct
            for (int i = 0; i < NUM_EDF_TASKS; i++) {
                sortedIndices[i] = i;
                if (s_xEdfTasks[i].xHandle != NULL) {
                    s_xEdfTasks[i].currentPriority = uxTaskPriorityGet(s_xEdfTasks[i].xHandle);
                }
                else {
                    s_xEdfTasks[i].currentPriority = 0;
                }
            }

            // 2. Sort indices based on deadline in s_xEdfTasks (Bubble Sort)
            for (int i = 0; i < NUM_EDF_TASKS - 1; i++) {
                for (int j = 0; j < NUM_EDF_TASKS - i - 1; j++) {
                    // Compare deadlines
                    if (s_xEdfTasks[sortedIndices[j]].xNextDeadline > s_xEdfTasks[sortedIndices[j + 1]].xNextDeadline) {
                        int temp = sortedIndices[j];
                        sortedIndices[j] = sortedIndices[j + 1];
                        sortedIndices[j + 1] = temp;
                    }
                    else if (s_xEdfTasks[sortedIndices[j]].xNextDeadline == s_xEdfTasks[sortedIndices[j + 1]].xNextDeadline) {
                        // Tie-break by index (lower index = higher effective prio in tie)
                        if (sortedIndices[j] > sortedIndices[j + 1]) {
                            int temp = sortedIndices[j];
                            sortedIndices[j] = sortedIndices[j + 1];
                            sortedIndices[j + 1] = temp;
                        }
                    }
                }
            }

            // 3. Assign Priorities & Log Changes/Preemptions
            TaskHandle_t xNewHighestTaskHandle = NULL;

            for (int i = 0; i < NUM_EDF_TASKS; i++) {
                UBaseType_t uxNewPriority = uxHighestEdfPriority - i;
                int actualTaskIndex = sortedIndices[i];
                TaskHandle_t taskToChange = s_xEdfTasks[actualTaskIndex].xHandle;

                if (taskToChange == NULL || eTaskGetState(taskToChange) == eDeleted)
                    continue;

                // Store the handle of the task that should have the highest priority now
                if (i == 0) {
                    xNewHighestTaskHandle = taskToChange;
                }

                // Update stored priority and check if actual priority needs setting
                UBaseType_t previousStoredPriority = s_xEdfTasks[actualTaskIndex].currentPriority;
                s_xEdfTasks[actualTaskIndex].currentPriority = uxNewPriority;

                if (uxTaskPriorityGet(taskToChange) != uxNewPriority) {
                    vTaskPrioritySet(taskToChange, uxNewPriority);

                    if (!bPriorityChanged) {
                        printf("[Scheduler] Tick=%-5lu Priority Updates:\n", currentTick);
                        bPriorityChanged = pdTRUE;
                    }

                    printf("  - %-12s: %lu -> %lu (Deadline: %lu)\n",
                        s_xEdfTasks[actualTaskIndex].pcTaskName,
                        (unsigned long)previousStoredPriority,
                        (unsigned long)uxNewPriority,
                        (unsigned long)s_xEdfTasks[actualTaskIndex].xNextDeadline);
                }
            }

            // 4. Log Preemption / Context Switch Info
            if (bPriorityChanged) {
                printf("  New Priority Order: ");
                for (int i = 0; i < NUM_EDF_TASKS; i++) {
                    int idx = sortedIndices[i];
                    if (s_xEdfTasks[idx].xHandle == NULL)
                        continue;
                    printf("%s(%lu)%s",
                        s_xEdfTasks[idx].pcTaskName,
                        (unsigned long)s_xEdfTasks[idx].currentPriority,
                        (i < NUM_EDF_TASKS - 1) ? " > " : "");
                }
                printf("\n");

                // Check if the highest priority task changed -> likely preemption
                if (xNewHighestTaskHandle != s_highestPrioTaskHandle &&
                    xNewHighestTaskHandle != NULL &&
                    s_highestPrioTaskHandle != NULL) {

                    // Find names for logging
                    const char* newHighestName = "Unknown";
                    const char* oldHighestName = "Unknown";

                    for (int i = 0; i < NUM_EDF_TASKS; ++i) {
                        if (s_xEdfTasks[i].xHandle == xNewHighestTaskHandle)
                            newHighestName = s_xEdfTasks[i].pcTaskName;
                        if (s_xEdfTasks[i].xHandle == s_highestPrioTaskHandle)
                            oldHighestName = s_xEdfTasks[i].pcTaskName;
                    }

                    printf("  Context Switch: %s preempts %s (earlier deadline)\n\n",
                        newHighestName, oldHighestName);
                }

                fflush(stdout);
            }

            // Update the tracked highest priority task handle for the next cycle
            s_highestPrioTaskHandle = xNewHighestTaskHandle;

            xSemaphoreGive(s_xEdfMutex);
        }
        else {
            printf("[Scheduler] Tick=%-5lu ERROR: Failed to get mutex!\n", currentTick);
            fflush(stdout);
        }
    }
}

// --- Public Setup Function ---
void start_edf_demo(void) {
    printf("--- Initializing EDF Demo ---\n");

    // Initialize sensors
    initializeSensors();

    // Create Mutex
    s_xEdfMutex = xSemaphoreCreateMutex();
    if (s_xEdfMutex == NULL) {
        printf("ERROR: Failed to create EDF Mutex!\n");
        fflush(stdout);
        while (1);
    }

    // Task Handles
    TaskHandle_t tempHandle = NULL;
    TaskHandle_t pressHandle = NULL;
    TaskHandle_t heightHandle = NULL;

    // Create Sensor Tasks
    printf("Creating EDF Sensor Tasks...\n");
    xTaskCreate(vTemperatureTask_EDF, "TempTask", EDF_SENSOR_TASK_STACK_SIZE,
        (void*)0, EDF_SENSOR_TASK_INITIAL_PRIORITY, &tempHandle);
    xTaskCreate(vPressureTask_EDF, "PressureTask", EDF_SENSOR_TASK_STACK_SIZE,
        (void*)1, EDF_SENSOR_TASK_INITIAL_PRIORITY, &pressHandle);
    xTaskCreate(vHeightTask_EDF, "HeightTask", EDF_SENSOR_TASK_STACK_SIZE,
        (void*)2, EDF_SENSOR_TASK_INITIAL_PRIORITY, &heightHandle);

    if (!tempHandle || !pressHandle || !heightHandle) {
        printf("ERROR: Failed to create one or more EDF sensor tasks!\n");
        fflush(stdout);
        while (1);
    }

    // Initialize shared task info array
    TickType_t now = xTaskGetTickCount();
    printf("Initializing EDF Task Info (Current Tick = %lu)...\n", (unsigned long)now);

    if (xSemaphoreTake(s_xEdfMutex, portMAX_DELAY) == pdTRUE) {
        s_xEdfTasks[0] = (EdfTaskInfo_t){
            tempHandle,
            pdMS_TO_TICKS(TEMP_TASK_PERIOD_MS),
            now + pdMS_TO_TICKS(TEMP_TASK_PERIOD_MS),
            "TempTask",
            0,
            EDF_SENSOR_TASK_INITIAL_PRIORITY,
            1  // Initial job count
        };

        s_xEdfTasks[1] = (EdfTaskInfo_t){
            pressHandle,
            pdMS_TO_TICKS(PRESSURE_TASK_PERIOD_MS),
            now + pdMS_TO_TICKS(PRESSURE_TASK_PERIOD_MS),
            "PressureTask",
            1,
            EDF_SENSOR_TASK_INITIAL_PRIORITY,
            1  // Initial job count
        };

        s_xEdfTasks[2] = (EdfTaskInfo_t){
            heightHandle,
            pdMS_TO_TICKS(HEIGHT_TASK_PERIOD_MS),
            now + pdMS_TO_TICKS(HEIGHT_TASK_PERIOD_MS),
            "HeightTask",
            2,
            EDF_SENSOR_TASK_INITIAL_PRIORITY,
            1  // Initial job count
        };

        // Calculate hyperperiod
        s_hyperperiod = calculate_hyperperiod();

        xSemaphoreGive(s_xEdfMutex);
    }
    else {
        printf("ERROR: Failed to get mutex for EDF init!\n");
        fflush(stdout);
        while (1);
    }

    // Create EDF Scheduler Task
    printf("Creating EDF Scheduler Task...\n");
    xTaskCreate(vEdfSchedulerTask_EDF, "EDFSched", EDF_SENSOR_TASK_STACK_SIZE,
        NULL, EDF_SCHEDULER_PRIORITY, NULL);

    printf("EDF Demo Setup Complete.\n");
    fflush(stdout);
}
     