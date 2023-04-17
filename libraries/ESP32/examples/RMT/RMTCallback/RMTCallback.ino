#include "Arduino.h"
#include "esp32-hal.h"

extern "C" void receive_trampoline(uint32_t *data, size_t len, void * arg);

class MyProcessor {
  private:
    int8_t gpio = -1;
    uint32_t buff; // rolling buffer of most recent 32 bits.
    int at = 0;
    EventGroupHandle_t events = NULL;
    size_t rx_num_symbols = RMT_MEM_NUM_BLOCKS_1 * RMT_SYMBOLS_PER_CHANNEL_BLOCK;
    rmt_data_t rx_symbols[RMT_MEM_NUM_BLOCKS_1 * RMT_SYMBOLS_PER_CHANNEL_BLOCK];

  public:
    MyProcessor(uint8_t pin, uint32_t rmtFreqHz) {
      if (!rmtInit(pin, RMT_RX_MODE, RMT_MEM_NUM_BLOCKS_1, rmtFreqHz))
      {
        Serial.println("init receiver failed\n");
        return;
      }
      gpio = pin;
      events = xEventGroupCreate();
    }

    void begin() {
      // Creating RMT RX Callback Task
      xTaskCreate(readTask, "MyProcessor", 2048, this, 4, NULL);
    }

    static void readTask(void *args) {
      MyProcessor *me = (MyProcessor *) args;

      while(1) {
        // blocks until RMT has read data
        rmtReadAsync(me->gpio, me->rx_symbols, &me->rx_num_symbols, me->events, true, portMAX_DELAY);
        // process the data like a callback whenever there is data available
        process(me->rx_symbols, me->rx_num_symbols, me);
      }
  
      vTaskDelete(NULL);
    }

    static void process(rmt_data_t *data, size_t len, void *args) {
      MyProcessor *me = (MyProcessor *) args;
      uint32_t *buff = &me->buff;
      
      for (int i = 0; len; len--) {
        if (data[i].duration0 == 0)
          break;
        *buff = (*buff << 1) | (data[i].level0 ? 1 : 0);
        i++;

        if (data[i].duration1 == 0)
          break;
        *buff = (*buff << 1) | (data[i].level1 ? 1 : 0);
        i++;
      };
    }

    uint32_t val() {
      return buff;
    }
};

// Attach 3 processors to GPIO 4, 5 and 10 with different tick/speeds.
MyProcessor mp1 = MyProcessor(4, 1000000);
MyProcessor mp2 = MyProcessor(5, 1000000);
MyProcessor mp3 = MyProcessor(10, 2000000);

void setup()
{
  Serial.begin(115200);
  mp1.begin();
  mp2.begin();
  mp3.begin();
}

void loop()
{
  Serial.printf("GPIO 4: %08lx 5: %08lx 10: %08lx\n", mp1.val(), mp2.val(), mp3.val());
  delay(500);
}
