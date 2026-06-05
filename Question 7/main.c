/************************************************************************ 
文件名称: main.c 
作者:    
版本:    V1.01
说明:    模数转换实验 
修改记录: 由LED显示模数转换的结果，改变成由四位数码管显示电压值  
-------------------------------------------------------------------------   
* 功能描述: 采集电位器的模拟电压值，转换成数字量
* 通过四位共阳数码管显示
-------------------------------------------------------------------------
* 接线说明:
*          ADC0809：0809DB0~DB7--P00~P07 , 0809CS--P27，0809EOC--P34
*                   0809WR--P36,0809RD--P37,0809A,B,C通道选择--GND  
*                   0809CLK--ALE(注：在核心板上),0809IN0--POT（电位器输出） 
*接线说明：P10~P17-DB0~DB7，P20-RS，P21-RW，P22-EN
k1-23 k2-24                                       
*************************************************************************/
#include<reg52.h>
#include<absacc.h>
#include <intrins.h>
#include <math.h>
#define out P1
#define fan_out  P3
#define uchar unsigned char
#define uint unsigned int
//char code SST516[3] _at_ 0x003b;
#define KP 5				//比例系数
#define KI 0				//积分系数
#define KD 0				//微分系数
sbit rs=P2^0;
sbit rw=P2^1;
sbit e=P2^2;
int error;					//当前误差
int last_error;				//上一次误差
int integral;				//误差积分
int derivative;				//误差微分
int pid_output;				//PID输出值
uint speed_threshold = 0;
unsigned char code segbit[]={0xc0,0xf9,0xa4,0xb0,	// 0, 1, 2, 3
								 0x99,0x92,0x82,0xf8,0x80,0x90, 0xff};// 4, 5, 6, 7, 8, 9, off 
unsigned char code combit[]={0xf1,0xf2,0xf4,0xf8};
uchar code turn[]={0x02,0x06,0x04,0x0c,0x08,0x09,0x01,0x03};
sbit ADD=P2^3;
sbit SUB=P2^4;
 uchar dispbuf[4];
#define ADC0809 XBYTE[0x7fff]    /* 定义ADC0809 端口地址 */

sbit EOC=P3^4;
int tar_v = 0;
int pre_tar_v = 0;
void TimeInitial();
void Delay(unsigned int i);
void check_busy(void);
void write_command(uchar com);
void write_data(uchar dat);
void LCD_initial(void);
void string(uchar ad ,uchar *s);
void lcd_test(void);
void delay(uint);
void PID_Control(uint current_temp);
//步进电机转速控制
uint fan_speed = 0;			//风扇转速（0-100）
uint speed_counter = 0;		//转速控制计数器
uchar i;
void Timer0_Init(void)
{
	TMOD |= 0x01;	//设置定时器0为模式1（16位定时器）
	TH0 = 0xF8;		//定时器初值，定时约1ms
	TL0 = 0xCD;
	ET0 = 1;		//使能定时器0中断
	EA = 1;			//开总中断
	TR0 = 1;		//启动定时器0
}
void Timer0_ISR(void) interrupt 1
{
	TH0 = 0xF8;		//定时器初值，定时约1ms
	TL0 = 0xCD;

	if(fan_speed > 0)	//如果风扇转速大于0
	{
		speed_counter++;
		
		//根据转速计算步进电机切换频率
		//fan_speed越大，切换越快；fan_speed越小，切换越慢
		speed_threshold = 100 / fan_speed + 1;
		
		if(speed_counter >= speed_threshold)
		{
			speed_counter = 0;
			i = i < 8 ? i+1 : 0;	//循环切换步进电机相序
			fan_out=(fan_out&0xf0)|turn[i];	//输出到步进电机控制风扇转速
		}
	}
	else
	{
		//转速为0，停止步进电机
		fan_out=(fan_out&0xf0)|0x00;
		speed_counter = 0;
	}
}
void main()
{


	uchar  temp;
	uint   voldata, pre_voldata;

fan_out=(fan_out&0xf0)|0x03;
	LCD_initial();			     //LCD1602 初始化
	string(0x84,"WINDWAY");	 //显示字符串
		delay(100); 
	write_command(0x01);     //清屏
			dispbuf[3] = voldata%10 + '0';
		dispbuf[2] = voldata/10%10 + '0';
		dispbuf[1] = voldata/100%10 + '0';
		dispbuf[0] = voldata/1000 + '0';
		string(0x84,dispbuf);
			dispbuf[3] = tar_v%10 + '0';
		dispbuf[2] = tar_v/10%10 + '0';
		dispbuf[1] = tar_v/100%10 + '0';
		dispbuf[0] = tar_v/1000 + '0';
		string(0xC4,dispbuf);
		Timer0_Init();
	while(1)
	{
		ADC0809=0x0f;		
		do
		{;}
		while(~EOC);  //转换是否完成		
		//delayms(1);
		temp = ADC0809; //读出转换结果
		voldata = temp*1.0/255*500;
		if(pre_voldata != voldata)
		{
		pre_voldata = voldata;
		dispbuf[3] = voldata%10 + '0';
		dispbuf[2] = voldata/10%10 + '0';
		dispbuf[1] = voldata/100%10 + '0';
		dispbuf[0] = voldata/1000 + '0';
		string(0x84,dispbuf);
		}
		PID_Control(voldata);
		if(!ADD)			//正转
		{
tar_v+=100;
		dispbuf[3] = tar_v%10 + '0';
		dispbuf[2] = tar_v/10%10 + '0';
		dispbuf[1] = tar_v/100%10 + '0';
		dispbuf[0] = tar_v/1000 + '0';
		string(0xC4,dispbuf);
		}
		
		else if(!SUB)		//反转
		{
tar_v-=100;
		dispbuf[3] = tar_v%10 + '0';
		dispbuf[2] = tar_v/10%10 + '0';
		dispbuf[1] = tar_v/100%10 + '0';
		dispbuf[0] = tar_v/1000 + '0';
		string(0xC4,dispbuf);	
		} 
	}
}

 
//1ms延时程序
void delay(uint j)
{
uchar i=250;
for(;j>0;j--)
	{
	while(--i);
	i=249;
	while(--i);
	i=250;
	}
}
//查忙程序
void check_busy(void)
{
uchar dt;
do
{
dt=0xff;
e=0;
rs=0;	
rw=1;
e=1;
dt=out;
}while(dt&0x80);
e=0;
}
//写控制指令
void write_command(uchar com)
{
check_busy();
e=0;
rs=0;
rw=0;
out=com;
e=1;
_nop_();
e=0;
delay(1);
}
//写数据指令
void write_data(uchar dat)
{
check_busy();
e=0;
rs=1;
rw=0;
out=dat;
e=1;
_nop_();
e=0;
delay(1);	
}
//液晶屏初始化
void LCD_initial(void)
{
	write_command(0x38);//8位总线,双行显示，5X7的点阵字符
	write_command(0x0C);//开整体显示,光标关，无黑块
	write_command(0x06);//光标右移
	write_command(0x01);//清屏
	delay(1);
}
//输出字符串
void string(uchar ad,uchar *s)
{
write_command(ad);
while(*s>0)
	{
	write_data(*s++);
	delay(100);
	}
}
void PID_Control(uint current_v)
{
	//计算误差：目标温度 - 当前温度
	error = abs(current_v - tar_v);
	
	//计算积分项（累加误差）
	integral += error;
	
	//限制积分项，防止积分饱和
	if(integral > 1000) integral = 1000;
	if(integral < -1000) integral = -1000;
	
	//计算微分项（误差变化率）
	derivative = error - last_error;
	
	//计算PID输出
	pid_output = 0.1 * KP * error + 0.03 * KI * integral + 0.1 * KD * derivative;
	
	//保存当前误差作为下次的上一次误差
	last_error = error;
	
	//将PID输出转换为风扇转速（0-100）
	if(pid_output > 100) 
		fan_speed = 100;	//最大转速
	else if(pid_output < 0) 
		fan_speed = 0;		//停止
	else 
		fan_speed = pid_output;
}
