# Multitasking Validation Test

Validates FreeRTOS primitives via the Arduino framework: task creation, dual-core pinning, portMUX spinlock, binary semaphore, task notification, queue, mutex, event group, stack watermark, and task deletion.

## Test Cases

| Test Function | Description |
|---|---|
| `test_task_create` | Create a task, verify it runs |
| `test_dual_core_pinning` | Pin tasks to core 0 and core 1, verify `xPortGetCoreID()` (multi-core only) |
| `test_portmux_no_corruption` | Two tasks increment shared counter with portMUX, verify no lost increments |
| `test_semaphore` | Binary semaphore producer/consumer, verify all values received |
| `test_task_notification` | Send notification with value, verify receiver gets it |
| `test_queue_send_receive` | Queue producer sends 1-10, consumer verifies sum = 55 |
| `test_mutex_no_corruption` | Two tasks increment counter with mutex, verify count = 2000 |
| `test_event_group` | Set bits from main task, verify waiter receives both bits |
| `test_stack_watermark` | Verify `uxTaskGetStackHighWaterMark()` returns sensible value |
| `test_task_delete` | Create then delete a running task |

## Requirements

- **Hardware**: Any ESP32 variant
- **Wokwi**: Supported
- **QEMU**: Supported

## Notes

- Dual-core pinning test only runs on multi-core SoCs (ESP32, ESP32-S3).
- All tests use a 5-second timeout to avoid infinite hangs.
- portMUX test uses 100,000 iterations per task for reliable corruption detection.
