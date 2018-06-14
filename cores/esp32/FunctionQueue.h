/*
 * FunctionQueue.h
 *
 *  Created on: 9 jun. 2018
 *      Author: Herman
 */

#ifndef CORE_CORE_FUNCTIONQUEUE_H_
#define CORE_CORE_FUNCTIONQUEUE_H_

#include <functional>

extern "C"
{
#include "freertos\freeRTOS.h"
#include "freertos\task.h"
#include "freertos\queue.h"
#include "freertos\semphr.h"
#include "freertos/event_groups.h"
}

class FunctionQueue {
public:

	struct QueuedItem
	{
		std::function<void(void)> queuedFunction;
	};

	FunctionQueue();
	FunctionQueue(uint8_t);
	virtual ~FunctionQueue();

	static void staticFunction(void* pvParameters);
	static EventGroupHandle_t loopEventHandle;
	static uint8_t allSynced;
	static SemaphoreHandle_t syncedMutex;

	bool scheduleFunction(std::function<void(void)>);

	uint8_t syncIndex;

	QueueHandle_t functionQueue;
	TaskHandle_t  functionTask;

};

extern FunctionQueue FQ;

#endif /* CORE_CORE_FUNCTIONQUEUE_H_ */
