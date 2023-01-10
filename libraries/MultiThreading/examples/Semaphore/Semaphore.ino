/*
  This example demonstrates basic usage of FreeRTOS Semaphores and queue sets for coordination between tasks for multi threading.
  Please refer to other examples in this folder to better understand usage of tasks.
  It is also advised to read documentation on FreeRTOS web pages:
  https://www.freertos.org/a00106.html

  Theory:
  Semaphore is in essence a variable. Tasks can set the value, wait until one or more
  semaphores are set and thus communicate between each other their state.
  A binary semaphore is a semaphore that has a maximum count of 1, hence the 'binary' name.
  A task can only 'take' the semaphore if it is available, and the semaphore is only available if its count is 1.

  Semaphore can be controlled from any number of tasks. If you use semaphore as a one-way
  signalization with only one task giving and only one task taking there is much faster option
  called Task Notifications - please see FreeRTOS documentation read more about them: https://www.freertos.org/RTOS-task-notifications.html

  The example:
  We'll use a semaphore to signal when a package is delivered to a warehouse by multiple
  delivery trucks, and multiple workers are waiting to receive the package.
*/
#include <Arduino.h>


SemaphoreHandle_t package_delivered_semaphore;

void delivery_truck_task(void *pvParameters) {
    int truck_number = (int) pvParameters;
    while(1) {
        // Wait for a package to be delivered
        // ...
        // Notify the warehouse that a package has been delivered
        xSemaphoreGive(package_delivered_semaphore);
        Serial.print("Package delivered by truck: ");
        Serial.println(truck_number);
        //wait for some time
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void warehouse_worker_task(void *pvParameters) {
    int worker_number = (int) pvParameters;
    while(1) {
        // Wait for a package to be delivered
        xSemaphoreTake(package_delivered_semaphore, portMAX_DELAY);
        Serial.print("Package received by worker: ");
        Serial.println(worker_number);
        // Receive the package
        // ...
    }
}

void setup() {
    Serial.begin(115200);
    // Create the semaphore
    package_delivered_semaphore = xSemaphoreCreateCounting(10, 0);

    // Create multiple delivery truck tasks
    for (int i = 0; i < 5; i++) {
        xTaskCreate(delivery_truck_task, "Delivery Truck", 1024, (void *)i, tskIDLE_PRIORITY, NULL);
    }

    // Create multiple warehouse worker tasks
    for (int i = 0; i < 3; i++) {
        xTaskCreate(warehouse_worker_task, "Warehouse Worker", 1024, (void *)i, tskIDLE_PRIORITY, NULL);
    }

    // Start the RTOS scheduler
    vTaskStartScheduler();
}

void loop() {
    // Empty loop
}