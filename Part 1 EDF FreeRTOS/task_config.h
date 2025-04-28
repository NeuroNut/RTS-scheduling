#ifndef TASK_CONFIG_H
#define TASK_CONFIG_H

// --- Task Periods and Stack ---
#define TEMP_TASK_PERIOD_MS     500
#define PRESSURE_TASK_PERIOD_MS 1000
#define HEIGHT_TASK_PERIOD_MS   750
#define SENSOR_TASK_STACK_SIZE  ( configMINIMAL_STACK_SIZE + 50 ) // Slightly increase stack for safety/printf


// --- EDF Configuration ---
#define NUM_EDF_TASKS           3  // We have 3 sensor tasks managed by EDF

// Priority Levels:
// Highest priority is reserved for the EDF scheduler task itself.
// Sensor tasks will dynamically get priorities *below* the scheduler.
// Requires configMAX_PRIORITIES >= (EDF_BASE_PRIORITY + NUM_EDF_TASKS + 1)
// Example: If configMAX_PRIORITIES = 5:
// IDLE = 0
// EDF Tasks = 1, 2, 3 (dynamically assigned)
// EDF Scheduler = 4

#define EDF_BASE_PRIORITY       ( tskIDLE_PRIORITY + 1 ) // Lowest priority EDF will assign
#define EDF_SCHEDULER_PRIORITY  ( EDF_BASE_PRIORITY + NUM_EDF_TASKS ) // Highest priority in the system

// How often the EDF scheduler task runs to check deadlines (in ms)
#define EDF_CHECK_PERIOD_MS     50

// Initial priority for sensor tasks when created (before EDF takes over)
// Should be the lowest priority EDF can assign.
#define SENSOR_TASK_INITIAL_PRIORITY ( EDF_BASE_PRIORITY )


#endif // TASK_CONFIG_H