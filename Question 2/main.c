/*******************************************************************************  
文件名称： main.c 
作 者：    zw sangel
版 本：    V1.00
说 明：    LCD1602IO控制方式 
修改记录：  
*******************************************************************************/
/*******************************************************************************    
* 功能描述:                                                              	  *
*          程序运行后显示                                                 	  *
*          第一行：WINDWAY                                   			      *
*          第二行：A GOOD NEWS			                                      *

*接线说明：P00~P07-DB0~DB7，P20-RS，P21-RW，P22-EN
*******************************************************************************/
#include <reg52.h>
#include <intrins.h>
#include <math.h>
#define uchar unsigned char
#define uint unsigned int
#define out P0
#define out_fan  P2
/***********端口定义**********************************************************/
sbit rs=P3^4;
sbit rw=P3^5;
sbit e=P3^6;

sbit DQ=P1^4;

sbit add=P1^0;
sbit sub=P1^1;
sbit add1=P1^2;
sbit sub1=P1^3;
uchar code turn[]={0x02,0x06,0x04,0x0c,0x08,0x09,0x01,0x03};	//步进电机正转相序表
/***********函数申明**********************************************************/
void check_busy(void);
void write_command(uchar com);
void write_data(uchar dat);
void LCD_initial(void);
void string(uchar ad ,uchar *s);
void lcd_test(void);
void delay(uint);

void delay5(uchar);
void init_ds18b20(void);
uchar readbyte(void);
void writebyte(uchar);

void PID_Control(uint current_temp);
uint retemp(void);
uint count = 0;			//定时器中断计数器
int   temp, pre_temp = 0;	//当前温度值、上一次温度值（初始化）
uint   pre_target_temp = 0;	//上一次目标温度值（用于检测目标温度变化）
uchar dispbuf[4];			//显示缓冲区，存储温度的4位数字
uchar i = 0;				//步进电机相序索引（初始化）
uint speed_threshold = 0;
//PID控制相关变量
int target_temp = 250;		//目标温度（25.0°C，放大10倍）
int error;					//当前误差
int last_error;				//上一次误差
int integral;				//误差积分
int derivative;				//误差微分
int pid_output;				//PID输出值

//PID参数（根据实际系统调整）
#define KP 20				//比例系数
#define KI 1				//积分系数
#define KD 3				//微分系数

//步进电机转速控制
uint fan_speed = 0;			//风扇转速（0-100）
uint speed_counter = 0;		//转速控制计数器
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
			out_fan = turn[i];	//输出到步进电机控制风扇转速
		}
	}
	else
	{
		//转速为0，停止步进电机
		out_fan = 0x00;
		speed_counter = 0;
	}
}
/***********主程序*************************************************************/
void main(void)
{
	out_fan=0x03;					//初始化风扇控制端口

	LCD_initial();					//LCD1602初始化
	Timer0_Init();					//定时器0初始化
	
	//显示目标温度
	dispbuf[3] = target_temp%10 + '0';			//目标温度小数位
	dispbuf[1] = target_temp/10%10 + '0';		//目标温度个位
	dispbuf[0] = target_temp/100%10 + '0';		//目标温度十位
	dispbuf[2] = '.';							//小数点
	string(0xC0,"Set:");						//第二行显示"Set:"
	string(0xC4,dispbuf);						//显示目标温度



	while(1)						//主循环
	{
		if(!add1)		
		{
			target_temp++;
			delay(5);
		}
		else if(!sub1)
		{
			target_temp--;
			delay(5);
		}
		else if(!add)		
		{
			target_temp += 10;
			delay(5);
		}
		else if(!sub)
		{
			target_temp -= 10;
			delay(5);
		}

		temp=retemp();				//读取当前温度
		//temp=255;
		PID_Control(temp);			//执行PID控制
			dispbuf[3] = fan_speed%10 + '0';			//目标温度小数位
			dispbuf[1] = fan_speed/10%10 + '0';		//目标温度个位
			dispbuf[0] = fan_speed/100%10 + '0';		//目标温度十位
			dispbuf[2] = '.';							//小数点
			string(0xC9,dispbuf);						//显示目标温度		 
		if(temp != pre_temp)		//当前温度变化时刷新显示
		{
			pre_temp = temp;		//更新上一次温度值
			dispbuf[3] = temp%10 + '0';			//温度小数位
			dispbuf[1] = temp/10%10 + '0';		//温度个位
			dispbuf[0] = temp/100%10 + '0';		//温度十位
			dispbuf[2] = '.';					//小数点
			string(0x80,"Cur:");				//第一行显示"Cur:"
			string(0x84,dispbuf);				//显示当前温度
		}
		
		if(target_temp != pre_target_temp)	//目标温度变化时刷新显示
		{
			pre_target_temp = target_temp;	//更新上一次目标温度值
			dispbuf[3] = target_temp%10 + '0';			//目标温度小数位
			dispbuf[1] = target_temp/10%10 + '0';		//目标温度个位
			dispbuf[0] = target_temp/100%10 + '0';		//目标温度十位
			dispbuf[2] = '.';							//小数点
			string(0xC0,"Set:");						//第二行显示"Set:"
			string(0xC4,dispbuf);						//显示目标温度
		}



		delay(5); 
	}
}  

//1ms延时程序（约1ms/次）
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

//5us延时程序（约5us/次）
void delay5(uchar n)
{
	do
	{
		_nop_();
		_nop_();
		_nop_();
		n--;
	}
	while(n);
}
//查忙程序：检测LCD1602是否处于忙状态
void check_busy(void)
{
	uchar dt;
	do
	{
		dt=0xff;
		e=0;
		rs=0;	//指令寄存器
		rw=1;	//读模式
		e=1;	//使能
		dt=out;	//读取状态字
	}while(dt&0x80);	//D7=1表示忙
	e=0;	//关闭使能
}
//写控制指令：向LCD1602写入命令
void write_command(uchar com)
{
	check_busy();	//先检测忙状态
	e=0;
	rs=0;			//指令寄存器
	rw=0;			//写模式
	out=com;		//输出命令
	e=1;			//使能
	_nop_();
	e=0;			//关闭使能
	delay(1);		//延时
}

//写数据指令：向LCD1602写入显示数据
void write_data(uchar dat)
{
	check_busy();	//先检测忙状态
	e=0;
	rs=1;			//数据寄存器
	rw=0;			//写模式
	out=dat;		//输出数据
	e=1;			//使能
	_nop_();
	e=0;			//关闭使能
	delay(1);		//延时	
}
//液晶屏初始化：初始化LCD1602显示参数
void LCD_initial(void)
{
	write_command(0x38);	//8位总线，双行显示，5x7点阵字符
	write_command(0x0C);	//开显示，光标关，无闪烁
	write_command(0x06);	//光标右移，字符不动
	write_command(0x01);	//清屏
	delay(1);
}

//输出字符串：从指定地址开始显示字符串
void string(uchar ad, uchar *s)
{
	write_command(ad);	//设置显示地址
	while(*s>0)			//循环输出每个字符
	{
		write_data(*s++);
		delay(100);
	}
}
//从DS18B20读取一字节数据
uchar readbyte(void)
{
	uchar i=0;
	uchar date=0;
	for (i=8;i>0;i--)	//循环读取8位
	{
		DQ =0;
		delay5(1);
		DQ =1;	//释放总线，等待15us后读取数据
		date>>=1;			//右移一位
		if(DQ)				//读取数据线状态
			date|=0x80;		//如果为高电平，设置最高位
		delay5(11);			//等待剩余时间
	}
	return(date);
}
/*--------------DS18B20初始化--------------------*/
void init_ds18b20(void)
{
	 uchar x=0; 
	 DQ =0;    	//拉低总线，发出复位信号
	 delay5(120); 	//保持低电平480-960us
	 DQ =1;    	//释放总线
	 delay5(16);	//等待15-60us，DS18B20会发出存在脉冲
	 delay5(80);	//等待DS18B20响应
}
/*--------------向DS18B20写一字节------------------*/
void writebyte(uchar dat)
{
 uchar i=0;
 for(i=8;i>0;i--)
	 {
	  DQ =0;
	  DQ =dat&0x01;//写"1"时保持15us以上
	  delay5(12);	   //写"0"时保持60us以上
	  DQ = 1;	   
	  dat>>=1;
	  delay5(5);
	  }
}
/*--------------读取温度值------------------*/
uint retemp(void)
{
	uint tt;
	uchar a,b;
	uint t;
	init_ds18b20();		//DS18B20初始化
	writebyte(0xCC); 	//跳过ROM匹配
	writebyte(0x44);	//启动温度转换
	init_ds18b20();		//DS18B20初始化
	writebyte(0xCC); 	//跳过ROM匹配
	writebyte(0xBE); 	//读暂存器
	a=readbyte();		//读温度低字节
	b=readbyte();		//读温度高字节
	t=b;
	t<<=8;				//高字节左移8位
	t=t|a;				//合并高低字节
	tt=t*0.625;			//转换为实际温度值（精度0.0625°C，放大10倍）
	if(abs(pre_temp - tt) < 300) return(tt);
	return(pre_temp);
}

/*--------------PID控制器------------------*/
void PID_Control(uint current_temp)
{
	if(current_temp <=  target_temp + 2)
	{
	fan_speed = 0;
	return;
	} 
	//计算误差：目标温度 - 当前温度
	error = current_temp - target_temp;
	
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
