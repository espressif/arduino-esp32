/*
 * FunctionQueue.cpp
 *
 *  Created on: 9 jun. 2018
 *      Author: Herman
 */

#include <FunctionQueue.h>
#include <functional>
#include "Arduino.h"
#include "freertos/event_groups.h"

EventGroupHandle_t FunctionQueue::loopEventHandle = xEventGroupCreate();
uint8_t FunctionQueue::allSynced = 0;
SemaphoreHandle_t FunctionQueue::syncedMutex = xSemaphoreCreateMutex();


FunctionQueue::FunctionQueue()
:FunctionQueue(0)
{
}

FunctionQueue::FunctionQueue(uint8_t reqSyncIndex)
:syncIndex(reqSyncIndex)
{
	if (syncIndex != 0)
	{
		xSemaphoreTake(syncedMutex, portMAX_DELAY);
		allSynced |= (1u << syncIndex);
		xSemaphoreGive(syncedMutex);
	}

	functionQueue = xQueueCreate( 10,sizeof(QueuedItem*) );
	// ARDUINO_RUNNING_CORE is defined in main.cpp core is here hardcoded to 1
	// Priority 1 is identical to the prio of the looptask
	xTaskCreatePinnedToCore(staticFunction, "FunctionQueue", 8192, this, 1, &this->functionTask, 1);
}

FunctionQueue::~FunctionQueue()
{
	if (syncIndex != 0)
	{
		xSemaphoreTake(syncedMutex, portMAX_DELAY);
		allSynced &= ~(1u << syncIndex);
		xSemaphoreGive(syncedMutex);
	}
	vQueueDelete(functionQueue);
	vTaskDelete(functionTask);
}

bool FunctionQueue::scheduleFunction(std::function<void(void)> sf)
{
	QueuedItem* queuedItem = new QueuedItem;
	queuedItem->queuedFunction = sf;
	xQueueSendToBack(functionQueue,&queuedItem,portMAX_DELAY);

	return true;
}

void FunctionQueue::staticFunction(void* pvParameters)
{
	FunctionQueue* _this = static_cast<FunctionQueue*>(pvParameters);
    for (;;) {
    	if (_this->syncIndex != 0)
    	{
    		uint16_t er = xEventGroupSync(loopEventHandle,(1u << _this->syncIndex),0x01,portMAX_DELAY);
    		int mc = uxQueueMessagesWaiting(_this->functionQueue);
    		// only run already queued functions to allow recursive
    		while (mc-- > 0)
    		{
    	     	QueuedItem* queuedItem = nullptr;
    	        if (xQueueReceive(_this->functionQueue, &queuedItem, portMAX_DELAY) == pdTRUE){
    	        	queuedItem->queuedFunction();
    	        	delete queuedItem;
    	        }
    		}
    	}
    	else
    	{
         	QueuedItem* queuedItem = nullptr;
            if (xQueueReceive(_this->functionQueue, &queuedItem, portMAX_DELAY) == pdTRUE){
            	queuedItem->queuedFunction();
            	delete queuedItem;
            }
    	}
    }
    vTaskDelete(NULL);
}

FunctionQueue FQ;

