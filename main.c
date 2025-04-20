/* Standard includes. */
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <limits.h> // Required for ULONG_MAX

/* Visual studio intrinsics used so the __debugbreak() function is available
should an assert get hit. */
#include <intrin.h>

/* FreeRTOS kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h" // Required for Mutex

/* Application includes */
#include "sensors.h"
#include "task_config.h"


/* This project provides two demo applications.  A simple blinky style demo
application, and a more comprehensive test and demo application.  The
mainCREATE_SIMPLE_BLINKY_DEMO_ONLY setting is used to select between the two.

If mainCREATE_SIMPLE_BLINKY_DEMO_ONLY is 1 then the blinky demo will be built.
The blinky demo is implemented and described in main_blinky.c.

If mainCREATE_SIMPLE_BLINKY_DEMO_ONLY is not 1 then the comprehensive test and
demo application will be built.  The comprehensive test and demo application is
implemented and described in main_full.c. */
#define mainCREATE_SIMPLE_BLINKY_DEMO_ONLY	1 // Keep this as 1 to run our tasks

/* ... [Rest of the comments and defines like mainREGION_x_SIZE remain unchanged] ... */
#define mainREGION_1_SIZE	8201
#define mainREGION_2_SIZE	29905
#define mainREGION_3_SIZE	7607

/*-----------------------------------------------------------*/

/*
 * main_blinky() is used when mainCREATE_SIMPLE_BLINKY_DEMO_ONLY is set to 1.
 * main_full() is used when mainCREATE_SIMPLE_BLINKY_DEMO_ONLY is set to 0.
 */
extern void main_blinky(void); /* We won't call this */
extern void main_full(void);   /* We won't call this */

/* ... [Declarations for Hook functions vFullDemoTickHookFunction, vFullDemoIdleFunction remain unchanged] ... */
void vFullDemoTickHookFunction(void);
void vFullDemoIdleFunction(void);

/* ... [prvInitialiseHeap declaration remains unchanged] ... */
static void  prvInitialiseHeap(void);

/* ... [Prototypes for standard FreeRTOS hooks remain unchanged] ... */
void vApplicationMallocFailedHook(void);
void vApplicationIdleHook(void);
void vApplicationStackOverflowHook(TaskHandle_t pxTask, char* pcTaskName);
void vApplicationTickHook(void);
void vApplicationGetIdleTaskMemory(StaticTask_t** ppxIdleTaskTCBBuffer, StackType_t** ppxIdleTaskStackBuffer, uint32_t* pulIdleTaskStackSize);
void vApplicationGetTimerTaskMemory(StaticTask_t** ppxTimerTaskTCBBuffer, StackType_t** ppxTimerTaskStackBuffer, uint32_t* pulTimerTaskStackSize);

/* ... [prvSaveTraceFile declaration remains unchanged] ... */
static void prvSaveTraceFile(void);

/*-----------------------------------------------------------*/
/*            EDF Scheduler Data Structures                  */
/*-----------------------------------------------------------*/

// Structure to hold EDF task info
typedef struct {
	TaskHandle_t xHandle;       // Task handle
	TickType_t xPeriodTicks;    // Period of the task in ticks
	TickType_t xNextDeadline;   // Absolute deadline in ticks
	const char* pcTaskName;     // Name for debugging
	int          taskIndex;       // Index in the array (0, 1, 2)
} EdfTaskInfo_t;

// Array to hold info for our tasks managed by EDF
static EdfTaskInfo_t xEdfTasks[NUM_EDF_TASKS];

// Mutex to protect access to xEdfTasks array
static SemaphoreHandle_t xEdfMutex = NULL;

/*-----------------------------------------------------------*/

/* When configSUPPORT_STATIC_ALLOCATION is set to 1 the application writer can
use a callback function to optionally provide the memory required by the idle
and timer tasks.  This is the stack that will be used by the timer task.  It is
declared here, as a global, so it can be checked by a test that is implemented
in a different file. */
StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH];

/* Notes if the trace is running or not. */
static BaseType_t xTraceRunning = pdTRUE;

/*-----------------------------------------------------------*/
/*            Sensor Task Definitions (Modified for EDF)     */
/*-----------------------------------------------------------*/

// Task function for Temperature Reading (Improved Output)
static void vTemperatureTask(void* pvParameters) {
	TickType_t xLastWakeTime;
	const TickType_t xFrequency = pdMS_TO_TICKS(TEMP_TASK_PERIOD_MS);
	int tempValue;
	int taskIndex = (int)pvParameters; // Get index passed during creation

	xLastWakeTime = xTaskGetTickCount();

	for (;; ) {
		// --- EDF Deadline Update (For *next* period) ---
		TickType_t xDeadlineForNextPeriod = xLastWakeTime + xFrequency;
		if (xSemaphoreTake(xEdfMutex, portMAX_DELAY) == pdTRUE) {
			xEdfTasks[taskIndex].xNextDeadline = xDeadlineForNextPeriod;
			xSemaphoreGive(xEdfMutex);
		}
		// --- End EDF Update ---

		// Wait for the start of the current period
		vTaskDelayUntil(&xLastWakeTime, xFrequency);

		// --- Job Execution START ---
		TickType_t currentTick = xTaskGetTickCount();
		// Get the current deadline for THIS job (calculated in the previous iteration)
		TickType_t currentDeadline = xLastWakeTime; // Deadline was start time + period
		UBaseType_t currentPriority = uxTaskPriorityGet(NULL); // Get own priority
		printf("[%-12s] Tick=%-5lu START Job (Prio:%lu, Deadline:%lu)\n",
			xEdfTasks[taskIndex].pcTaskName,
			(unsigned long)currentTick,
			(unsigned long)currentPriority,
			(unsigned long)currentDeadline); // Deadline for the job *just started*
		fflush(stdout);

		// --- Perform Task Work ---
		tempValue = getTemperature();
		// You could add a small artificial delay here if tasks finish too quickly
		// for(volatile int i=0; i<50000; i++); // Example artificial workload

		// --- Job Execution END ---
		printf("[%-12s] Tick=%-5lu END Job   (Value:%d)\n",
			xEdfTasks[taskIndex].pcTaskName,
			(unsigned long)xTaskGetTickCount(), // Get time right at the end
			tempValue);
		fflush(stdout);
	}
}

// Task function for Pressure Reading (Improved Output)
static void vPressureTask(void* pvParameters) {
	TickType_t xLastWakeTime;
	const TickType_t xFrequency = pdMS_TO_TICKS(PRESSURE_TASK_PERIOD_MS);
	int pressureValue;
	int taskIndex = (int)pvParameters; // Get index

	xLastWakeTime = xTaskGetTickCount();

	for (;; ) {
		// --- EDF Deadline Update (For *next* period) ---
		TickType_t xDeadlineForNextPeriod = xLastWakeTime + xFrequency;
		if (xSemaphoreTake(xEdfMutex, portMAX_DELAY) == pdTRUE) {
			xEdfTasks[taskIndex].xNextDeadline = xDeadlineForNextPeriod;
			xSemaphoreGive(xEdfMutex);
		}
		// --- End EDF Update ---

		vTaskDelayUntil(&xLastWakeTime, xFrequency);

		// --- Job Execution START ---
		TickType_t currentTick = xTaskGetTickCount();
		TickType_t currentDeadline = xLastWakeTime;
		UBaseType_t currentPriority = uxTaskPriorityGet(NULL);
		printf("[%-12s] Tick=%-5lu START Job (Prio:%lu, Deadline:%lu)\n",
			xEdfTasks[taskIndex].pcTaskName,
			(unsigned long)currentTick,
			(unsigned long)currentPriority,
			(unsigned long)currentDeadline);
		fflush(stdout);

		// --- Perform Task Work ---
		pressureValue = getPressure();
		// for(volatile int i=0; i<50000; i++);

		// --- Job Execution END ---
		printf("[%-12s] Tick=%-5lu END Job   (Value:%d)\n",
			xEdfTasks[taskIndex].pcTaskName,
			(unsigned long)xTaskGetTickCount(),
			pressureValue);
		fflush(stdout);
	}
}

// Task function for Height Reading (Improved Output)
static void vHeightTask(void* pvParameters) {
	TickType_t xLastWakeTime;
	const TickType_t xFrequency = pdMS_TO_TICKS(HEIGHT_TASK_PERIOD_MS);
	int heightValue;
	int taskIndex = (int)pvParameters; // Get index

	xLastWakeTime = xTaskGetTickCount();

	for (;; ) {
		// --- EDF Deadline Update (For *next* period) ---
		TickType_t xDeadlineForNextPeriod = xLastWakeTime + xFrequency;
		if (xSemaphoreTake(xEdfMutex, portMAX_DELAY) == pdTRUE) {
			xEdfTasks[taskIndex].xNextDeadline = xDeadlineForNextPeriod;
			xSemaphoreGive(xEdfMutex);
		}
		// --- End EDF Update ---

		vTaskDelayUntil(&xLastWakeTime, xFrequency);

		// --- Job Execution START ---
		TickType_t currentTick = xTaskGetTickCount();
		TickType_t currentDeadline = xLastWakeTime;
		UBaseType_t currentPriority = uxTaskPriorityGet(NULL);
		printf("[%-12s] Tick=%-5lu START Job (Prio:%lu, Deadline:%lu)\n",
			xEdfTasks[taskIndex].pcTaskName,
			(unsigned long)currentTick,
			(unsigned long)currentPriority,
			(unsigned long)currentDeadline);
		fflush(stdout);

		// --- Perform Task Work ---
		heightValue = getHeight();
		// for(volatile int i=0; i<50000; i++);

		// --- Job Execution END ---
		printf("[%-12s] Tick=%-5lu END Job   (Value:%d)\n",
			xEdfTasks[taskIndex].pcTaskName,
			(unsigned long)xTaskGetTickCount(),
			heightValue);
		fflush(stdout);
	}
}


/*-----------------------------------------------------------*/
/*             EDF Scheduler Task Definition                 */
/*-----------------------------------------------------------*/

// Comparison function for qsort (if needed, but bubble sort is fine for 3)
// Can be used later if NUM_EDF_TASKS grows
/*
static int compareEdfTasks(const void* a, const void* b) {
	EdfTaskInfo_t* taskA = (EdfTaskInfo_t*)a;
	EdfTaskInfo_t* taskB = (EdfTaskInfo_t*)b;

	if (taskA->xNextDeadline < taskB->xNextDeadline) return -1;
	if (taskA->xNextDeadline > taskB->xNextDeadline) return 1;
	return 0; // Should ideally handle ties (e.g., by original priority or task ID)
}
*/

// EDF Scheduler Task Function (Improved Output)
static void vEdfSchedulerTask(void* pvParameters) {
	TickType_t xLastCheckTime;
	const TickType_t xCheckFrequency = pdMS_TO_TICKS(EDF_CHECK_PERIOD_MS);
	const UBaseType_t uxHighestEdfPriority = EDF_BASE_PRIORITY + NUM_EDF_TASKS - 1;
	int sortedIndices[NUM_EDF_TASKS];
	BaseType_t bPriorityChanged = pdFALSE; // Flag to track if any change happened

	printf("[EDFSched  ] Tick=%-5lu EDF Scheduler Task Started (Prio:%lu)\n",
		(unsigned long)xTaskGetTickCount(), (unsigned long)EDF_SCHEDULER_PRIORITY);
	fflush(stdout);

	xLastCheckTime = xTaskGetTickCount();

	for (;; ) {
		vTaskDelayUntil(&xLastCheckTime, xCheckFrequency);

		bPriorityChanged = pdFALSE; // Reset flag for this check

		if (xSemaphoreTake(xEdfMutex, portMAX_DELAY) == pdTRUE) {

			// 1. Populate index array
			for (int i = 0; i < NUM_EDF_TASKS; i++) { sortedIndices[i] = i; }

			// 2. Sort indices based on deadline in xEdfTasks (Bubble Sort)
			for (int i = 0; i < NUM_EDF_TASKS - 1; i++) {
				for (int j = 0; j < NUM_EDF_TASKS - i - 1; j++) {
					if (xEdfTasks[sortedIndices[j]].xNextDeadline > xEdfTasks[sortedIndices[j + 1]].xNextDeadline) {
						int temp = sortedIndices[j]; sortedIndices[j] = sortedIndices[j + 1]; sortedIndices[j + 1] = temp;
					}
					else if (xEdfTasks[sortedIndices[j]].xNextDeadline == xEdfTasks[sortedIndices[j + 1]].xNextDeadline) {
						if (sortedIndices[j] > sortedIndices[j + 1]) { // Tie-break by index
							int temp = sortedIndices[j]; sortedIndices[j] = sortedIndices[j + 1]; sortedIndices[j + 1] = temp;
						}
					}
				}
			}

			// 3. Assign Priorities only if changed
			for (int i = 0; i < NUM_EDF_TASKS; i++) {
				UBaseType_t uxNewPriority = uxHighestEdfPriority - i;
				int actualTaskIndex = sortedIndices[i];
				TaskHandle_t taskToChange = xEdfTasks[actualTaskIndex].xHandle;

				if (eTaskGetState(taskToChange) != eDeleted) {
					UBaseType_t currentPriority = uxTaskPriorityGet(taskToChange);
					if (currentPriority != uxNewPriority) {
						vTaskPrioritySet(taskToChange, uxNewPriority);
						// Print only when a priority IS changed
						if (!bPriorityChanged) { // Print header only on first change
							printf("[EDFSched  ] Tick=%-5lu --- EDF Priority Updates ---\n", (unsigned long)xTaskGetTickCount());
						}
						printf("[EDFSched  ]   Set %-12s Prio: %lu (Deadline: %lu)\n",
							xEdfTasks[actualTaskIndex].pcTaskName,
							(unsigned long)uxNewPriority,
							(unsigned long)xEdfTasks[actualTaskIndex].xNextDeadline);
						bPriorityChanged = pdTRUE; // Mark that a change occurred
					}
				}
			}

			// Optional: Print summary if priorities changed
			if (bPriorityChanged) {
				printf("[EDFSched  ]   New Order (Prio): ");
				for (int i = 0; i < NUM_EDF_TASKS; i++) {
					int idx = sortedIndices[i];
					printf("%s(%lu)%s", xEdfTasks[idx].pcTaskName, (unsigned long)uxTaskPriorityGet(xEdfTasks[idx].xHandle), (i < NUM_EDF_TASKS - 1) ? " > " : "");
				}
				printf("\n");
				fflush(stdout);
			}

			xSemaphoreGive(xEdfMutex);
		}
		else {
			printf("[EDFSched  ] Tick=%-5lu ERROR: Failed to get mutex!\r\n", (unsigned long)xTaskGetTickCount());
			fflush(stdout);
		}
	}
}


/*-----------------------------------------------------------*/
/*                Main Function (Modified for EDF)           */
/*-----------------------------------------------------------*/

int main(void)
{
	/* This demo uses heap_5.c, so start by defining some heap regions... */
	prvInitialiseHeap();

	/* Initialise the trace recorder.  Use of the trace recorder is optional.*/
	// vTraceEnable( TRC_START ); // Can enable if using Tracealyzer

	printf("--- System Starting ---\r\n");
	fflush(stdout);


	// Initialize sensors (random seed)
	initializeSensors();

	// Create the Mutex for EDF task data protection
	xEdfMutex = xSemaphoreCreateMutex();
	if (xEdfMutex == NULL) {
		printf("ERROR: Failed to create EDF Mutex!\r\n");
		fflush(stdout);
		// Halt or handle error appropriately
		while (1);
	}


	// Task Handles (needed to store in EdfTaskInfo_t)
	TaskHandle_t tempHandle = NULL;
	TaskHandle_t pressHandle = NULL;
	TaskHandle_t heightHandle = NULL;


	printf("Creating Sensor Tasks...\r\n");
	fflush(stdout);

	// Create Sensor Tasks, passing their index (0, 1, 2) as the parameter
	// Use SENSOR_TASK_INITIAL_PRIORITY
	xTaskCreate(vTemperatureTask,      // Function
		"TempTask",            // Name
		SENSOR_TASK_STACK_SIZE,// Stack size
		(void*)0,              // Parameter (index 0)
		SENSOR_TASK_INITIAL_PRIORITY, // Initial Priority
		&tempHandle);          // Task handle storage

	xTaskCreate(vPressureTask, "PressureTask", SENSOR_TASK_STACK_SIZE, (void*)1, SENSOR_TASK_INITIAL_PRIORITY, &pressHandle);
	xTaskCreate(vHeightTask, "HeightTask", SENSOR_TASK_STACK_SIZE, (void*)2, SENSOR_TASK_INITIAL_PRIORITY, &heightHandle);

	// Check if tasks were created successfully
	if (tempHandle == NULL || pressHandle == NULL || heightHandle == NULL) {
		printf("ERROR: Failed to create one or more sensor tasks!\r\n");
		fflush(stdout);
		while (1);
	}

	// Initialize the EdfTaskInfo array AFTER tasks are created
	TickType_t now = xTaskGetTickCount(); // Get current time for initial deadline calculation
	printf("Initializing EDF Task Info (Current Tick = %lu)...\r\n", (unsigned long)now);
	fflush(stdout);

	if (xSemaphoreTake(xEdfMutex, portMAX_DELAY) == pdTRUE) { // Protect initialization
		xEdfTasks[0] = (EdfTaskInfo_t){ tempHandle, pdMS_TO_TICKS(TEMP_TASK_PERIOD_MS), now + pdMS_TO_TICKS(TEMP_TASK_PERIOD_MS), "TempTask", 0 };
		xEdfTasks[1] = (EdfTaskInfo_t){ pressHandle, pdMS_TO_TICKS(PRESSURE_TASK_PERIOD_MS), now + pdMS_TO_TICKS(PRESSURE_TASK_PERIOD_MS), "PressureTask", 1 };
		xEdfTasks[2] = (EdfTaskInfo_t){ heightHandle, pdMS_TO_TICKS(HEIGHT_TASK_PERIOD_MS), now + pdMS_TO_TICKS(HEIGHT_TASK_PERIOD_MS), "HeightTask", 2 };
		xSemaphoreGive(xEdfMutex);
	}
	else {
		printf("ERROR: Failed to get mutex for EDF init!\r\n");
		fflush(stdout);
		while (1);
	}

	// Create the EDF Scheduler Task with the highest priority
	printf("Creating EDF Scheduler Task...\r\n");
	fflush(stdout);
	xTaskCreate(vEdfSchedulerTask,       // Function
		"EDFSched",              // Name
		SENSOR_TASK_STACK_SIZE,  // Stack size (can use same or more)
		NULL,                    // Parameter
		EDF_SCHEDULER_PRIORITY,  // The highest priority
		NULL);                  // Handle (not needed here)


	printf("--- Starting Scheduler ---\r\n");
	fflush(stdout);
	/* Start the scheduler. */
	vTaskStartScheduler();


	/* If all is well, the scheduler will now be running, and the following
	line will never be reached.  If the following line does execute, then
	there was insufficient FreeRTOS heap memory available for the idle and/or
	timer tasks	to be created.  See the memory management section on the
	FreeRTOS web site for more details. */
	for (;; );
	return 0; // Should not be reached
}
/*-----------------------------------------------------------*/

// ... [Rest of the standard hook functions vApplicationMallocFailedHook, vApplicationIdleHook, etc. remain unchanged] ...
// Make sure they compile, even if empty or using the #if guards for the full demo.

void vApplicationMallocFailedHook(void)
{
	vAssertCalled(__LINE__, __FILE__);
}
/*-----------------------------------------------------------*/

void vApplicationIdleHook(void)
{
#if ( mainCREATE_SIMPLE_BLINKY_DEMO_ONLY != 1 )
	{
		vFullDemoIdleFunction();
	}
#endif
}
/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook(TaskHandle_t pxTask, char* pcTaskName)
{
	(void)pcTaskName;
	(void)pxTask;
	vAssertCalled(__LINE__, __FILE__);
}
/*-----------------------------------------------------------*/

void vApplicationTickHook(void)
{
#if ( mainCREATE_SIMPLE_BLINKY_DEMO_ONLY != 1 )
	{
		vFullDemoTickHookFunction();
	}
#endif /* mainCREATE_SIMPLE_BLINKY_DEMO_ONLY */
}
/*-----------------------------------------------------------*/

void vApplicationDaemonTaskStartupHook(void)
{
	/* This function will be called once only, when the daemon task starts to
	execute	(sometimes called the timer task). */
}
/*-----------------------------------------------------------*/

void vAssertCalled(unsigned long ulLine, const char* const pcFileName)
{
	// ... [Assert function remains unchanged] ...
	static BaseType_t xPrinted = pdFALSE;
	volatile uint32_t ulSetToNonZeroInDebuggerToContinue = 0;
	(void)ulLine;
	(void)pcFileName;
	printf("ASSERT! Line %ld, file %s, GetLastError() %ld\r\n", ulLine, pcFileName, GetLastError());
	taskENTER_CRITICAL();
	{
		if (xPrinted == pdFALSE)
		{
			xPrinted = pdTRUE;
			if (xTraceRunning == pdTRUE)
			{
				// vTraceStop(); // Uncomment if using trace
				// prvSaveTraceFile(); // Uncomment if using trace
			}
		}
		__debugbreak();
		while (ulSetToNonZeroInDebuggerToContinue == 0)
		{
			__asm { NOP };
			__asm { NOP };
		}
	}
	taskEXIT_CRITICAL();
}
/*-----------------------------------------------------------*/

static void prvSaveTraceFile(void)
{
	// ... [Save trace file function remains unchanged - likely unused now] ...
#if ( configUSE_TRACE_FACILITY == 1 ) // Guard if trace is disabled
	FILE* pxOutputFile;
	fopen_s(&pxOutputFile, "Trace.dump", "wb");
	if (pxOutputFile != NULL)
	{
		// Note: RecorderDataPtr might not be defined if trace is not fully configured
		// fwrite( RecorderDataPtr, sizeof( RecorderDataType ), 1, pxOutputFile );
		fclose(pxOutputFile);
		printf("\r\nTrace output saved to Trace.dump\r\n");
	}
	else
	{
		printf("\r\nFailed to create trace dump file\r\n");
	}
#endif
}
/*-----------------------------------------------------------*/

static void  prvInitialiseHeap(void)
{
	// ... [Heap initialization remains unchanged] ...
	static uint8_t ucHeap[configTOTAL_HEAP_SIZE];
	volatile uint32_t ulAdditionalOffset = 19;
	const HeapRegion_t xHeapRegions[] =
	{
		{ ucHeap + 1,											mainREGION_1_SIZE },
		{ ucHeap + 15 + mainREGION_1_SIZE,						mainREGION_2_SIZE },
		{ ucHeap + 19 + mainREGION_1_SIZE + mainREGION_2_SIZE,	mainREGION_3_SIZE },
		{ NULL, 0 }
	};
	configASSERT((ulAdditionalOffset + mainREGION_1_SIZE + mainREGION_2_SIZE + mainREGION_3_SIZE) < configTOTAL_HEAP_SIZE);
	(void)ulAdditionalOffset;
	vPortDefineHeapRegions(xHeapRegions);
}
/*-----------------------------------------------------------*/

// ... [Static memory allocation hooks vApplicationGetIdleTaskMemory, vApplicationGetTimerTaskMemory remain unchanged] ...
// Make sure configSUPPORT_STATIC_ALLOCATION is 1 in FreeRTOSConfig.h if these are used
void vApplicationGetIdleTaskMemory(StaticTask_t** ppxIdleTaskTCBBuffer, StackType_t** ppxIdleTaskStackBuffer, uint32_t* pulIdleTaskStackSize)
{
	static StaticTask_t xIdleTaskTCB;
	static StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE];
	*ppxIdleTaskTCBBuffer = &xIdleTaskTCB;
	*ppxIdleTaskStackBuffer = uxIdleTaskStack;
	*pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}
/*-----------------------------------------------------------*/
void vApplicationGetTimerTaskMemory(StaticTask_t** ppxTimerTaskTCBBuffer, StackType_t** ppxTimerTaskStackBuffer, uint32_t* pulTimerTaskStackSize)
{
	static StaticTask_t xTimerTaskTCB;
	*ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
	*ppxTimerTaskStackBuffer = uxTimerTaskStack; // Re-uses the global declared earlier
	*pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}