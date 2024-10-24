#include "freertos_stats.h"
#include "sdkconfig.h"
//#undef CONFIG_FREERTOS_VTASKLIST_INCLUDE_COREID
//#undef CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS

#if CONFIG_FREERTOS_USE_TRACE_FACILITY
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/portable.h"
#endif /* CONFIG_FREERTOS_USE_TRACE_FACILITY */

void printRunningTasks(Print & printer) {
#if CONFIG_FREERTOS_USE_TRACE_FACILITY
#if CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
#define FREERTOS_TASK_NUMBER_MAX_NUM 256
  static UBaseType_t ulRunTimeCounters[FREERTOS_TASK_NUMBER_MAX_NUM];
  static configRUN_TIME_COUNTER_TYPE ulLastRunTime = 0;
#endif
  TaskStatus_t *pxTaskStatusArray = NULL;
  volatile UBaseType_t uxArraySize = 0, x = 0;
  configRUN_TIME_COUNTER_TYPE ulTotalRunTime = 0, uiCurrentRunTime = 0, uiTaskRunTime = 0;
  const char * taskStates[] = {
    "Running",
    "Ready",
    "Blocked",
    "Suspended",
    "Deleted",
    "Invalid"
  };

  // Take a snapshot of the number of tasks in case it changes while this function is executing.
  uxArraySize = uxTaskGetNumberOfTasks();
  //printer.printf("Running tasks: %u\n", uxArraySize);

  // Allocate a TaskStatus_t structure for each task.
  pxTaskStatusArray = (TaskStatus_t*)pvPortMalloc( uxArraySize * sizeof( TaskStatus_t ) );

  if( pxTaskStatusArray != NULL ) {
    // Generate raw status information about each task.
    uxArraySize = uxTaskGetSystemState( pxTaskStatusArray, uxArraySize, &ulTotalRunTime );
#if CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
    uiCurrentRunTime = ulTotalRunTime - ulLastRunTime;
    ulLastRunTime = ulTotalRunTime;
#endif
    printer.printf("Tasks: %u"
#if CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
      ", Runtime: %us, Period: %uus"
#endif
      "\n", uxArraySize
#if CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
      , ulTotalRunTime / 1000000, uiCurrentRunTime
#endif
    );
    printer.printf("Num\t            Name"
#if CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
      "\t     Load"
#endif
      "\tPrio\t Free"
#if CONFIG_FREERTOS_VTASKLIST_INCLUDE_COREID
      "\tCore"
#endif
      "\tState\r\n");
    for( x = 0; x < uxArraySize; x++ ) {
#if CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
      if (pxTaskStatusArray[ x ].xTaskNumber < FREERTOS_TASK_NUMBER_MAX_NUM) {
        uiTaskRunTime = (pxTaskStatusArray[ x ].ulRunTimeCounter - ulRunTimeCounters[pxTaskStatusArray[ x ].xTaskNumber]);
        ulRunTimeCounters[pxTaskStatusArray[ x ].xTaskNumber] = pxTaskStatusArray[ x ].ulRunTimeCounter;
        uiTaskRunTime = (uiTaskRunTime * 100) / uiCurrentRunTime; // in percentage
      } else {
        uiTaskRunTime = 0;
      }
#endif
      printer.printf("%3u\t%16s"
#if CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
      "\t%8lu%%"
#endif
      "\t%4u\t%5lu"
#if CONFIG_FREERTOS_VTASKLIST_INCLUDE_COREID
      "\t%4c"
#endif
      "\t%s\r\n",
        pxTaskStatusArray[ x ].xTaskNumber,
        pxTaskStatusArray[ x ].pcTaskName,
#if CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
        uiTaskRunTime,
#endif
        pxTaskStatusArray[ x ].uxCurrentPriority,
        pxTaskStatusArray[ x ].usStackHighWaterMark,
#if CONFIG_FREERTOS_VTASKLIST_INCLUDE_COREID
        (pxTaskStatusArray[ x ].xCoreID == tskNO_AFFINITY)?'*':('0'+pxTaskStatusArray[ x ].xCoreID),
#endif
        taskStates[pxTaskStatusArray[ x ].eCurrentState]
      );
    }

    // The array is no longer needed, free the memory it consumes.
    vPortFree( pxTaskStatusArray );
    printer.println();
  }
#endif /* CONFIG_FREERTOS_USE_TRACE_FACILITY */
}
