#include "Speaker.h"
#include "music_8bit.h"
SPEAKER Speaker;

void setup() {
  // put your setup code here, to run once:
   Speaker.begin();
   Speaker.setVolume(10);
   Speaker.playMusic(data, 22500);  
}

void loop() {
  // put your main code here, to run repeatedly:
   Speaker.update();
}
