#include "lcd.h"
#include "stdlib.h"
#include  "SPI.h"
#include "FONT.h"


//管理LCD重要参数
//默认为竖屏
_lcd_dev lcddev;

//画笔颜色,背景颜色
u16 POINT_COLOR = 0x0000, BACK_COLOR = 0xFFFF;
u16 DeviceCode;
//SPIClass SPIClass(1);


//******************************************************************
//函数名：  LCD_WR_REG
//功能：    向液晶屏总线写入写16位指令
//输入参数：Reg:待写入的指令值
//返回值：  无
//修改记录：无
//******************************************************************


void LCD:: LCD_WR_REG(u16 data)
{
  LCD_CS_CLR;
  LCD_RS_CLR;

  // SPIv_WriteData(data);
  SPI.write(data);
  LCD_CS_SET;
}

//******************************************************************
//函数名：  LCD_WR_DATA
//功能：    向液晶屏总线写入写8位数据
//输入参数：Data:待写入的数据
//返回值：  无
//修改记录：无
//******************************************************************

void LCD:: LCD_WR_DATA(u8 data)
{

  LCD_CS_CLR;
  LCD_RS_SET;

  // SPIv_WriteData(data);
  SPI.write(data);
  LCD_CS_SET;

}
//******************************************************************
//函数名：  LCD_DrawPoint_16Bit
//功能：    8位总线下如何写入一个16位数据
//输入参数：(x,y):光标坐标
//返回值：  无
//修改记录：无
//******************************************************************

void LCD:: LCD_WR_DATA_16Bit(u16 data)
{
  LCD_CS_CLR;
  LCD_RS_SET;

  //SPIv_WriteData(data>>8);
  // SPIv_WriteData(data);
  SPI.write(data >> 8);
  SPI.write(data);
  LCD_CS_SET;
}

//******************************************************************
//函数名：  LCD_WriteReg
//功能：    写寄存器数据
//输入参数：LCD_Reg:寄存器地址
//      LCD_RegValue:要写入的数据

//******************************************************************

void LCD:: LCD_WriteReg(u16 LCD_Reg, u16 LCD_RegValue)
{
  LCD_WR_REG(LCD_Reg);
  LCD_WR_DATA(LCD_RegValue);
}

//******************************************************************
//函数名：  LCD_WriteRAM_Prepare
//功能：    开始写GRAM
//      在给液晶屏传送RGB数据前，应该发送写GRAM指令

//******************************************************************

void LCD:: LCD_WriteRAM_Prepare(void)
{
  LCD_WR_REG(lcddev.wramcmd);
}
/*************************************************
  鍑芥暟鍚嶏細LCD_SetCursor
  鍔熻兘锛氳缃厜鏍囦綅缃�
  鍏ュ彛鍙傛暟锛歺y鍧愭爣
  杩斿洖鍊硷細鏃�
*************************************************/
void LCD:: LCD_SetCursor(u16 Xpos, u16 Ypos)
{
  LCD_SetWindows(Xpos, Ypos, Xpos, Ypos);
}

//璁剧疆LCD鍙傛暟
//鏂逛究杩涜妯珫灞忔ā寮忓垏鎹�
void LCD:: LCD_SetParam(void)
{
  lcddev.wramcmd = 0x2C;
#if USE_HORIZONTAL==1 //浣跨敤妯睆    
  lcddev.dir = 1; //妯睆
  lcddev.width = 128;
  lcddev.height = 128;
  lcddev.setxcmd = 0x2A;
  lcddev.setycmd = 0x2B;
  LCD_WriteReg(0x36, 0xA8);

#else//绔栧睆
  lcddev.dir = 0; //绔栧睆
  lcddev.width = 128;
  lcddev.height = 128;
  lcddev.setxcmd = 0x2A;
  lcddev.setycmd = 0x2B;
  LCD_WriteReg(0x36, 0xc8);
#endif
}

//******************************************************************
//函数名：  LCD_DrawPoint
//功能：    在指定位置写入一个像素点数据
//输入参数：(x,y):光标坐标
//返回值：  无
//修改记录：无
//******************************************************************

void LCD:: LCD_DrawPoint(u16 x, u16 y)
{
  LCD_SetCursor(x, y); //璁剧疆鍏夋爣浣嶇疆
  LCD_WR_DATA_16Bit(POINT_COLOR);
}

//******************************************************************
//函数名：  LCD_GPIOInit
//功能：    液晶屏IO初始化，液晶初始化前要调用此函数
//输入参数：无
//返回值：  无
//修改记录：无
//******************************************************************

void LCD:: LCD_GPIOInit(void)
{
  pinMode(LCD_RS, OUTPUT);
  pinMode(LCD_CS, OUTPUT);
  pinMode(LCD_RST, OUTPUT);
  pinMode(LCD_LED, OUTPUT);
  SPI.begin(LCD_SCL, 22, LCD_SDA, LCD_CS);
  //SPI.beginTransaction(SPISettings(500000, MSBFIRST, SPI_MODE2));
}

//******************************************************************
//函数名：  LCD_Reset
//功能：    LCD复位函数，液晶初始化前要调用此函数
//输入参数：无
//返回值：  无
//修改记录：无
//******************************************************************

void LCD::LCD_RESET(void)
{
  LCD_RST_CLR;
  delay(100);
  LCD_RST_SET;
  delay(50);
}

//******************************************************************
//函数名：  LCD_Init
//功能：    LCD初始化
//输入参数：无
//返回值：  无
//修改记录：无
//******************************************************************

void LCD:: LCD_Init(void)
{

  //SPI2_Init();


  LCD_RESET();  
   
  LCD_WR_REG(0x11);//Sleep exit
  delay(120);

  LCD_WR_REG(0x13);//Sleep exit
  //ST7735R Frame Rate
  LCD_WR_REG(0xB1);
  LCD_WR_DATA(0x01);
  LCD_WR_DATA(0x2C);
  LCD_WR_DATA(0x2D);

  LCD_WR_REG(0xB2);
  LCD_WR_DATA(0x01);
  LCD_WR_DATA(0x2C);
  LCD_WR_DATA(0x2D);

  LCD_WR_REG(0xB3);
  LCD_WR_DATA(0x01);
  LCD_WR_DATA(0x2C);
  LCD_WR_DATA(0x2D);
  LCD_WR_DATA(0x01);
  LCD_WR_DATA(0x2C);
  LCD_WR_DATA(0x2D);

  LCD_WR_REG(0xB4); //Column inversion
  LCD_WR_DATA(0x07);

  //ST7735R Power Sequence
  LCD_WR_REG(0xC0);
  LCD_WR_DATA(0xA2);
  LCD_WR_DATA(0x02);
  LCD_WR_DATA(0x84);
  LCD_WR_REG(0xC1);
  LCD_WR_DATA(0xC5);

  LCD_WR_REG(0xC2);
  LCD_WR_DATA(0x0A);
  LCD_WR_DATA(0x00);

  LCD_WR_REG(0xC3);
  LCD_WR_DATA(0x8A);
  LCD_WR_DATA(0x2A);
  LCD_WR_REG(0xC4);
  LCD_WR_DATA(0x8A);
  LCD_WR_DATA(0xEE);

  LCD_WR_REG(0xC5); //VCOM
  LCD_WR_DATA(0x0E);

  LCD_WR_REG(0x36); //MX, MY, RGB mode
  LCD_WR_DATA(0xC8);

  //ST7735R Gamma Sequence
  LCD_WR_REG(0xe0);
  LCD_WR_DATA(0x0f);
  LCD_WR_DATA(0x1a);
  LCD_WR_DATA(0x0f);
  LCD_WR_DATA(0x18);
  LCD_WR_DATA(0x2f);
  LCD_WR_DATA(0x28);
  LCD_WR_DATA(0x20);
  LCD_WR_DATA(0x22);
  LCD_WR_DATA(0x1f);
  LCD_WR_DATA(0x1b);
  LCD_WR_DATA(0x23);
  LCD_WR_DATA(0x37);
  LCD_WR_DATA(0x00);
  LCD_WR_DATA(0x07);
  LCD_WR_DATA(0x02);
  LCD_WR_DATA(0x10);

  LCD_WR_REG(0xe1);
  LCD_WR_DATA(0x0f);
  LCD_WR_DATA(0x1b);
  LCD_WR_DATA(0x0f);
  LCD_WR_DATA(0x17);
  LCD_WR_DATA(0x33);
  LCD_WR_DATA(0x2c);
  LCD_WR_DATA(0x29);
  LCD_WR_DATA(0x2e);
  LCD_WR_DATA(0x30);
  LCD_WR_DATA(0x30);
  LCD_WR_DATA(0x39);
  LCD_WR_DATA(0x3f);
  LCD_WR_DATA(0x00);
  LCD_WR_DATA(0x07);
  LCD_WR_DATA(0x03);
  LCD_WR_DATA(0x10);

  LCD_WR_REG(0x2a);
  LCD_WR_DATA(0x00);
  LCD_WR_DATA(0x00);
  LCD_WR_DATA(0x00);
  LCD_WR_DATA(0x7f);

  LCD_WR_REG(0x2b);
  LCD_WR_DATA(0x00);
  LCD_WR_DATA(0x00);
  LCD_WR_DATA(0x00);
  LCD_WR_DATA(0x9f);

  LCD_WR_REG(0xF0); //Enable test command
  LCD_WR_DATA(0x01);
  LCD_WR_REG(0xF6); //Disable ram power save mode
  LCD_WR_DATA(0x00);

  LCD_WR_REG(0x3A); //65k mode
  LCD_WR_DATA(0x05);
  LCD_WR_REG(0x29);//Display on

  LCD_SetParam();//设置LCD参数  
  LCD_LED_SET;//点亮背光
  //LCD_Clear(WHITE);
}
//******************************************************************
//函数名：  LCD_Clear
//功能：    LCD全屏填充清屏函数
//输入参数：Color:要清屏的填充色
//返回值：  无
//修改记录：无
//******************************************************************

void LCD:: LCD_Clear(u16 Color)
{
  u16 i, j;
  LCD_SetWindows(0, 0, lcddev.width - 1, lcddev.height - 1);
  for (i = 0; i < lcddev.width; i++)
  {
    for (j = 0; j < lcddev.height; j++)
      LCD_WR_DATA_16Bit(Color); //鍐欏叆鏁版嵁
  }
}
/*************************************************
  鍑芥暟鍚嶏細LCD_SetWindows
  鍔熻兘锛氳缃甽cd鏄剧ず绐楀彛锛屽湪姝ゅ尯鍩熷啓鐐规暟鎹嚜鍔ㄦ崲琛�
  鍏ュ彛鍙傛暟锛歺y璧风偣鍜岀粓鐐�
  杩斿洖鍊硷細鏃�
*************************************************/
void LCD:: LCD_SetWindows(u16 xStar, u16 yStar, u16 xEnd, u16 yEnd)
{
#if USE_HORIZONTAL==1 //浣跨敤妯睆
  LCD_WR_REG(lcddev.setxcmd);
  LCD_WR_DATA(xStar >> 8);
  LCD_WR_DATA(0x00FF & xStar + 3);
  LCD_WR_DATA(xEnd >> 8);
  LCD_WR_DATA(0x00FF & xEnd + 3);

  LCD_WR_REG(lcddev.setycmd);
  LCD_WR_DATA(yStar >> 8);
  LCD_WR_DATA(0x00FF & yStar + 2);
  LCD_WR_DATA(yEnd >> 8);
  LCD_WR_DATA(0x00FF & yEnd + 2);

#else

  LCD_WR_REG(lcddev.setxcmd);
  LCD_WR_DATA(xStar >> 8);
  LCD_WR_DATA(0x00FF & xStar + 2);
  LCD_WR_DATA(xEnd >> 8);
  LCD_WR_DATA(0x00FF & xEnd + 2);

  LCD_WR_REG(lcddev.setycmd);
  LCD_WR_DATA(yStar >> 8);
  LCD_WR_DATA(0x00FF & yStar + 3);
  LCD_WR_DATA(yEnd >> 8);
  LCD_WR_DATA(0x00FF & yEnd + 3);
#endif

  LCD_WriteRAM_Prepare(); //寮�濮嬪啓鍏RAM
}

//******************************************************************
//函数名：  GUI_DrawPoint
//作者：    xiao冯@全动电子
//日期：    2013-02-22
//功能：    GUI描绘一个点
//输入参数：x:光标位置x坐标
//          y:光标位置y坐标
//      color:要填充的颜色
//返回值：  无
//修改记录：无
//******************************************************************
void LCD:: GUI_DrawPoint(u16 x,u16 y,u16 color)
{
  LCD_SetCursor(x,y);//设置光标位置 
  LCD_WR_DATA_16Bit(color); 
}

//******************************************************************
//函数名：  LCD_Fill
//作者：    xiao冯@全动电子
//日期：    2013-02-22
//功能：    在指定区域内填充颜色
//输入参数：sx:指定区域开始点x坐标
//          sy:指定区域开始点y坐标
//      ex:指定区域结束点x坐标
//      ey:指定区域结束点y坐标
//          color:要填充的颜色
//返回值：  无
//修改记录：无
//******************************************************************
void LCD:: LCD_Fill(u16 sx,u16 sy,u16 ex,u16 ey,u16 color)
{   

  u16 i,j;      
  u16 width=ex-sx+1;    //得到填充的宽度
  u16 height=ey-sy+1;   //高度
  LCD_SetWindows(sx,sy,ex-1,ey-1);//设置显示窗口
  for(i=0;i<height;i++)
  {
    for(j=0;j<width;j++)
    LCD_WR_DATA_16Bit(color); //写入数据   
  }

  LCD_SetWindows(0,0,lcddev.width-1,lcddev.height-1);//恢复窗口设置为全屏
}

//******************************************************************
//函数名：  LCD_DrawLine
//作者：    xiao冯@全动电子
//日期：    2013-02-22
//功能：    GUI画线
//输入参数：x1,y1:起点坐标
//          x2,y2:终点坐标 
//返回值：  无
//修改记录：无
//****************************************************************** 
void LCD:: LCD_DrawLine(u16 x1, u16 y1, u16 x2, u16 y2)
{
  u16 t; 
  int xerr=0,yerr=0,delta_x,delta_y,distance; 
  int incx,incy,uRow,uCol; 

  delta_x=x2-x1; //计算坐标增量 
  delta_y=y2-y1; 
  uRow=x1; 
  uCol=y1; 
  if(delta_x>0)incx=1; //设置单步方向 
  else if(delta_x==0)incx=0;//垂直线 
  else {incx=-1;delta_x=-delta_x;} 
  if(delta_y>0)incy=1; 
  else if(delta_y==0)incy=0;//水平线 
  else{incy=-1;delta_y=-delta_y;} 
  if( delta_x>delta_y)distance=delta_x; //选取基本增量坐标轴 
  else distance=delta_y; 
  for(t=0;t<=distance+1;t++ )//画线输出 
  {  
    LCD_DrawPoint(uRow,uCol);//画点 
    xerr+=delta_x ; 
    yerr+=delta_y ; 
    if(xerr>distance) 
    { 
      xerr-=distance; 
      uRow+=incx; 
    } 
    if(yerr>distance) 
    { 
      yerr-=distance; 
      uCol+=incy; 
    } 
  }  
} 

//******************************************************************
//函数名：  LCD_DrawRectangle
//作者：    xiao冯@全动电子
//日期：    2013-02-22
//功能：    GUI画矩形(非填充)
//输入参数：(x1,y1),(x2,y2):矩形的对角坐标
//返回值：  无
//修改记录：无
//******************************************************************  
void LCD:: LCD_DrawRectangle(u16 x1, u16 y1, u16 x2, u16 y2)
{
  LCD_DrawLine(x1,y1,x2,y1);
  LCD_DrawLine(x1,y1,x1,y2);
  LCD_DrawLine(x1,y2,x2,y2);
  LCD_DrawLine(x2,y1,x2,y2);
}  
//******************************************************************
//函数名：  LCD_DrawFillRectangle
//作者：    xiao冯@全动电子
//日期：    2013-02-22
//功能：    GUI画矩形(填充)
//输入参数：(x1,y1),(x2,y2):矩形的对角坐标
//返回值：  无
//修改记录：无
//******************************************************************   
void LCD:: LCD_DrawFillRectangle(u16 x1, u16 y1, u16 x2, u16 y2)
{
  LCD_Fill(x1,y1,x2,y2,POINT_COLOR);

}

//******************************************************************
//函数名：  _draw_circle_8
//作者：    xiao冯@全动电子
//日期：    2013-02-22
//功能：    8对称性画圆算法(内部调用)
//输入参数：(xc,yc) :圆中心坐标
//      (x,y):光标相对于圆心的坐标
//          c:填充的颜色
//返回值：  无
//修改记录：无
//******************************************************************  
void LCD:: _draw_circle_8(int xc, int yc, int x, int y, u16 c)
{
  GUI_DrawPoint(xc + x, yc + y, c);

  GUI_DrawPoint(xc - x, yc + y, c);

  GUI_DrawPoint(xc + x, yc - y, c);

  GUI_DrawPoint(xc - x, yc - y, c);

  GUI_DrawPoint(xc + y, yc + x, c);

  GUI_DrawPoint(xc - y, yc + x, c);

  GUI_DrawPoint(xc + y, yc - x, c);

  GUI_DrawPoint(xc - y, yc - x, c);
} 



//******************************************************************
//函数名：  gui_circle
//作者：    xiao冯@全动电子
//日期：    2013-02-22
//功能：    在指定位置画一个指定大小的圆(填充)
//输入参数：(xc,yc) :圆中心坐标
//          c:填充的颜色
//      r:圆半径
//      fill:填充判断标志，1-填充，0-不填充
//返回值：  无
//修改记录：无
//******************************************************************  
void LCD:: gui_circle(int xc, int yc,u16 c,int r, int fill)
{
  int x = 0, y = r, yi, d;

  d = 3 - 2 * r;


  if (fill) 
  {
    // 如果填充（画实心圆）
    while (x <= y) {
      for (yi = x; yi <= y; yi++)
        _draw_circle_8(xc, yc, x, yi, c);

      if (d < 0) {
        d = d + 4 * x + 6;
      } else {
        d = d + 4 * (x - y) + 10;
        y--;
      }
      x++;
    }
  } else 
  {
    // 如果不填充（画空心圆）
    while (x <= y) {
      _draw_circle_8(xc, yc, x, y, c);
      if (d < 0) {
        d = d + 4 * x + 6;
      } else {
        d = d + 4 * (x - y) + 10;
        y--;
      }
      x++;
    }
  }
}



//******************************************************************
//函数名：  LCD_ShowChar
//作者：    xiao冯@全动电子
//日期：    2013-02-22
//功能：    显示单个英文字符
//输入参数：(x,y):字符显示位置起始坐标
//          fc:前置画笔颜色
//      bc:背景颜色
//      num:数值（0-94）
//      lenth:字体大小
//      mod:模式  0,填充模式;1,叠加模式
//返回值：  无
//修改记录：无
//******************************************************************  
void LCD:: LCD_ShowChar(u16 x,u16 y,u16 fc, u16 bc, u8 num,u8 lenth,u8 mod)
{  
    u8 temp;
    u8 pos,t;
  u16 colortemp=POINT_COLOR;      
       
  num=num-' ';//得到偏移后的值
  LCD_SetWindows(x,y,x+lenth/2-1,y+lenth-1);//设置单个文字显示窗口
  if(!mod) //非叠加方式
  {
    
    for(pos=0;pos<lenth;pos++)
    {
      if(lenth==12){temp=asc2_1206[num][pos];Serial.write(asc2_1206[num][pos]);}//调用1206字体
      else {temp=asc2_1608[num][pos];Serial.write(asc2_1206[num][pos]);}     //调用1608字体
      for(t=0;t<lenth/2;t++)
        {                 
            if(temp&0x01)LCD_WR_DATA(fc); 
        else LCD_WR_DATA(bc); 
        temp>>=1; 
        
        }
      
    } 
  }else//叠加方式
  {
    for(pos=0;pos<lenth;pos++)
    {
      if(lenth==12){temp=asc2_1206[num][pos];Serial.write(asc2_1206[num][pos]);}//调用1206字体
      else {temp=asc2_1608[num][pos];Serial.write(asc2_1206[num][pos]);}     //调用1608字体
      for(t=0;t<lenth/2;t++)
        {   
        POINT_COLOR=fc;              
            if(temp&0x01)LCD_DrawPoint(x+t,y+pos);//画一个点    
            temp>>=1; 
        }
    }
  }
  POINT_COLOR=colortemp;  
  LCD_SetWindows(0,0,lcddev.width-1,lcddev.height-1);//恢复窗口为全屏              
} 

//******************************************************************
//函数名：  LCD_ShowString
//作者：    xiao冯@全动电子
//日期：    2013-02-22
//功能：    显示英文字符串
//输入参数：x,y :起点坐标   
//      lenth:字体大小
//      *p:字符串起始地址
//      mod:模式 0,填充模式;1,叠加模式
//返回值：  无
//修改记录：无
//******************************************************************      
void LCD:: LCD_ShowString(u16 x,u16 y,u8 lenth,const char *p,u8 mod)
{         
    while((*p<='~')&&(*p>=' '))//判断是不是非法字符!
    {   
    if(x>(lcddev.width-1)||y>(lcddev.height-1)) 
    return;     
        LCD_ShowChar(x,y,POINT_COLOR,BACK_COLOR,*p,lenth,mod);
        x+=lenth/2;
        p++;
    }  
} 


//******************************************************************
//函数名：  mypow
//作者：    xiao冯@全动电子
//日期：    2013-02-22
//功能：    求m的n次方(gui内部调用)
//输入参数：m:乘数
//          n:幂
//返回值：  m的n次方
//修改记录：无
//******************************************************************  
u32  mypow(u8 m,u8 n)
{
  u32 result=1;  
  while(n--)result*=m;    
  return result;
}




//******************************************************************
//函数名：  LCD_ShowNum
//作者：    xiao冯@全动电子
//日期：    2013-02-22
//功能：    显示单个数字变量值
//输入参数：x,y :起点坐标   
//      len :指定显示数字的位数
//      lenth:字体大小(12,16)
//      color:颜色
//      num:数值(0~4294967295)
//返回值：  无
//修改记录：无
//******************************************************************         
void LCD:: LCD_ShowNum(u16 x,u16 y,u32 num,u8 len,u8 lenth)
{           
  u8 t,temp;
  u8 enshow=0;               
  for(t=0;t<len;t++)
  {
    temp=(num/mypow(10,len-t-1))%10;
    if(enshow==0&&t<(len-1))
    {
      if(temp==0)
      {
        LCD_ShowChar(x+(lenth/2)*t,y,POINT_COLOR,BACK_COLOR,' ',lenth,1);
        continue;
      }else enshow=1; 
       
    }
    LCD_ShowChar(x+(lenth/2)*t,y,POINT_COLOR,BACK_COLOR,temp+'0',lenth,1); 
  }
} 
//******************************************************************
//函数名：  GUI_DrawFont16
//作者：    xiao冯@全动电子
//日期：    2013-02-22
//功能：    显示单个16X16中文字体
//输入参数：x,y :起点坐标
//      fc:前置画笔颜色
//      bc:背景颜色  
//      s:字符串地址
//      mod:模式 0,填充模式;1,叠加模式
//返回值：  无
//修改记录：无
//******************************************************************
void LCD:: GUI_DrawFont16(u16 x, u16 y, u16 fc, u16 bc, const char *s,u8 mod)
{
  u8 i,j;
  u16 k;
  u16 HZnum;
  u16 x0=x;
  HZnum=sizeof(tfont16)/sizeof(typFNT_GB16);  //自动统计汉字数目
  
      
  for (k=0;k<HZnum;k++) 
  {
    if ((tfont16[k].Index[0]==*(s))&&(tfont16[k].Index[1]==*(s+1)))
    {   LCD_SetWindows(x,y,x+16-1,y+16-1);
        for(i=0;i<16*2;i++)
        {
        for(j=0;j<8;j++)
          { 
          if(!mod) //非叠加方式
          {
            if(tfont16[k].Msk[i]&(0x80>>j)) LCD_WR_DATA_16Bit(fc);
            else LCD_WR_DATA_16Bit(bc);
          }
          else
          {
            POINT_COLOR=fc;
            if(tfont16[k].Msk[i]&(0x80>>j)) LCD_DrawPoint(x,y);//画一个点
            x++;
            if((x-x0)==16)
            {
              x=x0;
              y++;
              break;
            }
          }

        }
        
      }
      
      
    }           
    continue;  //查找到对应点阵字库立即退出，防止多个汉字重复取模带来影响
  }

  LCD_SetWindows(0,0,lcddev.width-1,lcddev.height-1);//恢复窗口为全屏  
} 



//******************************************************************
//函数名：  GUI_DrawFont24
//作者：    xiao冯@全动电子
//日期：    2013-02-22
//功能：    显示单个24X24中文字体
//输入参数：x,y :起点坐标
//      fc:前置画笔颜色
//      bc:背景颜色  
//      s:字符串地址
//      mod:模式 0,填充模式;1,叠加模式
//返回值：  无
//修改记录：无
//******************************************************************
void LCD:: GUI_DrawFont24(u16 x, u16 y, u16 fc, u16 bc,const char *s,u8 mod)
{
  u8 i,j;
  u16 k;
  u16 HZnum;
  u16 x0=x;
  HZnum=sizeof(tfont24)/sizeof(typFNT_GB24);  //自动统计汉字数目
    
      for (k=0;k<HZnum;k++) 
      {
        if ((tfont24[k].Index[0]==*(s))&&(tfont24[k].Index[1]==*(s+1)))
        {   LCD_SetWindows(x,y,x+24-1,y+24-1);
            for(i=0;i<24*3;i++)
            {
              for(j=0;j<8;j++)
              {
                if(!mod) //非叠加方式
                {
                  if(tfont24[k].Msk[i]&(0x80>>j)) LCD_WR_DATA_16Bit(fc);
                  else LCD_WR_DATA_16Bit(bc);
                }
              else
              {
                POINT_COLOR=fc;
                if(tfont24[k].Msk[i]&(0x80>>j)) LCD_DrawPoint(x,y);//画一个点
                x++;
                if((x-x0)==24)
                {
                  x=x0;
                  y++;
                  break;
                }
              }
            }
          }
          
          
        }           
        continue;  //查找到对应点阵字库立即退出，防止多个汉字重复取模带来影响
      }

  LCD_SetWindows(0,0,lcddev.width-1,lcddev.height-1);//恢复窗口为全屏  
}




//******************************************************************
//函数名：  GUI_DrawFont32
//作者：    xiao冯@全动电子
//日期：    2013-02-22
//功能：    显示单个32X32中文字体
//输入参数：x,y :起点坐标
//      fc:前置画笔颜色
//      bc:背景颜色  
//      s:字符串地址
//      mod:模式 0,填充模式;1,叠加模式
//返回值：  无
//修改记录：无
//****************************************************************** 
void LCD:: GUI_DrawFont32(u16 x, u16 y, u16 fc, u16 bc,const char *s,u8 mod)
{
  u8 i,j;
  u16 k;
  u16 HZnum;
  u16 x0=x;
  HZnum=sizeof(tfont32)/sizeof(typFNT_GB32);  //自动统计汉字数目
  for (k=0;k<HZnum;k++) 
      {
        if ((tfont32[k].Index[0]==*(s))&&(tfont32[k].Index[1]==*(s+1)))
        {   LCD_SetWindows(x,y,x+32-1,y+32-1);
            for(i=0;i<32*4;i++)
            {
            for(j=0;j<8;j++)
              {
              if(!mod) //非叠加方式
              {
                if(tfont32[k].Msk[i]&(0x80>>j)) LCD_WR_DATA_16Bit(fc);
                else LCD_WR_DATA_16Bit(bc);
              }
              else
              {
                POINT_COLOR=fc;
                if(tfont32[k].Msk[i]&(0x80>>j)) LCD_DrawPoint(x,y);//画一个点
                x++;
                if((x-x0)==32)
                {
                  x=x0;
                  y++;
                  break;
                }
              }
            }
          }
          
          
        }           
        continue;  //查找到对应点阵字库立即退出，防止多个汉字重复取模带来影响
      }
  
  LCD_SetWindows(0,0,lcddev.width-1,lcddev.height-1);//恢复窗口为全屏  
} 

//******************************************************************
//函数名：  Show_Str
//作者：    xiao冯@全动电子
//日期：    2013-02-22
//功能：    显示一个字符串,包含中英文显示
//输入参数：x,y :起点坐标
//      fc:前置画笔颜色
//      bc:背景颜色
//      str :字符串   
//      lenth:字体大小
//      mod:模式 0,填充模式;1,叠加模式
//返回值：  无
//修改记录：无
//******************************************************************               
void LCD:: Show_Str(u16 x, u16 y, u16 fc, u16 bc,const char *str,u8 lenth,u8 mod)
{         
  u16 x0=x;                   
    u8 bHz=0;     //字符或者中文 
    while(*str!=0)//数据未结束
    { 
        if(!bHz)
        {
      if(x>(lcddev.width-lenth/2)||y>(lcddev.height-lenth)) 
      return; 
          if(*str>0x80)bHz=1;//中文 
          else              //字符
          {          
            if(*str==0x0D)//换行符号
            {         
                y+=lenth;
          x=x0;
                str++; 
            }  
            else
        {
          if(lenth==12||lenth==16)
          {  
          LCD_ShowChar(x,y,fc,bc,*str,lenth,mod);
          x+=lenth/2; //字符,为全字的一半 
          }
          else//字库中没有集成16X32的英文字体,用8X16代替
          {
            LCD_ShowChar(x,y,fc,bc,*str,16,mod);
            x+=8; //字符,为全字的一半 
          }
        } 
        str++; 
            
          }
        }else//中文 
        {   
      if(x>(lcddev.width-lenth)||y>(lcddev.height-lenth)) 
      return;  
            bHz=0;//有汉字库    
      if(lenth==32)
      GUI_DrawFont32(x,y,fc,bc,str,mod);   
      else if(lenth==24)
      GUI_DrawFont24(x,y,fc,bc,str,mod); 
      else
      GUI_DrawFont16(x,y,fc,bc,str,mod);
        
          str+=2; 
          x+=lenth;//下一个汉字偏移     
        }            
    }   
}



//******************************************************************
//函数名：  Gui_StrCenter
//作者：    xiao冯@全动电子
//日期：    2013-02-22
//功能：    居中显示一个字符串,包含中英文显示
//输入参数：x,y :起点坐标
//      fc:前置画笔颜色
//      bc:背景颜色
//      str :字符串   
//      lenth:字体大小
//      mod:模式 0,填充模式;1,叠加模式
//返回值：  无
//修改记录：无
//******************************************************************   
void LCD:: Gui_StrCenter(u16 x, u16 y, u16 fc, u16 bc,const char *str,u8 lenth,u8 mod)
{
  u16 len=strlen((const char *)str);
  u16 x1=(lcddev.width-len*8)/2;
  Show_Str(x+x1,y,fc,bc,str,lenth,mod);
}


  
//******************************************************************
//函数名：  Gui_Drawbmp16
//作者：    xiao冯@全动电子
//日期：    2013-02-22
//功能：    显示一副16位BMP图像
//输入参数：x,y :起点坐标
//      *p :图像数组起始地址
//返回值：  无
//修改记录：无
//******************************************************************  
void LCD:: Gui_Drawbmp16(u16 lenth,u16 width,u16 x,u16 y,const unsigned char *p) //显示40*40 QQ图片
{
    int i; 
  unsigned char picH,picL; 
  LCD_SetWindows(x,y,x+lenth-1,y+width-1);//窗口设置
    for(i=0;i<lenth*width;i++)
  { 
    picL=*(p+i*2);  //数据低位在前
    picH=*(p+i*2+1);        
    LCD_WR_DATA_16Bit(picH<<8|picL);              
  } 
  LCD_SetWindows(0,0,lcddev.width-1,lcddev.height-1);//恢复显示窗口为全屏  

}

