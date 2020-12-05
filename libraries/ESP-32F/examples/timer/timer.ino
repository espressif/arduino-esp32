/*------------------------------------------------------------------------------------------------------------------------------
                              果云科技
                           ESP-32F 开发板

                             定时器 实验
------------------------------------------------------------------------------------------------------------------------------*/
hw_timer_t *timer=NULL;//创建一个定时器结构体
volatile SemaphoreHandle_t timerSemaphore;//创建一个定时器信号量
int time_count=0;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR Timer_Hander()
{
  portENTER_CRITICAL_ISR(&timerMux);//进入临界段
  time_count++;
  portEXIT_CRITICAL_ISR(&timerMux);//退出临界段
  xSemaphoreGiveFromISR(timerSemaphore, NULL);//释放一个二值信号量timerSemaphore
  
}
void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  timerSemaphore=xSemaphoreCreateBinary();//定义信号量
  timer = timerBegin(0, 80, true);//初始化定时器0 80分频  向上计数

  // 配置定时器中断函数
  timerAttachInterrupt(timer, &Timer_Hander, true);

  // Set alarm to call onTimer function every second (value in microseconds).
  // Repeat the alarm (third parameter)
  timerAlarmWrite(timer, 5000000, true);//每计数5000000次触发定时器中断 自动重载开启

  // Start an alarm
  timerAlarmEnable(timer);//使能定时器函数
}

void loop() {
  // put your main code here, to run repeatedly:收到定时器触发后置位的二值信号量后打印计数值
  if (xSemaphoreTake(timerSemaphore,0) == pdTRUE){
    portENTER_CRITICAL_ISR(&timerMux);
    Serial.println(millis());//打印系统已经运行了多少时间
    Serial.println(time_count);//计数值
    portEXIT_CRITICAL_ISR(&timerMux);
  }
  
}
