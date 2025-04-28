#ifndef EDF_CONFIG_H
#define EDF_CONFIG_H

#include "FreeRTOS.h"
#include "task.h"

// --- Sensor Task Periods and Stack ---
#define TEMP_TASK_PERIOD_MS     500
#define PRESSURE_TASK_PERIOD_MS 1000
#define HEIGHT_TASK_PERIOD_MS   750
#define EDF_SENSOR_TASK_STACK_SIZE  ( configMINIMAL_STACK_SIZE + 50 )

// --- EDF Configuration ---
#define NUM_EDF_TASKS           3

// Priority Levels for EDF demo
// Ensure configMAX_PRIORITIES >= (EDF_BASE_PRIORITY + NUM_EDF_TASKS + 1) = 5
#define EDF_BASE_PRIORITY       ( tskIDLE_PRIORITY + 1 ) // Lowest priority EDF will assign (1)
#define EDF_SCHEDULER_PRIORITY  ( EDF_BASE_PRIORITY + NUM_EDF_TASKS ) // Highest priority (4)

// How often the EDF scheduler checks deadlines (in ms)
#define EDF_CHECK_PERIOD_MS     50

// Initial priority for sensor tasks when created
#define EDF_SENSOR_TASK_INITIAL_PRIORITY ( EDF_BASE_PRIORITY )

#endif // EDF_CONFIG_H