#include"lcd.h"
#include"icon.h"
LCD lcd;
 
const char* test[] = {
      "Hello",
      "World" ,
      "----",
      "Show off",
      "how",
      "the log buffer",
      "is",
      "working.",
      "Even",
      "scrolling is",
      "working"
  };
void setup() {
  // put your setup code here, to run once:
     lcd.LCD_GPIOInit();
     lcd.LCD_Init();
     Serial.begin(115200);
     Serial.println("Hello Goouuu");
     lcd.LCD_Clear(YELLOW);
     
     //POINT_COLOR=BLUE;
     //lcd.LCD_DrawLine(0,127,127,0); //»­¾ØÐÎ 
     //lcd.LCD_DrawLine(0,0,127,127); //»­¾ØÐÎ 
     //lcd.LCD_ShowString(32,5,8,test[0],1);
     //lcd.gui_circle(50,50,YELLOW,40,1);
    //  lcd.LCD_ShowString(50,50,12,"yangqia",1);
   //  lcd.Show_Str(32,5,BLUE,YELLOW,"系 ",12,0);
    lcd.Gui_Drawbmp16(128,128,0,0,gImage_123);
}

void loop() {
  // put your main code here, to run repeatedly:

}

