/*
 * Multitasking Validation Test
 *
 * Covers: FreeRTOS task creation, dual-core pinning (ESP32/S3),
 * portMUX spinlock, binary semaphore give/take between tasks,
 * task notification, and task deletion.
 */

#include <Arduino.h>
#include <unity.h>

#define TASK_STACK_SIZE 4096
#define INCREMENT_COUNT 100000
#define TASK_TIMEOUT_MS 5000

void setUp(void) {}
void tearDown(void) {}

// ==================== Task Creation ====================

static volatile bool task_ran = false;

static void simpleTask(void *param) {
  task_ran = true;
  vTaskDelete(NULL);
}

void test_task_create(void) {
  task_ran = false;
  BaseType_t ret = xTaskCreate(simpleTask, "simple", TASK_STACK_SIZE, NULL, 1, NULL);
  TEST_ASSERT_EQUAL(pdPASS, ret);
  unsigned long start = millis();
  while (!task_ran && millis() - start < TASK_TIMEOUT_MS) {
    delay(10);
  }
  TEST_ASSERT_TRUE(task_ran);
}

// ==================== Dual-Core Pinning ====================

#if CONFIG_FREERTOS_NUMBER_OF_CORES > 1
static volatile int task_core_0 = -1;
static volatile int task_core_1 = -1;

static void coreTask0(void *param) {
  task_core_0 = xPortGetCoreID();
  vTaskDelete(NULL);
}

static void coreTask1(void *param) {
  task_core_1 = xPortGetCoreID();
  vTaskDelete(NULL);
}

void test_dual_core_pinning(void) {
  task_core_0 = -1;
  task_core_1 = -1;

  xTaskCreatePinnedToCore(coreTask0, "core0", TASK_STACK_SIZE, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(coreTask1, "core1", TASK_STACK_SIZE, NULL, 1, NULL, 1);

  unsigned long start = millis();
  while ((task_core_0 < 0 || task_core_1 < 0) && millis() - start < TASK_TIMEOUT_MS) {
    delay(10);
  }

  TEST_ASSERT_EQUAL(0, task_core_0);
  TEST_ASSERT_EQUAL(1, task_core_1);
}
#endif

// ==================== portMUX Spinlock ====================

static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
static int shared_counter = 0;
static volatile bool mux_task_done_a = false;
static volatile bool mux_task_done_b = false;

static void muxTaskA(void *param) {
  for (int i = 0; i < INCREMENT_COUNT; i++) {
    portENTER_CRITICAL(&mux);
    shared_counter++;
    portEXIT_CRITICAL(&mux);
  }
  mux_task_done_a = true;
  vTaskDelete(NULL);
}

static void muxTaskB(void *param) {
  for (int i = 0; i < INCREMENT_COUNT; i++) {
    portENTER_CRITICAL(&mux);
    shared_counter++;
    portEXIT_CRITICAL(&mux);
  }
  mux_task_done_b = true;
  vTaskDelete(NULL);
}

void test_portmux_no_corruption(void) {
  shared_counter = 0;
  mux_task_done_a = false;
  mux_task_done_b = false;

#if CONFIG_FREERTOS_NUMBER_OF_CORES > 1
  xTaskCreatePinnedToCore(muxTaskA, "muxA", TASK_STACK_SIZE, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(muxTaskB, "muxB", TASK_STACK_SIZE, NULL, 1, NULL, 1);
#else
  xTaskCreate(muxTaskA, "muxA", TASK_STACK_SIZE, NULL, 1, NULL);
  xTaskCreate(muxTaskB, "muxB", TASK_STACK_SIZE, NULL, 1, NULL);
#endif

  unsigned long start = millis();
  while ((!mux_task_done_a || !mux_task_done_b) && millis() - start < TASK_TIMEOUT_MS) {
    delay(10);
  }

  TEST_ASSERT_TRUE(mux_task_done_a);
  TEST_ASSERT_TRUE(mux_task_done_b);
  TEST_ASSERT_EQUAL(INCREMENT_COUNT * 2, shared_counter);
}

// ==================== Binary Semaphore ====================

static SemaphoreHandle_t sem = NULL;
static volatile int sem_value = 0;

static void semProducer(void *param) {
  for (int i = 0; i < 5; i++) {
    sem_value = i + 1;
    xSemaphoreGive(sem);
    delay(10);
  }
  vTaskDelete(NULL);
}

static volatile bool sem_consumer_done = false;
static volatile int sem_last_value = 0;

static void semConsumer(void *param) {
  for (int i = 0; i < 5; i++) {
    if (xSemaphoreTake(sem, pdMS_TO_TICKS(TASK_TIMEOUT_MS)) == pdTRUE) {
      sem_last_value = sem_value;
    }
  }
  sem_consumer_done = true;
  vTaskDelete(NULL);
}

void test_semaphore(void) {
  sem = xSemaphoreCreateBinary();
  TEST_ASSERT_NOT_NULL(sem);
  sem_value = 0;
  sem_last_value = 0;
  sem_consumer_done = false;

  xTaskCreate(semConsumer, "consumer", TASK_STACK_SIZE, NULL, 1, NULL);
  xTaskCreate(semProducer, "producer", TASK_STACK_SIZE, NULL, 1, NULL);

  unsigned long start = millis();
  while (!sem_consumer_done && millis() - start < TASK_TIMEOUT_MS) {
    delay(10);
  }

  TEST_ASSERT_TRUE(sem_consumer_done);
  TEST_ASSERT_EQUAL(5, sem_last_value);
  vSemaphoreDelete(sem);
  sem = NULL;
}

// ==================== Task Notification ====================

static volatile bool notify_received = false;
static volatile uint32_t notify_value = 0;

static void notifyWaiter(void *param) {
  uint32_t val = 0;
  if (xTaskNotifyWait(0, ULONG_MAX, &val, pdMS_TO_TICKS(TASK_TIMEOUT_MS)) == pdTRUE) {
    notify_value = val;
    notify_received = true;
  }
  vTaskDelete(NULL);
}

void test_task_notification(void) {
  notify_received = false;
  notify_value = 0;

  TaskHandle_t waiter = NULL;
  xTaskCreate(notifyWaiter, "waiter", TASK_STACK_SIZE, NULL, 1, &waiter);
  TEST_ASSERT_NOT_NULL(waiter);

  delay(50);
  xTaskNotify(waiter, 0xBEEF, eSetValueWithOverwrite);

  unsigned long start = millis();
  while (!notify_received && millis() - start < TASK_TIMEOUT_MS) {
    delay(10);
  }

  TEST_ASSERT_TRUE(notify_received);
  TEST_ASSERT_EQUAL_HEX32(0xBEEF, notify_value);
}

// ==================== Queue ====================

static QueueHandle_t test_queue = NULL;

static void queueProducer(void *param) {
  for (int i = 1; i <= 10; i++) {
    xQueueSend(test_queue, &i, pdMS_TO_TICKS(1000));
    delay(5);
  }
  vTaskDelete(NULL);
}

void test_queue_send_receive(void) {
  test_queue = xQueueCreate(10, sizeof(int));
  TEST_ASSERT_NOT_NULL(test_queue);

  xTaskCreate(queueProducer, "qProducer", TASK_STACK_SIZE, NULL, 1, NULL);

  int received_sum = 0;
  int val;
  for (int i = 0; i < 10; i++) {
    if (xQueueReceive(test_queue, &val, pdMS_TO_TICKS(TASK_TIMEOUT_MS)) == pdTRUE) {
      received_sum += val;
    }
  }
  TEST_ASSERT_EQUAL(55, received_sum);

  vQueueDelete(test_queue);
  test_queue = NULL;
}

// ==================== Mutex ====================

static SemaphoreHandle_t test_mutex = NULL;
static int mutex_counter = 0;
static volatile bool mutex_task_a_done = false;
static volatile bool mutex_task_b_done = false;

static void mutexTaskA(void *param) {
  for (int i = 0; i < 1000; i++) {
    xSemaphoreTake(test_mutex, portMAX_DELAY);
    mutex_counter++;
    xSemaphoreGive(test_mutex);
  }
  mutex_task_a_done = true;
  vTaskDelete(NULL);
}

static void mutexTaskB(void *param) {
  for (int i = 0; i < 1000; i++) {
    xSemaphoreTake(test_mutex, portMAX_DELAY);
    mutex_counter++;
    xSemaphoreGive(test_mutex);
  }
  mutex_task_b_done = true;
  vTaskDelete(NULL);
}

void test_mutex_no_corruption(void) {
  test_mutex = xSemaphoreCreateMutex();
  TEST_ASSERT_NOT_NULL(test_mutex);
  mutex_counter = 0;
  mutex_task_a_done = false;
  mutex_task_b_done = false;

  xTaskCreate(mutexTaskA, "mutA", TASK_STACK_SIZE, NULL, 1, NULL);
  xTaskCreate(mutexTaskB, "mutB", TASK_STACK_SIZE, NULL, 1, NULL);

  unsigned long start = millis();
  while ((!mutex_task_a_done || !mutex_task_b_done) && millis() - start < TASK_TIMEOUT_MS) {
    delay(10);
  }

  TEST_ASSERT_TRUE(mutex_task_a_done);
  TEST_ASSERT_TRUE(mutex_task_b_done);
  TEST_ASSERT_EQUAL(2000, mutex_counter);

  vSemaphoreDelete(test_mutex);
  test_mutex = NULL;
}

// ==================== Event Group ====================

#define EVT_BIT_A (1 << 0)
#define EVT_BIT_B (1 << 1)

static EventGroupHandle_t evt_group = NULL;
static volatile bool evt_waiter_done = false;
static volatile EventBits_t evt_result = 0;

static void evtWaiter(void *param) {
  evt_result = xEventGroupWaitBits(evt_group, EVT_BIT_A | EVT_BIT_B, pdTRUE, pdTRUE, pdMS_TO_TICKS(TASK_TIMEOUT_MS));
  evt_waiter_done = true;
  vTaskDelete(NULL);
}

void test_event_group(void) {
  evt_group = xEventGroupCreate();
  TEST_ASSERT_NOT_NULL(evt_group);
  evt_waiter_done = false;
  evt_result = 0;

  xTaskCreate(evtWaiter, "evtWait", TASK_STACK_SIZE, NULL, 1, NULL);
  delay(50);

  xEventGroupSetBits(evt_group, EVT_BIT_A);
  delay(50);
  xEventGroupSetBits(evt_group, EVT_BIT_B);

  unsigned long start = millis();
  while (!evt_waiter_done && millis() - start < TASK_TIMEOUT_MS) {
    delay(10);
  }

  TEST_ASSERT_TRUE(evt_waiter_done);
  TEST_ASSERT_BITS(EVT_BIT_A | EVT_BIT_B, EVT_BIT_A | EVT_BIT_B, evt_result);

  vEventGroupDelete(evt_group);
  evt_group = NULL;
}

// ==================== Stack Watermark ====================

static volatile UBaseType_t watermark_result = 0;

static void stackWatermarkTask(void *param) {
  char buf[256];
  memset(buf, 0xAA, sizeof(buf));
  watermark_result = uxTaskGetStackHighWaterMark(NULL);
  vTaskDelete(NULL);
}

void test_stack_watermark(void) {
  watermark_result = 0;
  xTaskCreate(stackWatermarkTask, "watermark", TASK_STACK_SIZE, NULL, 1, NULL);

  unsigned long start = millis();
  while (watermark_result == 0 && millis() - start < TASK_TIMEOUT_MS) {
    delay(10);
  }

  // Task used 256 bytes of stack for buf + overhead, watermark should be > 0 and < TASK_STACK_SIZE
  TEST_ASSERT_GREATER_THAN(0, watermark_result);
  TEST_ASSERT_LESS_THAN(TASK_STACK_SIZE / sizeof(StackType_t), watermark_result);
}

// ==================== Task Delete ====================

static volatile bool delete_task_started = false;

static void deletableTask(void *param) {
  delete_task_started = true;
  while (true) {
    delay(100);
  }
}

void test_task_delete(void) {
  delete_task_started = false;
  TaskHandle_t handle = NULL;
  xTaskCreate(deletableTask, "deletable", TASK_STACK_SIZE, NULL, 1, &handle);
  TEST_ASSERT_NOT_NULL(handle);

  unsigned long start = millis();
  while (!delete_task_started && millis() - start < TASK_TIMEOUT_MS) {
    delay(10);
  }
  TEST_ASSERT_TRUE(delete_task_started);

  vTaskDelete(handle);
  delay(100);
}

// ==================== Main ====================

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  UNITY_BEGIN();

  RUN_TEST(test_task_create);
#if CONFIG_FREERTOS_NUMBER_OF_CORES > 1
  RUN_TEST(test_dual_core_pinning);
#endif
  RUN_TEST(test_portmux_no_corruption);
  RUN_TEST(test_semaphore);
  RUN_TEST(test_task_notification);
  RUN_TEST(test_queue_send_receive);
  RUN_TEST(test_mutex_no_corruption);
  RUN_TEST(test_event_group);
  RUN_TEST(test_stack_watermark);
  RUN_TEST(test_task_delete);

  UNITY_END();
}

void loop() {}
