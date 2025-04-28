/* Standard includes. */
#include <stdio.h>
#include <stdlib.h> // For exit()
#include <conio.h>  // For _getch()
#include <ctype.h>  // For toupper()
#include <intrin.h>

/* FreeRTOS kernel includes. */
#include "FreeRTOS.h"
#include "task.h"

// Include headers for the AVAILABLE demo setup functions
#include "edf_demo.h"
#include "rm_rcs_demo.h" // Only EDF and RM-RCS File demo remain

// --- Only common stuff needed by FreeRTOS hooks remains ---
// (e.g., heap_5 definitions, hook function prototypes/implementations)

// Example: Keep prvInitialiseHeap if using heap_5
#if ( configMEMMANG_HEAP_NB == 5 )
#define mainREGION_1_SIZE	8201 /* Adjust sizes as needed */
#define mainREGION_2_SIZE	29905
#define mainREGION_3_SIZE	7607
static void  prvInitialiseHeap(void);
#endif

// Prototypes for standard FreeRTOS hooks (implement them below or keep defaults)
void vApplicationMallocFailedHook(void);
void vApplicationIdleHook(void);
void vApplicationStackOverflowHook(TaskHandle_t pxTask, char* pcTaskName);
void vApplicationTickHook(void);
void vApplicationGetIdleTaskMemory(StaticTask_t** ppxIdleTaskTCBBuffer, StackType_t** ppxIdleTaskStackBuffer, uint32_t* pulIdleTaskStackSize);
void vApplicationGetTimerTaskMemory(StaticTask_t** ppxTimerTaskTCBBuffer, StackType_t** ppxTimerTaskStackBuffer, uint32_t* pulTimerTaskStackSize);
void vAssertCalled(unsigned long ulLine, const char* const pcFileName);

/*-----------------------------------------------------------*/

int main(void)
{
#if ( configMEMMANG_HEAP_NB == 5 )
    prvInitialiseHeap();
#endif

    int choice = 0;

    // Simple console menu for selection (Updated Menu)
    printf("----------------------------------------\n");
    printf(" Select Scheduling Algorithm Demo:\n");
    printf("----------------------------------------\n");
    printf("  1. Earliest Deadline First (EDF - Sensor Tasks)\n");
    printf("  2. Rate Monotonic RCS (RM-RCS - File Input) Removed from here, please see separate c implementation\n"); // Re-numbered
    printf("----------------------------------------\n");
    printf("Enter choice (1-2): "); // Updated prompt

    // Basic input loop (Updated Range)
    while (choice < 1 || choice > 2) { // Check for 1 or 2
        char inputChar = _getch(); // Get character without waiting for Enter
        if (inputChar >= '1' && inputChar <= '2') { // Check against '1' and '2'
            choice = inputChar - '0'; // Convert char '1' or '2' to int
            printf("%c\n", inputChar); // Echo valid choice
        }
        else if (inputChar == '\r' || inputChar == '\n') {
            printf("\nPlease enter 1 or 2: ");
        }
        else {
            printf("\nInvalid input. Please enter 1 or 2: ");
        }
    }

    printf("\n--- Initializing Demo %d ---\n", choice);
    fflush(stdout);

    // Call the appropriate setup function based on choice (Updated Switch)
    switch (choice) {
    case 1:
        printf("Starting EDF Demo...\n");
        start_edf_demo();
        break;
    case 2: // Now case 2 runs RM-RCS
        printf("Starting RM-RCS (File Input) Demo...\n");
        start_rcs_file_demo(); // Make sure tasks.txt exists!
        break;
    default:
        // Should not happen due to input loop
        printf("ERROR: Invalid selection somehow.\n");
        exit(1);
    }

    printf("\n--- Starting Scheduler ---\n");
    fflush(stdout);
    vTaskStartScheduler();

    // Scheduler should not return
    printf("Scheduler returned. Halting.\n");
    fflush(stdout);
    for (;; );
    return 0;
}

/*-----------------------------------------------------------*/
// --- Implement common hook functions and prvInitialiseHeap below ---
// (Keep the standard implementations you had before)

// Example prvInitialiseHeap for heap_5

#if ( configMEMMANG_HEAP_NB == 5 )
static void  prvInitialiseHeap(void)
{
    // The total heap space provided to the kernel.
    static uint8_t ucHeap[configTOTAL_HEAP_SIZE];

    // Define the heap regions. For this simple case, we will define
    // only one region, using the entire ucHeap array. This is functionally
    // similar to using heap_4.c, but demonstrates heap_5 structure.
    // You could divide ucHeap into multiple regions if needed.
    const HeapRegion_t xHeapRegions[] =
    {
        // Make one large heap region starting at the beginning of ucHeap.
        // Ensure the size doesn't exceed the actual array size.
        { ucHeap, configTOTAL_HEAP_SIZE },

        // Mark the end of the regions array.
        { NULL, 0 }
    };

    // Pass the array of heap regions to the kernel.
    vPortDefineHeapRegions(xHeapRegions);

    // No complex sanity checks needed here as we use the whole block.
    // The kernel's internal checks are sufficient.
}
#endif // configMEMMANG_HEAP_NB

// Implement or keep stubs for the required hook functions
void vApplicationMallocFailedHook(void) { /* Handle error */ vAssertCalled(__LINE__, __FILE__); }
void vApplicationIdleHook(void) { /* Optional */ }
void vApplicationStackOverflowHook(TaskHandle_t pxTask, char* pcTaskName) { (void)pxTask; (void)pcTaskName; /* Handle error */ vAssertCalled(__LINE__, __FILE__); }
void vApplicationTickHook(void) { /* Optional */ }
#if ( configUSE_TIMERS == 1 ) // Only define if timers are enabled


void vApplicationDaemonTaskStartupHook(void)
{
    // This hook is called once when the timer/daemon task starts.
    // Can be left empty if no specific startup action is needed after the
    // scheduler starts but before timers are used by application tasks.
    // printf("Daemon Task Hook Called.\n"); // Optional debug message
}
#endif // configUSE_TIMERS


// Implement static allocation hooks if configSUPPORT_STATIC_ALLOCATION = 1
#if ( configSUPPORT_STATIC_ALLOCATION == 1 )
// Keep implementations from previous main.c if needed
void vApplicationGetIdleTaskMemory(StaticTask_t** ppxIdleTaskTCBBuffer, StackType_t** ppxIdleTaskStackBuffer, uint32_t* pulIdleTaskStackSize)
{
    static StaticTask_t xIdleTaskTCB;
    static StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE];
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

#if ( configUSE_TIMERS == 1 ) // Guard required for timer task memory
void vApplicationGetTimerTaskMemory(StaticTask_t** ppxTimerTaskTCBBuffer, StackType_t** ppxTimerTaskStackBuffer, uint32_t* pulTimerTaskStackSize)
{
    static StaticTask_t xTimerTaskTCB;
    extern StackType_t uxTimerTaskStack[]; // Ensure this is declared globally if used
    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}
#endif // configUSE_TIMERS
#endif // configSUPPORT_STATIC_ALLOCATION

// Keep vAssertCalled implementation
void vAssertCalled(unsigned long ulLine, const char* const pcFileName)
{
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
            // Disable trace saving if trace is disabled
#if ( configUSE_TRACE_FACILITY == 1 )
    // extern BaseType_t xTraceRunning; // Assume declared elsewhere if trace used
    // if( xTraceRunning == pdTRUE ) { vTraceStop(); /* prvSaveTraceFile(); */ }
#endif
        }
        __debugbreak();
        while (ulSetToNonZeroInDebuggerToContinue == 0) { __asm { NOP }; __asm { NOP }; }
    }
    taskEXIT_CRITICAL();
}

// You can likely remove prvSaveTraceFile completely if trace is disabled
// static void prvSaveTraceFile( void ) { /* ... */ }

// Also ensure the global uxTimerTaskStack is declared if needed by vApplicationGetTimerTaskMemory
#if ( configSUPPORT_STATIC_ALLOCATION == 1 && configUSE_TIMERS == 1 )
StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH];
#endif