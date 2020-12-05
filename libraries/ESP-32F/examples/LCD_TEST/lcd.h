#ifndef __LCD_H
#define __LCD_H
#include "Arduino.h"
#include "stdlib.h"
#include "string.h"
#define u8 unsigned char
#define u16  unsigned short
#define u32 unsigned int
 

class LCD
{
  public:
    u8  pin;
  public:   
    void LCD_Init(void);
    void LCD_WR_REG(u16 data);
    u16 LCD_RD_DATA(void); 
    void LCD_WriteReg(u16 LCD_Reg, u16 LCD_RegValue);
    void LCD_WR_DATA(u8 data);
    void LCD_WR_DATA_16Bit(u16 data);
    u16 LCD_ReadReg(u8 LCD_Reg);
    void LCD_WriteRAM_Prepare(void);
    void LCD_WriteRAM(u16 RGB_Code);
    u16 LCD_ReadRAM(void);
    void LCD_DisplayOn(void);
    void LCD_DisplayOff(void);
    void LCD_Clear(u16 Color);
    void LCD_SetCursor(u16 Xpos, u16 Ypos);
    void LCD_DrawPoint(u16 x, u16 y); 
    u16  LCD_ReadPoint(u16 x, u16 y);  
    void LCD_SetWindows(u16 xStar, u16 yStar, u16 xEnd, u16 yEnd);
    void LCD_DrawPoint_16Bit(u16 color);
    u16 LCD_BGR2RGB(u16 c);
    void LCD_SetParam(void);
    void LCD_GPIOInit(void);
    void LCD_RESET(void);
    void GUI_DrawPoint(u16 x,u16 y,u16 color);
    void LCD_Fill(u16 sx,u16 sy,u16 ex,u16 ey,u16 color);
    void LCD_DrawLine(u16 x1, u16 y1, u16 x2, u16 y2);
    void LCD_DrawRectangle(u16 x1, u16 y1, u16 x2, u16 y2);
    void Draw_Circle(u16 x0,u16 y0,u16 fc,u8 r);
    void LCD_ShowChar(u16 x,u16 y,u16 fc, u16 bc, u8 num,u8 lenth,u8 mod);
    void LCD_ShowNum(u16 x,u16 y,u32 num,u8 len,u8 lenth);
    void LCD_Show2Num(u16 x,u16 y,u16 num,u8 len,u8 lenth,u8 mod);
    void LCD_ShowString(u16 x,u16 y,u8 lenth,const char *p,u8 mod);
    void GUI_DrawFont16(u16 x, u16 y, u16 fc, u16 bc,const char *s,u8 mod);
    void GUI_DrawFont24(u16 x, u16 y, u16 fc, u16 bc,const char *s,u8 mod);
    void GUI_DrawFont32(u16 x, u16 y, u16 fc, u16 bc,const char *s,u8 mod);
    void Show_Str(u16 x, u16 y, u16 fc, u16 bc,const char *str,u8 lenth,u8 mod);
    void Gui_Drawbmp16(u16 lenth,u16 width,u16 x,u16 y,const unsigned char *p); //锟斤拷示40*40 QQ图片
    void _draw_circle_8(int xc, int yc, int x, int y, u16 c);
    void gui_circle(int xc, int yc,u16 c,int r, int fill);
    void Gui_StrCenter(u16 x, u16 y, u16 fc, u16 bc,const char *str,u8 lenth,u8 mod);
    void LCD_DrawFillRectangle(u16 x1, u16 y1, u16 x2, u16 y2);
};
//LCD锟斤拷要锟斤拷锟斤拷锟斤拷
typedef struct
{
  u16 width;			 
  u16 height;			 
  u16 id;				 
  u8  dir;			 
  u16	 wramcmd;		 
  u16  setxcmd;		 
  u16  setycmd;		 
} _lcd_dev;

 
extern _lcd_dev lcddev;	 
//定义LCD的尺寸
#if USE_HORIZONTAL==1	//定义是否使用横屏     0,不使用.1,使用.
#define LCD_W 320
#define LCD_H 240
#else
#define LCD_W 240
#define LCD_H 320
#endif


extern u16  POINT_COLOR;//默认红色    

extern u16  BACK_COLOR; //背景颜色.默认为白色

 
#define LCD_LED        	13   
 
#define LCD_RS         	17//RS
#define LCD_CS        	26//CS
#define LCD_RST     	  16//复位
#define LCD_SCL        	18//CLK
#define LCD_SDA        	23//SDI 
#define LCD_LED         13//背光
//置0置1操作语句宏定义 
#define	LCD_CS_SET   digitalWrite(LCD_CS,1)
#define	LCD_RS_SET  	digitalWrite(LCD_RS,1)
#define	LCD_SDA_SET  digitalWrite(LCD_SDA,1)
#define	LCD_SCL_SET  digitalWrite(LCD_SCL,1)
#define	LCD_RST_SET  digitalWrite(LCD_RST,1)
#define	LCD_LED_SET  	digitalWrite(LCD_LED,1)
 
#define	LCD_CS_CLR  digitalWrite(LCD_CS,0)
#define	LCD_RS_CLR  	digitalWrite(LCD_RS,0)
#define	LCD_SDA_CLR  digitalWrite(LCD_SDA,0)
#define	LCD_SCL_CLR  digitalWrite(LCD_SCL,0)
#define	LCD_RST_CLR  digitalWrite(LCD_RST,0)
#define	LCD_LED_CLR  digitalWrite(LCD_LED,0)

//画笔颜色
#define WHITE       0xFFFF
#define BLACK      	0x0000
#define BLUE       	0x001F
#define BRED        0XF81F
#define GRED 			 	0XFFE0
#define GBLUE			 	0X07FF
#define RED         0xF800
#define MAGENTA     0xF81F
#define GREEN       0x07E0
#define CYAN        0x7FFF
#define YELLOW      0xFFE0
#define BROWN 			0XBC40  
#define BRRED 			0XFC07  
#define GRAY  			0X8430  
//GUI锟斤拷色

#define DARKBLUE      	 0X01CF	 
#define LIGHTBLUE      	 0X7D7C 
#define GRAYBLUE       	 0X5458  
 

#define LIGHTGREEN     	0X841F  
//#define LIGHTGRAY     0XEF5B  
#define LGRAY 			 		0XC618  

#define LGRAYBLUE      	0XA651  
#define LBBLUE          0X2B12  

extern u16 BACK_COLOR, POINT_COLOR ;

void LCD_Init(void);
void LCD_WR_REG(u16 data);
u16 LCD_RD_DATA(void); 
void LCD_WriteReg(u16 LCD_Reg, u16 LCD_RegValue);
void LCD_WR_DATA(u8 data);
void LCD_WR_DATA_16Bit(u16 data);
u16 LCD_ReadReg(u8 LCD_Reg);
void LCD_WriteRAM_Prepare(void);
void LCD_WriteRAM(u16 RGB_Code);
u16 LCD_ReadRAM(void);
void LCD_DisplayOn(void);
void LCD_DisplayOff(void);
void LCD_Clear(u16 Color);
void LCD_SetCursor(u16 Xpos, u16 Ypos);
void LCD_DrawPoint(u16 x, u16 y); 
u16  LCD_ReadPoint(u16 x, u16 y);  

void LCD_SetWindows(u16 xStar, u16 yStar, u16 xEnd, u16 yEnd);
void LCD_DrawPoint_16Bit(u16 color);
u16 LCD_BGR2RGB(u16 c);
void LCD_SetParam(void); 
void LCD_GPIOInit(void);
void LCD_RESET(void);
void GUI_DrawPoint(u16 x,u16 y,u16 color);
void LCD_Fill(u16 sx,u16 sy,u16 ex,u16 ey,u16 color);
void LCD_DrawLine(u16 x1, u16 y1, u16 x2, u16 y2);
void LCD_DrawRectangle(u16 x1, u16 y1, u16 x2, u16 y2);
void Draw_Circle(u16 x0,u16 y0,u16 fc,u8 r);
void LCD_ShowChar(u16 x,u16 y,u16 fc, u16 bc, u8 num,u8 lenth,u8 mod);
void LCD_ShowNum(u16 x,u16 y,u32 num,u8 len,u8 lenth);
void LCD_Show2Num(u16 x,u16 y,u16 num,u8 len,u8 lenth,u8 mod);
void LCD_ShowString(u16 x,u16 y,u8 lenth,const char *p,u8 mod);
void GUI_DrawFont16(u16 x, u16 y, u16 fc, u16 bc,const char *s,u8 mod);
void GUI_DrawFont24(u16 x, u16 y, u16 fc, u16 bc,const char *s,u8 mod);
void GUI_DrawFont32(u16 x, u16 y, u16 fc, u16 bc,const char *s,u8 mod);
void Show_Str(u16 x, u16 y, u16 fc, u16 bc,const char *str,u8 lenth,u8 mod);
void Gui_Drawbmp16(u16 lenth,u16 width,u16 x,u16 y,const unsigned char *p); //锟斤拷示40*40 QQ图片
void _draw_circle_8(int xc, int yc, int x, int y, u16 c);
void gui_circle(int xc, int yc,u16 c,int r, int fill);
void Gui_StrCenter(u16 x, u16 y, u16 fc, u16 bc, u8 *str,u8 lenth,u8 mod);
void LCD_DrawFillRectangle(u16 x1, u16 y1, u16 x2, u16 y2);
#endif






