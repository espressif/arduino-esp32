/*
  This example demonstrates basic usage of FreeRTOS Mutually Exclusive Locks (Mutex) for securing access to shared resources in multi threading.
  Please refer to other examples in this folder to better understand usage of tasks.
  It is also advised to read documentation on FreeRTOS web pages:
  https://www.freertos.org/a00106.html

  This example creates 2 task with same implementation - they write into shared
  variable and then read it and check if it is the same as what they have written.
  In single thread programming like on Arduino this is of no concern and will be always ok, however when
  multi threading is used the tasks execution is switched by the FreeRTOS and the value can be rewritten from other task before reading again.
  The tasks print write and read operation - each in their own column for better reading. Task 0 is on the left and Task 1 is on the right.
  Watch the writes and read in secure mode when using the mutex (default) as the results are as you would expect them.
  Then try to comment the USE_MUTEX and watch again - there will be a lots of mismatches!

  Theory:
  Mutex is a specialized version of Semaphore (please see the Semaphore example for more info).
  In essence the mutex is a variable whose value determines if the mutes is taken (locked) or given (unlocked).
  When two or more processes access the same resource (variable, peripheral, etc) it might happen, for example
  that when one task starts to read a variable and the operating system (FreeRTOS) will schedule execution of another task
  which will write to this variable and when the previous task runs again it will read something different.

  Mutexes and binary semaphores are very similar but have some subtle differences:
  Mutexes include a priority inheritance mechanism, binary semaphores do not.
  This makes binary semaphores the better choice for implementing
  synchronisation (between tasks or between tasks and an interrupt), and mutexes the better
  choice for implementing simple mutual exclusion.

  You can check the danger by commenting the definition of USE_MUTEX which will disable the mutex and present the danger of concurrent access.
*/

#define USE_MUTEX
int shared_variable = 0;
SemaphoreHandle_t shared_var_mutex = NULL;

// Define a task function
void Task( void *pvParameters );

// The setup function runs once when you press reset or power on the board.
void setup() {
  // Initialize serial communication at 115200 bits per second:
  Serial.begin(115200);
  while(!Serial) delay(100);
  Serial.printf(" Task 0          | Task 1\n");

#ifdef USE_MUTEX
  shared_var_mutex = xSemaphoreCreateMutex(); // Create the mutex
#endif


  // Set up two tasks to run the same function independently.
  static int task_number0 = 0;
  xTaskCreate(
    Task
    ,  "Task 0" // A name just for humans
    ,  2048          // The stack size
    ,  (void*)&task_number0 // Pass reference to a variable describing the task number
    //,  5  // High priority
    ,  1  // priority
    ,  NULL // Task handle is not used here - simply pass NULL
    );

  static int task_number1 = 1;
  xTaskCreate(
    Task
    ,  "Task 1"
    ,  2048  // Stack size
    ,  (void*)&task_number1 // Pass reference to a variable describing the task number
    ,  1  // Low priority
    ,  NULL // Task handle is not used here - simply pass NULL
    );

  // Now the task scheduler, which takes over control of scheduling individual tasks, is automatically started.
}

void loop(){
}

/*--------------------------------------------------*/
/*---------------------- Tasks ---------------------*/
/*--------------------------------------------------*/

void Task(void *pvParameters){  // This is a task.
  int task_num = *((int*)pvParameters);
  Serial.printf("%s\n", task_num ? " Starting        |" : "                 | Starting");
  for (;;){ // A Task shall never return or exit.
#ifdef USE_MUTEX
    if(shared_var_mutex != NULL){ // Sanity check if the mutex exists
        // Try to take the mutex and wait indefintly if needed
        if(xSemaphoreTake(shared_var_mutex, portMAX_DELAY) == pdTRUE){
          // Mutex successfully taken
#endif
          int new_value = random(1000);

          char str0[32]; sprintf(str0, " %d <- %d      |", shared_variable, new_value);
          char str1[32]; sprintf(str1, "                 | %d <- %d", shared_variable, new_value);
          Serial.printf("%s\n", task_num ? str0 : str1);

          shared_variable = new_value;
          delay(random(100)); // wait random time of max 100 ms - simulating some computation

          sprintf(str0, " R: %d          |", shared_variable);
          sprintf(str1, "                 | R: %d", shared_variable);
          Serial.printf("%s\n", task_num ? str0 : str1);
          //Serial.printf("Task %d after write: reading %d\n", task_num, shared_variable);

          if(shared_variable != new_value){
            Serial.printf("%s\n", task_num ? " Mismatch!         |" : "                 | Mismatch!");
            //Serial.printf("Task %d: detected race condition - the value changed!\n", task_num);
          }

#ifdef USE_MUTEX
            xSemaphoreGive(shared_var_mutex); // After accessing the shared resource give the mutex and allow other processes to access it
        }else{
          // We could not obtain the semaphore and can therefore not access the shared resource safely.
        } // mutex take
    } // sanity check
#endif
    delay(10); // Allow other task to be scheduled
  } // Infinite loop
}