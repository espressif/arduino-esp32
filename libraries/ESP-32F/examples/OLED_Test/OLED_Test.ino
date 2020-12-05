#include "SSD1306.h"
#include "images.h"
#include "OLEDDisplayUi.h"
SSD1306 oled(0x3c,15,14);
OLEDDisplayUi ui     ( &oled );
void drawCircle(void) {
  for (int16_t i=0; i<DISPLAY_HEIGHT; i+=2) {
    oled.drawCircle(DISPLAY_WIDTH/2, DISPLAY_HEIGHT/2, i);
    oled.display();
    delay(10);
  }
  delay(1000);
  oled.clear();

  // This will draw the part of the circel in quadrant 1
  // Quadrants are numberd like this:
  //   0010 | 0001
  //  ------|-----
  //   0100 | 1000
  //
  oled.drawCircleQuads(DISPLAY_WIDTH/2, DISPLAY_HEIGHT/2, DISPLAY_HEIGHT/4, 0b00000001);
  oled.display();
  delay(200);
  oled.drawCircleQuads(DISPLAY_WIDTH/2, DISPLAY_HEIGHT/2, DISPLAY_HEIGHT/4, 0b00000011);
  oled.display();
  delay(200);
  oled.drawCircleQuads(DISPLAY_WIDTH/2, DISPLAY_HEIGHT/2, DISPLAY_HEIGHT/4, 0b00000111);
  oled.display();
  delay(200);
  oled.drawCircleQuads(DISPLAY_WIDTH/2, DISPLAY_HEIGHT/2, DISPLAY_HEIGHT/4, 0b00001111);
  oled.display();
}
void setup() {
  // put your setup code here, to run once:
   oled.init();
 
   oled.flipScreenVertically();
   //oled.setContrast(255);
   //drawCircle();
   oled.drawXbm(0,0, Logo_width,Logo_height, Logo_bits); oled.display();
}

void loop() {
  // put your main code here, to run repeatedly:

}
