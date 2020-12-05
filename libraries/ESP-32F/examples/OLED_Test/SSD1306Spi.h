/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 by Daniel Eichhorn
 * Copyright (c) 2016 by Fabrice Weinberg
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Credits for parts of this code go to Mike Rankin. Thank you so much for sharing!
 */

#ifndef SSD1306Spi_h
#define SSD1306Spi_h

#include "OLEDDisplay.h"
#include <SPI.h>

#if F_CPU == 160000000L
  #define BRZO_I2C_SPEED 1000
#else
  #define BRZO_I2C_SPEED 800
#endif

class SSD1306Spi : public OLEDDisplay {
  private:
      uint8_t             _rst;
      uint8_t             _dc;
      uint8_t             _cs;

  public:
    SSD1306Spi(uint8_t _rst, uint8_t _dc, uint8_t _cs) {
      this->_rst = _rst;
      this->_dc  = _dc;
      this->_cs  = _cs;
    }

    bool connect(){
      pinMode(_dc, OUTPUT);
      pinMode(_cs, OUTPUT);
      pinMode(_rst, OUTPUT);

      SPI.begin ();
      SPI.setClockDivider (SPI_CLOCK_DIV2);

      // Pulse Reset low for 10ms
      digitalWrite(_rst, HIGH);
      delay(1);
      digitalWrite(_rst, LOW);
      delay(10);
      digitalWrite(_rst, HIGH);
      return true;
    }

    void display(void) {
    #ifdef OLEDDISPLAY_DOUBLE_BUFFER
       uint8_t minBoundY = ~0;
       uint8_t maxBoundY = 0;

       uint8_t minBoundX = ~0;
       uint8_t maxBoundX = 0;

       uint8_t x, y;

       // Calculate the Y bounding box of changes
       // and copy buffer[pos] to buffer_back[pos];
       for (y = 0; y < (DISPLAY_HEIGHT / 8); y++) {
         for (x = 0; x < DISPLAY_WIDTH; x++) {
          uint16_t pos = x + y * DISPLAY_WIDTH;
          if (buffer[pos] != buffer_back[pos]) {
            minBoundY = _min(minBoundY, y);
            maxBoundY = _max(maxBoundY, y);
            minBoundX = _min(minBoundX, x);
            maxBoundX = _max(maxBoundX, x);
          }
          buffer_back[pos] = buffer[pos];
        }
        yield();
       }

       // If the minBoundY wasn't updated
       // we can savely assume that buffer_back[pos] == buffer[pos]
       // holdes true for all values of pos
       if (minBoundY == ~0) return;

       sendCommand(COLUMNADDR);
       sendCommand(minBoundX);
       sendCommand(maxBoundX);

       sendCommand(PAGEADDR);
       sendCommand(minBoundY);
       sendCommand(maxBoundY);

       digitalWrite(_cs, HIGH);
       digitalWrite(_dc, HIGH);   // data mode
       digitalWrite(_cs, LOW);
       for (y = minBoundY; y <= maxBoundY; y++) {
         for (x = minBoundX; x <= maxBoundX; x++) {
           SPI.transfer(buffer[x + y * DISPLAY_WIDTH]);
         }
         yield();
       }
       digitalWrite(_cs, HIGH);
     #else
       // No double buffering
       sendCommand(COLUMNADDR);
       sendCommand(0x0);
       sendCommand(0x7F);

       sendCommand(PAGEADDR);
       sendCommand(0x0);
       sendCommand(0x7);

        digitalWrite(_cs, HIGH);
        digitalWrite(_dc, HIGH);   // data mode
        digitalWrite(_cs, LOW);
        for (uint16_t i=0; i<DISPLAY_BUFFER_SIZE; i++) {
          SPI.transfer(buffer[i]);
          yield();
        }
        digitalWrite(_cs, HIGH);
     #endif
    }

  private:
    inline void sendCommand(uint8_t com) __attribute__((always_inline)){
      digitalWrite(_cs, HIGH);
      digitalWrite(_dc, LOW);
      digitalWrite(_cs, LOW);
      SPI.transfer(com);
      digitalWrite(_cs, HIGH);
    }
};

#endif
