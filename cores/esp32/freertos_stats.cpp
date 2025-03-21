// Copyright 2024 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "freertos_stats.h"
#include "sdkconfig.h"

#if CONFIG_FREERTOS_USE_TRACE_FACILITY
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/portable.h"
#endif /* CONFIG_FREERTOS_USE_TRACE_FACILITY */

void printRunningTasks(Print &printer) {
#if CONFIG_FREERTOS_USE_TRACE_FACILITY
#if CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
#define FREERTOS_TASK_NUMBER_MAX_NUM 256  // RunTime stats for how many Tasks to be stored
  static configRUN_TIME_COUNTER_TYPE ulRunTimeCounters[FREERTOS_TASK_NUMBER_MAX_NUM];
  static configRUN_TIME_COUNTER_TYPE ulLastRunTime = 0;
  configRUN_TIME_COUNTER_TYPE ulCurrentRunTime = 0, ulTaskRunTime = 0;
#endif
  configRUN_TIME_COUNTER_TYPE ulTotalRunTime = 0;
  TaskStatus_t *pxTaskStatusArray = NULL;
  volatile UBaseType_t uxArraySize = 0;
  uint32_t x = 0;
  const char *taskStates[] = {"Running", "Ready", "Blocked", "Suspended", "Deleted", "Invalid"};

  // Take a snapshot of the number of tasks in case it changes while this function is executing.
  uxArraySize = uxTaskGetNumberOfTasks();

  // Allocate a TaskStatus_t structure for each task.
  pxTaskStatusArray = (TaskStatus_t *)pvPortMalloc(uxArraySize * sizeof(TaskStatus_t));

  if (pxTaskStatusArray != NULL) {
    // Generate raw status information about each task.
    uxArraySize = uxTaskGetSystemState(pxTaskStatusArray, uxArraySize, &ulTotalRunTime);

#if CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
    ulCurrentRunTime = ulTotalRunTime - ulLastRunTime;
    ulLastRunTime = ulTotalRunTime;
#endif
    printer.printf(
      "Tasks: %u"
#if CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
      ", Runtime: %lus, Period: %luus"
#endif
      "\n",
      uxArraySize
#if CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
      ,
      ulTotalRunTime / 1000000, ulCurrentRunTime
#endif
    );
    printer.printf("Num\t            Name"
#if CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
                   "\tLoad"
#endif
                   "\tPrio\t Free"
#if CONFIG_FREERTOS_VTASKLIST_INCLUDE_COREID
                   "\tCore"
#endif
                   "\tState\r\n");
    for (x = 0; x < uxArraySize; x++) {
#if CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
      if (pxTaskStatusArray[x].xTaskNumber < FREERTOS_TASK_NUMBER_MAX_NUM) {
        ulTaskRunTime = (pxTaskStatusArray[x].ulRunTimeCounter - ulRunTimeCounters[pxTaskStatusArray[x].xTaskNumber]);
        ulRunTimeCounters[pxTaskStatusArray[x].xTaskNumber] = pxTaskStatusArray[x].ulRunTimeCounter;
        ulTaskRunTime = (ulTaskRunTime * 100) / ulCurrentRunTime;  // in percentage
      } else {
        ulTaskRunTime = 0;
      }
#endif
      printer.printf(
        "%3u\t%16s"
#if CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
        "\t%3lu%%"
#endif
        "\t%4u\t%5lu"
#if CONFIG_FREERTOS_VTASKLIST_INCLUDE_COREID
        "\t%4c"
#endif
        "\t%s\r\n",
        pxTaskStatusArray[x].xTaskNumber, pxTaskStatusArray[x].pcTaskName,
#if CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
        ulTaskRunTime,
#endif
        pxTaskStatusArray[x].uxCurrentPriority, pxTaskStatusArray[x].usStackHighWaterMark,
#if CONFIG_FREERTOS_VTASKLIST_INCLUDE_COREID
        (pxTaskStatusArray[x].xCoreID == tskNO_AFFINITY) ? '*' : ('0' + pxTaskStatusArray[x].xCoreID),
#endif
        taskStates[pxTaskStatusArray[x].eCurrentState]
      );
    }

    // The array is no longer needed, free the memory it consumes.
    vPortFree(pxTaskStatusArray);
    printer.println();
  }
#else
  printer.println("FreeRTOS trace facility is not enabled.");
#endif /* CONFIG_FREERTOS_USE_TRACE_FACILITY */
}
