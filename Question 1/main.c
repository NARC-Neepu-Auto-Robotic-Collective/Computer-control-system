/*******************************************************************************
* 模拟温室大棚调温系统
* LCD1602 显示当前温度和设置温度
* DS18B20 读取当前温度
* 按键调节设置温度
* DAC0832 输出控制 LED 亮度（温差越大越亮，需要升温时亮）
* 灯负端接5V，DAC输出0V时最亮，DAC输出5V时熄灭
*******************************************************************************/
#include <reg52.h>
#include <intrins.h>

#define uchar unsigned char
#define uint unsigned int
#define out P0

/*********** 端口定义 **********************************************************/
/* LCD1602 */
sbit rs = P2^5;
sbit rw = P2^6;
sbit e  = P2^7;

/* DS18B20 */
sbit DQ = P3^7;

/* DAC0832 */
sbit DAC_CS = P2^0;   // 片选
sbit DAC_WR = P2^1;   // 写信号

/* 按键 */
sbit KEY_UP   = P2^2;   // 升高设置温度
sbit KEY_DOWN = P2^3;   // 降低设置温度

/*********** 函数声明 **********************************************************/
void check_busy(void);
void write_command(uchar com);
void write_data(uchar dat);
void LCD_initial(void);
void string(uchar ad, uchar *s);
void delay(uint);
void delay5(uchar);
void init_ds18b20(void);
uchar readbyte(void);
void writebyte(uchar);
uint retemp(void);
void write_dac(uchar dat);
void key_scan(void);
void led_control(void);

/*********** 全局变量 **********************************************************/
uint count = 0;
uint temp;                    // 当前温度(10倍值, 如250=25.0℃)
uint set_temp = 250;          // 设置温度(10倍值, 默认25.0℃)
uchar dispbuf[4];             // 当前温度显示缓冲
uchar dispbuf_set[4];         // 设置温度显示缓冲
uchar i;

/*********** 定时器0初始化 5ms定时 **********************************************/
void Timer0_Init(void)
{
	TMOD |= 0x01;
	TH0 = 0xEC;
	TL0 = 0x78;
	ET0 = 1;
	EA = 1;
	TR0 = 1;
}

/*********** 定时器0中断服务 ****************************************************/
void Timer0_ISR(void) interrupt 1
{
	TH0 = 0xEC;
	TL0 = 0x78;

	count++;
	if(count >= 200)
	{
		count = 0;
		i = i < 8 ? i + 1 : 0;
	}
}

/*********** 主函数 ************************************************************/
void main(void)
{
	DAC_CS = 1;      // DAC0832 初始不选中，释放P0总线
	DAC_WR = 1;
	LCD_initial();
	Timer0_Init();

	while(1)
	{
		/* 读取当前温度 */
		temp = retemp();

		/* 显示当前温度: "Temp:XX.XC" 第一行 */
		dispbuf[3] = temp % 10 + '0';
		dispbuf[1] = temp / 10 % 10 + '0';
		dispbuf[0] = temp / 100 % 10 + '0';
		dispbuf[2] = '.';
		string(0x80, "Temp:");
		string(0x85, dispbuf);
		write_data('C');

		/* 显示设置温度: "Set :XX.XC" 第二行 */
		dispbuf_set[3] = set_temp % 10 + '0';
		dispbuf_set[1] = set_temp / 10 % 10 + '0';
		dispbuf_set[0] = set_temp / 100 % 10 + '0';
		dispbuf_set[2] = '.';
		string(0xC0, "Set :");
		string(0xC5, dispbuf_set);
		write_data('C');

		/* 按键扫描：调节设置温度 */
		key_scan();

		/* 根据温差控制 LED 亮度 */
		led_control();

		delay(100);
	}
}

/*********** 1ms延时 ***********************************************************/
void delay(uint j)
{
	uchar i = 250;
	for(; j > 0; j--)
	{
		while(--i);
		i = 249;
		while(--i);
		i = 250;
	}
}

/*********** 5us延时 ***********************************************************/
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

/*********** 检查LCD忙状态 ******************************************************/
void check_busy(void)
{
	uchar dt;
	do
	{
		dt = 0xff;
		e = 0;
		rs = 0;
		rw = 1;
		e = 1;
		dt = out;
	} while(dt & 0x80);
	e = 0;
}

/*********** 写LCD指令 *********************************************************/
void write_command(uchar com)
{
	check_busy();
	e = 0;
	rs = 0;
	rw = 0;
	out = com;
	e = 1;
	_nop_();
	e = 0;
	delay(1);
}

/*********** 写LCD数据 *********************************************************/
void write_data(uchar dat)
{
	check_busy();
	e = 0;
	rs = 1;
	rw = 0;
	out = dat;
	e = 1;
	_nop_();
	e = 0;
	delay(1);
}

/*********** LCD初始化 *********************************************************/
void LCD_initial(void)
{
	write_command(0x38);
	write_command(0x0C);
	write_command(0x06);
	write_command(0x01);
	delay(1);
}

/*********** 输出字符串到LCD指定位置 *********************************************/
void string(uchar ad, uchar *s)
{
	write_command(ad);
	while(*s > 0)
	{
		write_data(*s++);
	}
}

/*********** DS18B20 读一个字节 *************************************************/
uchar readbyte(void)
{
	uchar i = 0;
	uchar date = 0;
	for(i = 8; i > 0; i--)
	{
		DQ = 0;
		delay5(1);
		DQ = 1;
		date >>= 1;
		if(DQ)
			date |= 0x80;
		delay5(11);
	}
	return(date);
}

/*********** DS18B20 初始化 *****************************************************/
void init_ds18b20(void)
{
	uchar x = 0;
	DQ = 0;
	delay5(120);
	DQ = 1;
	delay5(16);
	delay5(80);
}

/*********** DS18B20 写一个字节 *************************************************/
void writebyte(uchar dat)
{
	uchar i = 0;
	for(i = 8; i > 0; i--)
	{
		DQ = 0;
		DQ = dat & 0x01;
		delay5(12);
		DQ = 1;
		dat >>= 1;
		delay5(5);
	}
}

/*********** DS18B20 读取温度（返回10倍温度值）************************************/
uint retemp(void)
{
	uint tt;
	uchar a, b;
	uint t;
	init_ds18b20();
	writebyte(0xCC);      // 跳过 ROM
	writebyte(0x44);      // 启动温度转换
	delay(800);           // 等待转换完成（12位精度需~750ms）
	init_ds18b20();
	writebyte(0xCC);      // 跳过 ROM
	writebyte(0xBE);      // 读暂存器
	a = readbyte();       // 低字节
	b = readbyte();       // 高字节
	t = b;
	t <<= 8;
	t = t | a;
	tt = t * 0.625;
	return(tt);
}

/*********** 写DAC0832 *********************************************************/
void write_dac(uchar dat)
{
	e = 0;                // 关闭LCD使能，释放P0总线
	out = dat;            // 先放数据到P0总线
	DAC_CS = 0;           // 选中DAC0832
	_nop_();
	DAC_WR = 0;           // WR变低，进入透明模式
	_nop_();
	_nop_();
	DAC_WR = 1;           // WR上升沿锁存数据
	_nop_();
	_nop_();
	DAC_CS = 1;           // 释放DAC0832，交还P0总线给LCD
}

/*********** 按键扫描：调节设置温度 ***********************************************/
void key_scan(void)
{
	if(KEY_UP == 0)
	{
		delay(15);        // 消抖
		if(KEY_UP == 0)
		{
			if(set_temp < 500)    // 最高50.0℃
				set_temp += 10;   // 增加1.0℃
			while(KEY_UP == 0);  // 等待松开
		}
	}
	if(KEY_DOWN == 0)
	{
		delay(15);
		if(KEY_DOWN == 0)
		{
			if(set_temp > 50)     // 最低5.0℃
				set_temp -= 10;   // 减少1.0℃
			while(KEY_DOWN == 0);
		}
	}
}

/*********** LED亮度控制（根据温差）**********************************************/
#define MAX_DIFF  100   // 最大温差10.0℃（×10），对应最亮

void led_control(void)
{
	uint diff;
	uchar dac_val;

	if(temp < set_temp)                    // 当前温度低于设置温度：需要升温
	{
		diff = set_temp - temp;
		if(diff > MAX_DIFF)
			diff = MAX_DIFF;
		/* 温差越大，DAC输出越小→LED越亮 */
		dac_val = 255 - (uchar)(diff * 255 / MAX_DIFF);
	}
	else                                   // 当前温度≥设置温度：不需要升温
	{
		dac_val = 255;                     // DAC输出5V→LED熄灭
	}

	write_dac(dac_val);
}
