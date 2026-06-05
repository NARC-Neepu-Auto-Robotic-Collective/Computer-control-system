#include <REGX52.H>
#include "LCD1602.h"
#include "key.h"
#include "DA.h"

// ================= 接口定义 =================
sbit HS1101_Pin = P3^4; 

// ================= 全局变量 =================
unsigned char KeyNum = 0;       
unsigned char CurrentHumi = 0;   
unsigned char TargetHumi = 50;   

int Humi_Error = 0;              
int DA_Output = 0;               
unsigned char Kp = 10;           

extern void Delay(unsigned int ms);
int op = 0;
unsigned int fre = 0; 
void Sys_Init(void);
bit  Disflag=0;
// ================= 主函数 =================
void main(void)
{
    unsigned char temp0 = 0, temp1 = 0;
    LCD_Init();
    Sys_Init();
	TR0=1;								// 启动计数器0
	TR1=1;								// 启动定时器1
	IE=0x88;							// 打开定时中断1和总中断
    // 开机初始化静态框架
    LCD_ShowString(1, 1, "Now Humi: --%   "); 
    LCD_ShowString(2, 1, "Set Humi: "); 
    LCD_ShowNum(2, 11, TargetHumi, 2);
    LCD_ShowString(2, 13, "%   ");
//    while(1)
//    {
//	for(op = 100;op < 200;op+= 10)
//	{
//	 DAC0832_Write((unsigned char)255-op);
//Delay(100);
//	}

//	 }
		
    while(1)
    {
        // 1. 键盘扫描
        KeyNum = MatrixKey();       
        if(KeyNum != 0)
        {
            if(KeyNum <= 10) 
            {
                if(KeyNum == 10) KeyNum = 0; 
                TargetHumi = (TargetHumi % 10) * 10 + KeyNum; 
                if(TargetHumi > 99) TargetHumi = 99; 
            }
            if(KeyNum == 11) TargetHumi = 0;
            
            // 【局部清屏更新法】修改第二行
            LCD_ShowNum(2, 11, TargetHumi, 2); 
            LCD_ShowString(2, 13, "%   "); // 覆盖后面可能残留的乱码
        }

        // 2. 业务逻辑轮询
        Delay(1); 
        if(Disflag)	
        {
            Disflag=0;	 
			
            fre = fre - 100; // 软件校准补偿值
            // --- 重点修复：第一行更新逻辑 ---
            if((5623 <= fre) && (fre <= 6852)) 
            { 
                if((6734 < fre) && (fre <= 6852)) { temp0 = 0; temp1 = (6852 - fre) * 10 / 118; } 
                else if((6618 < fre) && (fre <= 6734)) { temp0 = 1; temp1 = (6734 - fre) * 10 / 116; } 
                else if((6503 < fre) && (fre <= 6618)) { temp0 = 2; temp1 = (6618 - fre) * 10 / 115; } 
                else if((6388 < fre) && (fre <= 6503)) { temp0 = 3; temp1 = (6503 - fre) * 10 / 115; } 
                else if((6271 < fre) && (fre <= 6388)) { temp0 = 4; temp1 = (6388 - fre) * 10 / 117; } 
                else if((6152 < fre) && (fre <= 6271)) { temp0 = 5; temp1 = (6271 - fre) * 10 / 119; } 
                else if((6029 < fre) && (fre <= 6152)) { temp0 = 6; temp1 = (6152 - fre) * 10 / 123; } 
                else if((5901 < fre) && (fre <= 6029)) { temp0 = 7; temp1 = (6029 - fre) * 10 / 128; } 
                else if((5766 < fre) && (fre <= 5901)) { temp0 = 8; temp1 = (5901 - fre) * 10 / 135; } 
                else if((5623 <= fre) && (fre <= 5766)) { temp0 = 9; temp1 = (5766 - fre) * 10 / 143; } 
                
                CurrentHumi = temp0 * 10 + temp1;
                
                // 【局部清屏更新法】无论数字变几位，行尾全部用空格推平
                LCD_ShowString(1, 1, "Now Humi: ");
                LCD_ShowNum(1, 11, CurrentHumi, 2); 
                LCD_ShowString(1, 13, "%   "); 
            } 
//            else 
//            { 
//                CurrentHumi = 0; 
//                // 直接整行字符串硬覆盖，连残影的根都拔掉
//                LCD_ShowString(1, 1, "Now Humi: EE%   "); 
//            } 

            // --- PID 负反馈输出 ---
            if(CurrentHumi > TargetHumi && CurrentHumi != 0)
            {
                Humi_Error = CurrentHumi - TargetHumi;
                DA_Output = 0.1 * Humi_Error * Kp;
                if(DA_Output > 100) DA_Output = 100; 
            }
            else
            {
                DA_Output = 0; 
            }
            DAC0832_Write((unsigned char)155-DA_Output);
        }
    }
}

void timer1() interrupt 3 
{
	static char j = 0;
	TH1=0x4C;						   // 重设定时器值，50ms @ 11.0592MHz XTAL
	TL1=0x00;
	if(++j == 20)					   // 50ms * 20 = 1S
	{			  
		j = 0;
		fre = (TH0 << 8) | TL0;		   // 1S内的计数值即为1秒内的输入频率
		TH0 = 0;					   // 清零计数
		TL0 = 0;
        Disflag=1;	 
	}
}


void Sys_Init(void)
{   
	TMOD=0x15;              // 定时器0工作于计数方式，工作方式1，16位计数
	                        // 定时器1工作于定时方式，工作方式1，16位定时
	TH0=0;					// 清零计数器
	TL0=0;
	TH1=0x4C;				// 12M晶振工作下，定时50ms
	TL1=0x00;
}