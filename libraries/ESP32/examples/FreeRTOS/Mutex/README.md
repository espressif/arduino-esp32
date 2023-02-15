# Mutex Example

This example demonstrates the basic usage of FreeRTOS Mutually Exclusive Locks (Mutex) for securing access to shared resources in multi-threading.
Please refer to other examples in this folder to better understand the usage of tasks.
It is also advised to read the documentation on FreeRTOS web pages:
https://www.freertos.org/a00106.html

This example creates 2 tasks with the same implementation - they write into a shared variable and then read it and check if it is the same as what they have written.
In single-thread programming like on Arduino this is of no concern and will be always ok, however when multi-threading is used the execution of the task is switched by the FreeRTOS and the value can be rewritten from another task before reading again.
The tasks print write and read operation - each in their column for better reading. Task 0 is on the left and Task 1 is on the right.
Watch the writes and read in secure mode when using the mutex (default) as the results are as you would expect them.
Then try to comment the USE_MUTEX and watch again - there will be a lot of mismatches!

### Theory:
Mutex is a specialized version of Semaphore (please see the Semaphore example for more info).
In essence, the mutex is a variable whose value determines if the mute is taken (locked) or given (unlocked).
When two or more processes access the same resource (variable, peripheral, etc) it might happen, for example, that when one task starts to read a variable and the operating system (FreeRTOS) will schedule the execution of another task
which will write to this variable and when the previous task runs again it will read something different.

Mutexes and binary semaphores are very similar but have some subtle differences:
Mutexes include a priority inheritance mechanism, whereas binary semaphores do not.
This makes binary semaphores the better choice for implementing synchronization (between tasks or between tasks and an interrupt), and mutexes the better
choice for implementing simple mutual exclusion.
What is priority inheritance?
If a low-priority task holds the Mutex but gets interrupted by a Higher priority task, which
then tries to take the Mutex, the low-priority task will temporarily ‘inherit’ the high priority so a middle-priority task can't block the low-priority task, and thus also block the high priority task.
Semaphores don't have the logic to handle this, in part because Semaphores aren't 'owned' by the task that takes them.

A mutex can also be recursive - if a task that holds the mutex takes it again, it will succeed, and the mutex will be released
for other tasks only when it is given the same number of times that it was taken.

You can check the danger by commenting on the definition of USE_MUTEX which will disable the mutex and present the danger of concurrent access.


# Supported Targets

This example supports all ESP32 SoCs.

## How to Use Example

Flash and observe the serial output.

Comment the `USE_MUTEX` definition, save and flash again and observe the behavior of unprotected access to the shared variable.

* How to install the Arduino IDE: [Install Arduino IDE](https://github.com/espressif/arduino-esp32/tree/master/docs/arduino-ide).

#### Using Arduino IDE

To get more information about the Espressif boards see [Espressif Development Kits](https://www.espressif.com/en/products/devkits).

* Before Compile/Verify, select the correct board: `Tools -> Board`.
* Select the COM port: `Tools -> Port: xxx` where the `xxx` is the detected COM port.

#### Using Platform IO

* Select the COM port: `Devices` or set the `upload_port` option on the `platformio.ini` file.

## Example Log Output

The expected output of shared variables protected by mutex demonstrates mutually exclusive access from tasks - they do not interrupt each other and do not rewrite the value before the other task has read it back.

```
 Task 0          | Task 1
                 | Starting
                 | 0 <- 227
 Starting        |
                 | R: 227
 227 <- 737      |
 R: 737          |
                 | 737 <- 282
                 | R: 282
 282 <- 267      |
```

The output of unprotected access to shared variable - it happens often that a task is interrupted after writing and before reading the other task write a different value - a corruption occurred!

```
 Task 0          | Task 1
                 | Starting
                 | 0 <- 333
 Starting        |
 333 <- 620      |
 R: 620          |
 620 <- 244      |
                 | R: 244
                 | Mismatch!
                 | 244 <- 131
 R: 131          |
 Mismatch!       |
 131 <- 584      |
                 | R: 584
                 | Mismatch!
                 | 584 <- 134
                 | R: 134
                 | 134 <- 554
 R: 554          |
 Mismatch!       |
 554 <- 313      |
```

## Troubleshooting

***Important: Make sure you are using a good quality USB cable and that you have a reliable power source***

## Contribute

To know how to contribute to this project, see [How to contribute.](https://github.com/espressif/arduino-esp32/blob/master/CONTRIBUTING.rst)

If you have any **feedback** or **issue** to report on this example/library, please open an issue or fix it by creating a new PR. Contributions are more than welcome!

Before creating a new issue, be sure to try Troubleshooting and check if the same issue was already created by someone else.

## Resources

* Official ESP32 Forum: [Link](https://esp32.com)
* Arduino-ESP32 Official Repository: [espressif/arduino-esp32](https://github.com/espressif/arduino-esp32)
* ESP32 Datasheet: [Link to datasheet](https://www.espressif.com/sites/default/files/documentation/esp32_datasheet_en.pdf)
* ESP32-S2 Datasheet: [Link to datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s2_datasheet_en.pdf)
* ESP32-C3 Datasheet: [Link to datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-c3_datasheet_en.pdf)
* ESP32-S3 Datasheet: [Link to datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf)
* Official ESP-IDF documentation: [ESP-IDF](https://idf.espressif.com)
