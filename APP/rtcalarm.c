#include "2410lib.h"
#include "2410addr.h"
#include "def.h"

//触摸屏用到的宏定义
#define ADCPRS 39
#define ITERATION 5	

//LCD显示用到的宏定义
#define MVAL		(13)
#define MVAL_USED 	(0)		//0=each frame   1=rate by MVAL
#define INVVDEN		(1)		//0=normal       1=inverted
#define BSWP		(0)		//Byte swap control
#define HWSWP		(1)		//Half word swap control

#define M5D(n) ((n) & 0x1fffff)	//获取最低的21位

//TFT 800480
#define LCD_XSIZE_TFT_800480 	(800)	//定义LCD屏幕的宽度
#define LCD_YSIZE_TFT_800480 	(480)	//定义LCD屏幕的高度

//TFT 800480
#define SCR_XSIZE_TFT_800480	(800)
#define SCR_YSIZE_TFT_800480 	(480)

//TFT800480
#define HOZVAL_TFT_800480   (LCD_XSIZE_TFT_800480-1)
#define LINEVAL_TFT_800480	(LCD_YSIZE_TFT_800480-1)

//Timing parameter for LCD
#define VBPD_800480		(9)		  //垂直同步信号的后肩
#define VFPD_800480		(6)		  //垂直同步信号的前肩
#define VSPW_800480		(2)		  //垂直同步信号的脉宽

#define HBPD_800480		(10)	  //水平同步信号的后肩
#define HFPD_800480		(67)	  //水平同步信号的前肩
#define HSPW_800480		(20)	  //水平同步信号的脉宽
#define CLKVAL_TFT_800480	(1)   //22.5MHZ	

#define FONTDATAMAX 2048          //定义格式化字模变量的数组大小

#define CHAR_ROW_CNT (8)
#define CHAR_ROW_CNT_HALF (4)
#define CHAR_SIZE (64)
#define CHAR_SIZE_HALF (32)

#define BGCOLOR 0x00ff

extern const unsigned char fontdata_8x8[FONTDATAMAX];   //声明格式化字模表（定义参见：font_8x8.c）。
//声明屏幕中使用到的"现在时间下一闹钟停止："这些文字的字模数组，用于显示在LCD屏幕上（定义参见：font_8x8.c）。
extern const unsigned char char_set[12][512];  
extern const unsigned char num_set[13][256];    //声明"0123456789"这些数字的字模数组，用于显示在LCD屏幕上（定义参见：font_8x8.c）。

//声明闹钟部分要用到的函数
void OpenRtc(void);//开启时钟
void OpenAlarm(void);//开启闹钟
void Get_Rtc(void); //实时时钟显示
void setRTCtime(U8,U8,U8,U8,U8,U8,U8);//设置当前时间
void setRTCalm(U8,U8,U8,U8,U8,U8);//设置响铃时间
void setNextRTCalm(void);//设置下一闹钟时间（自动获取数值）
void __irq IsrAlarm(void);//引发闹钟中断
void __irq Tick_Isr(void);//引发时钟中断

//定义闹钟部分要用到的全局常量
extern const int rtctime_init [6];  //外部引用当前时间的值，当前时间为2014年5月27日11时1分15秒（定义参见：timedata.c）。
extern const int rtcalm_init [3][6] ;  //外部引用闹钟响应时间，以二维数组表示设置了多个闹钟。（定义参见：timedata.c）。

//声明触摸屏部分要用到的函数
void Touch_Screen_Init(void);//初始化触摸屏
void Touch_Screen_Off(void) ;//关闭触摸屏
void __irq Touch_Screen(void);//引发触摸屏中断

//声明蜂鸣器部分要用到的函数
void Buzzer_PWM_Run(void);//声明蜂鸣器启动
void Buzzer_Stop(void);//声明蜂鸣器关闭
void Buzzer_Freq_Set(U32);//蜂鸣器参数设定

//声明LCD屏幕显示要用到的函数
void Test_Lcd_Tft_800480(void);	//声明在LCD屏幕上完成显示的全部操作。
static void Lcd_Init(void);	//声明LCD功能模块初始化。
static void Lcd_EnvidOnOff(int onoff);	//声明LCD视频和控制信号输出或者停止。
static void PutPixel(U32 x,U32 y,U32 c);	//声明LCD单个象素的显示数据输出。
static void Lcd_ClearScr(U16 c);	//声明LCD全屏填充特定颜色单元或清屏.
//static void Paint_Bmp(int x0,int y0,int h,int l,unsigned char bmp[]);	//声明在LCD屏幕上指定坐标点画一个指定大小的图片.

/*声明LCD输出所用到的函数*/
void lcd_draw_char(int loc_x, int loc_y, unsigned char c);	//声明一个方法，书写汉字。
void lcd_draw_num(int loc_x, int loc_y, unsigned char c);	//声明一个方法，书写数字。
//声明一个方法，在LCD屏幕上画出数字时钟和下一闹钟的时间。
void lcd_draw_clock(int _year, int _month, int _date, int _hour, int _min, int _sec, int loc_x, int loc_y);	

//LCD要用到的全局变量
//extern unsigned char GEC_800480[];	
volatile static unsigned short LCD_BUFER[SCR_YSIZE_TFT_800480][SCR_XSIZE_TFT_800480];


//定义一个字符数组，去除第0位，1-7位分别放周日到周六7天。
char *week[8] = { "","SUN","MON", "TUES", "WED", "THURS","FRI", "SAT" } ;
int year,month,date,weekday,hour,min,sec;	//定义年、月、日、星期、时、分、秒
int enable_beep = 1;	//蜂鸣器使用标识为1，标识开启蜂鸣器
unsigned int buf[ITERATION][2];

int rtcalm_init_index = 0;	//有关多闹钟

/********************************************************************
Function name: xmain
Description	 : 主函数
*********************************************************************/
void xmain(void)
{
	int index = 0;   //???
	
    ChangeClockDivider(1,1);
    ChangeMPllValue(0xa1,0x3,0x1);   
    Port_Init();
    Uart_Select(0);
    Uart_Init(0,115200);
	
	Test_Lcd_Tft_800480();  //为防止LCD屏幕显示内容过慢，这句应该放在程序的最前面进行加载。

	/*LCD输出"现在时间：xxxx-xx-xx xx:xx:xx"*/
	for (index = 0; index <5 ;++index)
	{
		lcd_draw_char(index*64,0,index);
	}
	lcd_draw_clock(rtctime_init[0],rtctime_init[1],rtctime_init[2],rtctime_init[3],rtctime_init[4],rtctime_init[5],0,64);
	
	/*LCD输出"下一闹钟：xxxx-xx-xx xx:xx:xx"*/
	for (index = 0; index <4 ;++index)
	{
		lcd_draw_char(index*64,150,index+5);
	}
	lcd_draw_char(256,150,4);
   
   	setRTCtime(rtctime_init[0],rtctime_init[1],rtctime_init[2],0x0,rtctime_init[3],rtctime_init[4],rtctime_init[5]);
   	//设置RTC时钟时间，参数为年、月、日、星期、时、分、秒
	rtcalm_init_index = 0;
	setNextRTCalm();
   
	OpenAlarm();  //开启闹钟
	OpenRtc();    //开启时钟
	while(1);
}

/********************************************************************
Function name: setRTCtime
Description	 : 设置时钟定时时间，参数顺序为年、月、日、星期、时、分、秒
*********************************************************************/
void setRTCtime(U8 wRTCyear,U8 wRTCmon,U8 wRTCdate,U8 wRTCday,U8 wRTChour,U8 wRTCmin,U8 wRTCsec)
{
	rRTCCON=0x01;	
	rBCDYEAR = wRTCyear;
	rBCDMON  = wRTCmon;
	rBCDDATE = wRTCdate;
	rBCDDAY  = wRTCday;
	rBCDHOUR = wRTChour;
	rBCDMIN  = wRTCmin;
	rBCDSEC  = wRTCsec;	
    rRTCCON = 0x0;	//disable RTC write
}

/********************************************************************
Function name: OpenRtc
Description	 : 开启时钟
*********************************************************************/
void OpenRtc(void)
{
    pISR_TICK=(unsigned)Tick_Isr;
    rTICNT=0xB0;//Tick time interrupt enable;Tick time count value=127
    EnableIrq(BIT_TICK);//开启RTC中断
}

/********************************************************************
Function name: CloseRtc
Description	 : 关闭时钟
*********************************************************************/
void CloseRtc(void)
{
    rTICNT &= ~(1<<7);
    DisableIrq(BIT_TICK);
}

/********************************************************************
Function name: setNextRTCalm
Description	 : 设置下一个闹钟响应时间
*********************************************************************/
void setNextRTCalm(void)
{
	while (1)
	{
		if (rtcalm_init_index > 3) 
		{
			lcd_draw_clock(0,0,0,0,0,0,0,214);
			break; //无下一闹钟数据
		}
		if (year > rtcalm_init[rtcalm_init_index][0])
		{
			++rtcalm_init_index;
			continue;
		}
		else if (year == rtcalm_init[rtcalm_init_index][0] && month > rtcalm_init[rtcalm_init_index][1])
		{
			++rtcalm_init_index;
			continue;
		}
		else if (year == rtcalm_init[rtcalm_init_index][0] && month == rtcalm_init[rtcalm_init_index][1] && 
		date > rtcalm_init[rtcalm_init_index][2])
		{
			++rtcalm_init_index;
			continue;
		}
		else if (year == rtcalm_init[rtcalm_init_index][0] && month == rtcalm_init[rtcalm_init_index][1] && 
		date == rtcalm_init[rtcalm_init_index][2] && hour > rtcalm_init[rtcalm_init_index][3])
		{
			++rtcalm_init_index;
			continue;
		}
		else if (year == rtcalm_init[rtcalm_init_index][0] && month == rtcalm_init[rtcalm_init_index][1] && 
		date == rtcalm_init[rtcalm_init_index][2] && hour == rtcalm_init[rtcalm_init_index][3] && 
		min > rtcalm_init[rtcalm_init_index][4])
		{
			++rtcalm_init_index;
			continue;
		}
		else if (year == rtcalm_init[rtcalm_init_index][0] && month == rtcalm_init[rtcalm_init_index][1] && 
		date == rtcalm_init[rtcalm_init_index][2] && hour == rtcalm_init[rtcalm_init_index][3] && 
		min == rtcalm_init[rtcalm_init_index][4] && sec > rtcalm_init[rtcalm_init_index][5])
		{
			++rtcalm_init_index;
			continue;
		}
		else 
		{
			rRTCCON=0x0001;  //最后一个位为1代表RTC控制器开启	
			rALMYEAR =  rtcalm_init[rtcalm_init_index][0];
			rALMMON  =  rtcalm_init[rtcalm_init_index][1];
			rALMDATE =  rtcalm_init[rtcalm_init_index][2];
			rALMHOUR =  rtcalm_init[rtcalm_init_index][3];
			rALMMIN  =  rtcalm_init[rtcalm_init_index][4];
			rALMSEC  =  rtcalm_init[rtcalm_init_index][5];
			rRTCCON = 0x0;	//关闭RTC控制器
			//LCD显示（刷新）下一闹钟时间
			lcd_draw_clock(rtcalm_init[rtcalm_init_index][0],rtcalm_init[rtcalm_init_index][1],rtcalm_init[rtcalm_init_index][2],
				rtcalm_init[rtcalm_init_index][3],rtcalm_init[rtcalm_init_index][4],rtcalm_init[rtcalm_init_index][5],0,214);
			
			//为下次设置闹钟作准备
			++rtcalm_init_index;
			break;
		}
	}
}

/********************************************************************
Function name: setRTCalm
Description	 : 设置闹钟响应时间，参数顺序为年、月、日、时、分、秒
*********************************************************************/
void setRTCalm(U8 almyear,U8 almmon,U8 almdate,U8 almhour,U8 almmin,U8 almsec)
{
	rRTCCON=0x0001;  //最后一个位为1代表RTC控制器开启
	rALMYEAR = almyear;
	rALMMON  = almmon;
	rALMDATE = almdate;
	rALMHOUR = almhour;
	rALMMIN  = almmin;
	rALMSEC  = almsec;
	rRTCCON = 0x0;	//关闭RTC控制器
}

/********************************************************************
Function name: OpenAlarm
Description	 : 开启闹钟
*********************************************************************/
void OpenAlarm(void)
{
	pISR_RTC = (unsigned)IsrAlarm;
	ClearPending(BIT_RTC);	
	rRTCALM = (0x7f);	//enable alarm
	EnableIrq(BIT_RTC);		
}

/********************************************************************
Function name: CloseAlarm
Description	 : 关闭闹钟
*********************************************************************/
void CloseAlarm(void)
{
	rRTCALM = 0;			//disable alarm
	DisableIrq(BIT_RTC);
}

/********************************************************************
Function name: Get_Rtc
Description	 : 实时时钟显示程序
*********************************************************************/
void Get_Rtc(void)
{
	rRTCCON = 0x01;  //RTC读写使能，选择BCD时钟、计数器，无复位，1/32768
	if (rBCDYEAR == 0x99)    
		year = 0x1999;
	else    
		year = 0x2000 + rBCDYEAR;
	month=rBCDMON;
	date=rBCDDATE;
	weekday=rBCDDAY;
	hour=rBCDHOUR;
	min=rBCDMIN;
	sec=rBCDSEC;
	rRTCCON = 0x0;  //RTC读写禁止，选择BCD时钟、计数器，无复位，1/32768
}

/********************************************************************
Function name: IsrAlarm
Description	 : 闹钟中断程序
*********************************************************************/
void __irq IsrAlarm(void)
{
    ClearPending(BIT_RTC);
    Uart_Printf("s3c244A RTCALM  oucer \n");
    Buzzer_PWM_Run();
    /*输出"停止"二字*/
	lcd_draw_char(336,400,9);
	lcd_draw_char(400,400,10);
	//Delay(1000);
    Touch_Screen_Init();
}

/********************************************************************
Function name: Tick_Isr
Description	 : 时钟中断程序，在串口显示时间
*********************************************************************/
void __irq Tick_Isr(void)
{
	ClearPending(BIT_TICK);
	Get_Rtc();
	//Uart_Printf("RTC TIME : %4x-%02x-%02x - %s - %02x:%02x:%02x\n",year,month,date,week[weekday],hour,min,sec);
	lcd_draw_clock(year,month,date,hour,min,sec,0,64);
}

/********************************************************************
Function name: Touch_Screen_Init
Description	 : 触摸屏初始化
*********************************************************************/
void Touch_Screen_Init(void)
{
    rADCDLY = (30000);    // ADC Start or Interval Delay
    rADCCON = (1<<14)|(ADCPRS<<6)|(0<<3)|(0<<2)|(0<<1)|(0); 
    // Enable Prescaler,Prescaler,AIN5/7 fix,Normal,Disable read start,No operation
    
    rADCTSC = (0<<8)|(1<<7)|(1<<6)|(0<<5)|(1<<4)|(0<<3)|(0<<2)|(3);//tark
    // Down,YM:GND,YP:AIN5,XM:Hi-z,XP:AIN7,XP pullup En,Normal,Waiting for interrupt mode

	rSUBSRCPND |= BIT_SUB_TC;   //rSUBSRCPND：0x4a000018   BIT_SUB_TC：0x1<<9
    rINTSUBMSK  =~(BIT_SUB_TC);  //0x4a00001c
     
   	pISR_ADC    = (unsigned)Touch_Screen;
    
    rINTMSK    &= ~(BIT_ADC);
    rINTSUBMSK &= ~(BIT_SUB_TC);
    
    Uart_Printf( "\ntouch screen stop ring\n" ) ;   
}

/********************************************************************
Function name: Touch_Screen_Off
Description	 : 触摸屏关闭
*********************************************************************/
void Touch_Screen_Off(void)
{
	rINTMSK    |= (BIT_ADC);
    rINTSUBMSK |= (BIT_SUB_TC);
}

/********************************************************************
Function name: Touch_Screen
Description	 : 触发触摸屏中断
*********************************************************************/
void __irq Touch_Screen(void)
{
	int i;
	int total_x=0;
	int avg_x=0;
	int total_y=0;
	int avg_y=0;
    rINTSUBMSK |= (BIT_SUB_ADC | BIT_SUB_TC);     //Mask sub interrupt (ADC and TC) 

    //Uart_Printf("\nTS Down!\n");
        
      //Auto X-Position and Y-Position Read
    rADCTSC=(1<<7)|(1<<6)|(0<<5)|(1<<4)|(1<<3)|(1<<2)|(0);
          //[7] YM=GND, [6] YP is connected with AIN[5], [5] XM=Hi-Z, [4] XP is connected with AIN[7]
          //[3] XP pull-up disable, [2] Auto(sequential) X/Y position conversion mode, [1:0] No operation mode  

    for(i=0;i<ITERATION;i++)
    {
        rADCTSC  = (1<<7)|(1<<6)|(0<<5)|(1<<4)|(1<<3)|(1<<2)|(0);            
        rADCCON |= 0x1;             //Start Auto conversion
        while(rADCCON & 0x1);       //Check if Enable_start is low 	//0:0 skip loop
        while(!(0x8000&rADCCON));   //Check ECFLG   				//5:1 skip loop
    
        //保证高6位作废，前面10位有效，0x3ff=0000 0011 1111 1111，rADCDAT0代表坐标x，rADCDAT1代表y。
        buf[i][0] = (0x3ff&rADCDAT0);   
        buf[i][1] = (0x3ff&rADCDAT1);
    }

    for(i=0;i<ITERATION;i++){
		total_x+=buf[i][0];
		total_y+=buf[i][1];
        //Uart_Printf("X %4d, Y %4d\n", buf[i][0], buf[i][1]);
    }
	avg_x=total_x/ITERATION;
	avg_y=total_y/ITERATION;
	if(avg_x<=550&&avg_x>400&&avg_y<=200&&avg_y>100)
	{
		Buzzer_Stop();
		setNextRTCalm();
		/*擦除"停止"二字*/
		lcd_draw_char(336,400,11);
		lcd_draw_char(400,400,11);
		Touch_Screen_Off();
	}
    rADCTSC = (1<<7)|(1<<6)|(0<<5)|(1<<4)|(0<<3)|(0<<2)|(3);  
      //[7] YM=GND, [6] YP is connected with AIN[5], [5] XM=Hi-Z, [4] XP is connected with AIN[7]
      //[3] XP pull-up enable, [2] Normal ADC conversion, [1:0] Waiting for interrupt mode                
    rSUBSRCPND |= BIT_SUB_TC;
   	rINTSUBMSK  =~(BIT_SUB_TC);   //Unmask sub interrupt (TC)     
    ClearPending(BIT_ADC);
}

/********************************************************************
Function name: Buzzer_Freq_Set
Description	 : 蜂鸣器参数设定
*********************************************************************/
void Buzzer_Freq_Set(U32 freq)
{   
	rGPBCON = rGPBCON & ~(3<<0)|(1<<1);//set GPB0 as tout0, pwm output
		
	rTCFG0 = rTCFG0 & ~0xff|15; //prescaler = 15
	rTCFG1 = rTCFG1 & ~0xf|2;//divider = 1/8
			
	rTCNTB0 = (PCLK>>7)/freq;//rTCNTB0=PCLK/{(prescaler+1) * divider *freq}
	rTCMPB0 = rTCNTB0>>1;	//占空比50%
	
	//disable deadzone, auto-reload, inv-off, update TCNTB0&TCMPB0, start timer 0
	rTCON = rTCON & ~0x1f|(0<<4)|(1<<3)|(0<<2)|(1<<1)|(1);
	rTCON &= ~(1<<1);			//clear manual update bit
}

/********************************************************************
Function name: Buzzer_Stop
Description	 : 蜂鸣器停止发声
*********************************************************************/
void Buzzer_Stop( void )
{
	rGPBCON |= 1;		
	rGPBCON = rGPBCON & ~3|1;			//set GPB0 as output
	rGPBDAT &= ~1;//output 0
}

/********************************************************************
Function name: Buzzer_PWM_Run
Description	 : 蜂鸣器发声
********************************************************************/ 
void Buzzer_PWM_Run( void )
{
	U16 freq = 1500 ;
	Buzzer_Freq_Set( freq ) ;
}

/********************************************************************
Function name: PutPixel
Description	 : LCD单个象素的显示数据输出
*********************************************************************/
static void PutPixel(U32 x,U32 y,U32 c)
{
	if ( (x < SCR_XSIZE_TFT_800480) && (y < SCR_YSIZE_TFT_800480) )
		LCD_BUFER[(y)][(x)] = c;
}

/********************************************************************
Function name: Lcd_Port_Init
Description	 : LCD数据和控制端口初始化
*********************************************************************/
static void Lcd_Port_Init(void)
{
    rGPCUP = 0x0; // enable Pull-up register
    rGPCCON = 0xaaaa56a9; //Initialize VD[7:0],LCDVF[2:0],VM,VFRAME,VLINE,VCLK,LEND 
    //rGPCCON = 0xaaaaaaaa;
    rGPDUP = 0x0 ; // enable Pull-up register
    rGPDCON=0xaaaaaaaa; //Initialize VD[15:8]
}

/********************************************************************
Function name: Lcd_Init
Description	 : LCD功能模块初始化
*********************************************************************/
static void Lcd_Init(void)
{
	//CLKVAL=1;MMODE=0;PNRMODE=11:11 = TFT LCD panel
	//BPPMODE=1100=16 bpp for TFT;ENVID0=Disable the video output and the LCD control signal.
	rLCDCON1=(CLKVAL_TFT_800480<<8)|(MVAL_USED<<7)|(3<<5)|(12<<1)|0;
    //VBPD=9;LINEVAL=799;VFPD=6;VSPW=2
	rLCDCON2=(VBPD_800480<<24)|(LINEVAL_TFT_800480<<14)|(VFPD_800480<<6)|(VSPW_800480);
	//HBPD=10;HOZVAL=479;HFPD=16
	rLCDCON3=(HBPD_800480<<19)|(HOZVAL_TFT_800480<<8)|(HFPD_800480);
	//MVAL=13;HSPW=20
	rLCDCON4=(MVAL<<8)|(HSPW_800480);
	//FRM5:6:5,HSYNC and VSYNC are inverted
	rLCDCON5=(1<<11)|(0<<10) |(1<<9)|(1<<8)|(0<<7) |(0<<6)|(BSWP<<1)|(HWSWP);	
	
	rLCDSADDR1=(((U32)LCD_BUFER>>22)<<21)|M5D((U32)LCD_BUFER>>1);
	rLCDSADDR2=M5D( ((U32)LCD_BUFER+(SCR_XSIZE_TFT_800480*LCD_YSIZE_TFT_800480*2))>>1 );
	rLCDSADDR3=(((SCR_XSIZE_TFT_800480-LCD_XSIZE_TFT_800480)/1)<<11)|(LCD_XSIZE_TFT_800480/1);
	
	rLCDINTMSK|=(3); // MASK LCD Sub Interrupt
	rLPCSEL&=(~7); // Disable LPC3600
	rTPAL=0; // Disable Temp Palette
}

/********************************************************************
Function name: Lcd_EnvidOnOff
Description	 : LCD视频和控制信号输出或者停止，1开启视频输出
*********************************************************************/
static void Lcd_EnvidOnOff(int onoff)
{
    if(onoff==1)
	rLCDCON1|=1; // ENVID=ON
    else
	rLCDCON1 =rLCDCON1 & 0x3fffe; // ENVID Off
}

/********************************************************************
Function name: Lcd_ClearScr
Description	 : LCD全屏填充特定颜色单元或清屏
*********************************************************************/
static void Lcd_ClearScr(U16 c)
{
	unsigned int x,y ;
		
    for( y = 0 ; y < SCR_YSIZE_TFT_800480; y++ )
    {
    	for( x = 0 ; x < SCR_XSIZE_TFT_800480; x++ )
    	{
			LCD_BUFER[y][x] = c;
    	}
    }
}

/********************************************************************
Function name: Paint_Bmp
Description	 : 在LCD屏幕上指定坐标点画一个指定大小的图片
*********************************************************************/
/*
static void Paint_Bmp(int x0,int y0,int h,int l,unsigned char bmp[])
{
	int x,y;
	U32 c;
	int p = 0;

	//本图片显示是正的显示方式
    for( y = 0 ; y < l ; y++ )
    {
    	for( x = 0 ; x < h ; x++ )
    	{
    		c = bmp[p+1] | (bmp[p]<<8) ;

			if ( ( (x0+x) < SCR_XSIZE_TFT_800480) && ( (y0+y) < SCR_YSIZE_TFT_800480) )
				LCD_BUFER[y0+y][x0+x] = c ;

    		p = p + 2 ;
    	}
    }
}
*/
/********************************************************************
Function name: Test_Lcd_Tft_800480
Description	 : 在LCD屏幕上完成显示的全部操作
*********************************************************************/
void Test_Lcd_Tft_800480( void )
{
    Lcd_Port_Init();          //LCD数据和控制端口初始化
    Lcd_Init();				  //LCD功能模块初始化
    Lcd_EnvidOnOff(1);		  //开启视频输出
	Lcd_ClearScr(BGCOLOR);   //设置背景色
    //Paint_Bmp( 0,0,800,480, GEC_800480) ;		//在LCD屏幕上指定坐标点画一个指定大小的图片
}

/********************************************************************
Function name: lcd_draw_char
Description	 : 在LCD屏幕上指定坐标写一个汉字
*********************************************************************/
void lcd_draw_char(int loc_x, int loc_y, unsigned char c)
{
    int i,j;
    unsigned char line_dots[CHAR_ROW_CNT];
    
    for (i = 0; i < CHAR_SIZE; ++i)
    {
        for (j = 0 ; j < CHAR_ROW_CNT ; ++j)
        {
        	line_dots[j] = char_set[c][(i * CHAR_ROW_CNT) + j];
        }
        
        for (j = 0; j < CHAR_SIZE; ++j)
        {
            if (line_dots[(j / 8)] & (0x80 >> (j % 8)))
            {
                PutPixel(loc_x+j,loc_y+i,0xffffff);
            }
            else
            {
                PutPixel(loc_x+j,loc_y+i,BGCOLOR);
            }
        }
    }
}

/********************************************************************
Function name: lcd_draw_num
Description	 : 在LCD屏幕上指定坐标写一个数字
*********************************************************************/
void lcd_draw_num(int loc_x, int loc_y, unsigned char c)
{
    int i,j;
    unsigned char line_dots[CHAR_ROW_CNT_HALF];
    
    for (i = 0; i < CHAR_SIZE; ++i)
    {
        for (j = 0 ; j < (CHAR_ROW_CNT_HALF) ; ++j)
        {
        	line_dots[j] = num_set[c][(i * CHAR_ROW_CNT_HALF) + j];
        }
        
        for (j = 0; j < CHAR_SIZE_HALF; ++j)
        {
            if (line_dots[(j / 8)] & (0x80 >> (j % 8)))
            {
                PutPixel(loc_x+j,loc_y+i,0xffffff);
            }
            else
            {
                PutPixel(loc_x+j,loc_y+i,BGCOLOR);
            }
        }
    }
}

/********************************************************************
Function name: lcd_draw_clock
Description	 : 在LCD屏幕上输出时间，输出样式为XXXX-XX-XX XX:XX:XX
*********************************************************************/
void lcd_draw_clock(int _year, int _month, int _date, int _hour, int _min, int _sec, int loc_x, int loc_y)
{
	int i;
	
	//假如输入年份为0，清空显示
	if (_year == 0)
	{
		for (i = 0; i < 19 ;++i)
		{
			lcd_draw_num(loc_x+i*CHAR_SIZE_HALF,loc_y,12);
		}
		return ;
	}
	
	//输出固定格式 （横杠、冒号）
	for (i = 0; i < 19 ;++i)
	{
		if (i == 4 || i == 7)
		{
			lcd_draw_num(loc_x+i*CHAR_SIZE_HALF,loc_y,10);
		}
		else if (i == 13 || i == 16)
		{
			lcd_draw_num(loc_x+i*CHAR_SIZE_HALF,loc_y,11);
		}
	}
	
	//输出年份
	lcd_draw_num(loc_x,loc_y,((_year >> 12) & 0x000f));
	lcd_draw_num(loc_x+CHAR_SIZE_HALF,loc_y,((_year >> 8) & 0x000f));
	lcd_draw_num(loc_x+2*CHAR_SIZE_HALF,loc_y,((_year >> 4) & 0x000f));
	lcd_draw_num(loc_x+3*CHAR_SIZE_HALF,loc_y,(_year & 0x000f));
	
	//输出月份
	lcd_draw_num(loc_x+5*CHAR_SIZE_HALF,loc_y,((_month >> 4) & 0x000f));
	lcd_draw_num(loc_x+6*CHAR_SIZE_HALF,loc_y,(_month & 0x000f));
	
	//输出日期
	lcd_draw_num(loc_x+8*CHAR_SIZE_HALF,loc_y,((_date >> 4) & 0x000f));
	lcd_draw_num(loc_x+9*CHAR_SIZE_HALF,loc_y,(_date & 0x000f));
	
	//输出小时
	lcd_draw_num(loc_x+11*CHAR_SIZE_HALF,loc_y,((_hour >> 4) & 0x000f));
	lcd_draw_num(loc_x+12*CHAR_SIZE_HALF,loc_y,(_hour & 0x000f));
	
	//输出分钟
	lcd_draw_num(loc_x+14*CHAR_SIZE_HALF,loc_y,((_min >> 4) & 0x000f));
	lcd_draw_num(loc_x+15*CHAR_SIZE_HALF,loc_y,(_min & 0x000f));
	
	//输出秒钟
	lcd_draw_num(loc_x+17*CHAR_SIZE_HALF,loc_y,((_sec >> 4) & 0x000f));
	lcd_draw_num(loc_x+18*CHAR_SIZE_HALF,loc_y,(_sec & 0x000f));
}