#include "Arduino.h"
#include "esp32-hal.h"

extern "C" void receive_trampoline(uint32_t *data, size_t len, void * arg);

class MyProcessor {
  private:
    rmt_obj_t* rmt_recv = NULL;
    float realNanoTick;
    uint32_t buff; // rolling buffer of most recent 32 bits.
    int at = 0;

  public:
    MyProcessor(uint8_t pin, float nanoTicks) {
      assert((rmt_recv = rmtInit(21, false, RMT_MEM_192)));

      realNanoTick = rmtSetTick(rmt_recv, nanoTicks);
    };
    void begin() {
      rmtRead(rmt_recv, receive_trampoline, this);
    };

    void process(rmt_data_t *data, size_t len) {
      for (int i = 0; len; len--) {
        if (data[i].duration0 == 0)
          break;
        buff = (buff << 1) | (data[i].level0 ? 1 : 0);
        i++;

        if (data[i].duration1 == 0)
          break;
        buff = (buff << 1) | (data[i].level1 ? 1 : 0);
        i++;
      };
    };
    uint32_t val() {
      return buff;
    }
};

void receive_trampoline(uint32_t *data, size_t len, void * arg)
{
  MyProcessor * p = (MyProcessor *)arg;
  p->process((rmt_data_t*) data, len);
}

// Attach 3 processors to GPIO 4, 5 and 10 with different tick/speeds.
MyProcessor mp1 = MyProcessor(4, 1000);
MyProcessor mp2 = MyProcessor(5, 1000);
MyProcessor mp3 = MyProcessor(10, 500);

void setup()
{
  Serial.begin(115200);
  mp1.begin();
  mp2.begin();
  mp3.begin();
}

void loop()
{
  Serial.printf("GPIO 4: %08x 5: %08x 6: %08x\n", mp1.val(), mp2.val(), mp3.val());
  delay(500);
}
