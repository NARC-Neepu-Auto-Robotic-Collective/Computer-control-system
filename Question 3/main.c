/**
 * MIT License
 *
 * Copyright (c) 2026 HonorLiu0613 xiaoshijourney
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/******************************************************************************
 * 题目3：烤箱湿度控制系统
 * 功能：HS1101 频率法采集湿度 → LCD1602 显示 → 矩阵键盘设定目标湿度
 *       → 比例控制 DAC0832→LED 亮度调节（模拟烘烤灯）
 * 实验箱资源：湿度传感模块（HS1101 + 555振荡器）、DA模块、LED模块、LCD1602、矩阵键盘
 * MCU: AT89S52, XTAL: 11.0592MHz
 *
 * 湿度测量原理：HS1101 电容值随湿度变化 → 555振荡器频率变化
 * 定时器0 对外部脉冲计数（1s窗口），定时器1 提供 50ms 时基
 * 频率范围约 5623~6852 Hz，映射到湿度 0~99%RH
 ******************************************************************************/

#include <REGX52.H>
#include <intrins.h>

/* ==================== 类型定义 ==================== */
#define uchar unsigned char
#define uint  unsigned int

/* ==================== 硬件接口定义 ==================== */
/* LCD1602 (8位并行, P0口) */
#define LCD_DataPort  P0
sbit LCD_RS = P3^0;
sbit LCD_RW = P3^1;
sbit LCD_EN = P3^2;

/* DAC0832 (P2口) */
#define DAC_DataPort  P2
sbit DAC_CS = P3^6;
sbit DAC_WR = P3^5;

/* HS1101 555振荡器频率输入 (T0计数器引脚) */
sbit HS1101_Pin = P3^4;

/* ==================== 控制参数 ==================== */
#define KP  10                  /* 比例系数 */

/* ==================== 全局变量 ==================== */
bit    DataReady = 0;           /* 1秒测量完成标志 */
uint   Frequency = 0;           /* 1秒内脉冲计数（频率，Hz） */
uchar  CurrentHumidity = 0;     /* 当前湿度 (%RH, 0~99) */
uchar  TargetHumidity  = 50;    /* 目标湿度 (%RH, 0~99) */
int    Error;                   /* 湿度误差 */
int    DA_Output;               /* DAC 输出值 */

/* ==================== 函数声明 ==================== */
/* 延时 */
void Delay(uint xms);

/* LCD1602 内部驱动 */
static void LCD_Delay(void);
static void LCD_WriteCommand(uchar Command);
static void LCD_WriteData(uchar Data);
static void LCD_SetCursor(uchar Line, uchar Column);
static int  LCD_Pow(int X, int Y);

/* LCD1602 公开函数 */
void LCD_Init(void);
void LCD_ShowString(uchar Line, uchar Column, char *String);
void LCD_ShowNum(uchar Line, uchar Column, uint Number, uchar Length);

/* 矩阵键盘 */
uchar MatrixKey(void);

/* DAC0832 */
void DAC0832_Write(uchar Data);

/* 系统与算法 */
void  Sys_Init(void);
uchar HumidityFromFreq(uint freq);

/* ==================== 毫秒级延时（@11.0592MHz） ==================== */

/**
 * @brief  毫秒级延时
 * @param  xms  延时毫秒数
 */
void Delay(uint xms)
{
    uchar i, j;
    while (xms--) {
        i = 2;
        j = 152;
        do {
            while (--j);
        } while (--i);
    }
}

/* ==================== LCD1602 内部驱动函数 ==================== */

/**
 * @brief  LCD1602 内部短延时（@11.0592MHz）
 */
static void LCD_Delay(void)
{
    uchar i = 2, j = 152;
    do {
        while (--j);
    } while (--i);
}

/**
 * @brief  写指令到 LCD1602
 */
static void LCD_WriteCommand(uchar Command)
{
    LCD_RS = 0;
    LCD_RW = 0;
    LCD_DataPort = Command;
    LCD_EN = 1;
    LCD_Delay();
    LCD_EN = 0;
    LCD_Delay();
}

/**
 * @brief  写数据到 LCD1602
 */
static void LCD_WriteData(uchar Data)
{
    LCD_RS = 1;
    LCD_RW = 0;
    LCD_DataPort = Data;
    LCD_EN = 1;
    LCD_Delay();
    LCD_EN = 0;
    LCD_Delay();
}

/**
 * @brief  设置光标位置
 * @param  Line   行号 (1~2)
 * @param  Column 列号 (1~16)
 */
static void LCD_SetCursor(uchar Line, uchar Column)
{
    if (Line == 1) {
        LCD_WriteCommand(0x80 | (Column - 1));
    } else if (Line == 2) {
        LCD_WriteCommand(0x80 | (Column - 1 + 0x40));
    }
}

/**
 * @brief  计算 X 的 Y 次方
 */
static int LCD_Pow(int X, int Y)
{
    int Result = 1;
    uchar i;
    for (i = 0; i < Y; i++) {
        Result *= X;
    }
    return Result;
}

/* ==================== LCD1602 公开函数 ==================== */

/**
 * @brief  初始化 LCD1602（8位总线、双行显示、5×7点阵）
 */
void LCD_Init(void)
{
    LCD_WriteCommand(0x38);     /* 8位数据接口，双行显示，5×7点阵 */
    LCD_WriteCommand(0x0C);     /* 显示开，光标关，不闪烁 */
    LCD_WriteCommand(0x06);     /* 写入后光标右移，屏幕不滚动 */
    LCD_WriteCommand(0x01);     /* 光标复位，清屏 */
}

/**
 * @brief  在指定位置显示字符串
 */
void LCD_ShowString(uchar Line, uchar Column, char *String)
{
    uchar i;
    LCD_SetCursor(Line, Column);
    for (i = 0; String[i] != '\0'; i++) {
        LCD_WriteData(String[i]);
    }
}

/**
 * @brief  在指定位置显示无符号数字
 */
void LCD_ShowNum(uchar Line, uchar Column, uint Number, uchar Length)
{
    uchar i;
    LCD_SetCursor(Line, Column);
    for (i = Length; i > 0; i--) {
        LCD_WriteData(Number / LCD_Pow(10, i - 1) % 10 + '0');
    }
}

/* ==================== 4×4 矩阵键盘 ==================== */

/**
 * @brief  4×4 矩阵键盘扫描函数
 * @retval 按下的按键编号 (1~16)，无按键按下返回 0
 *
 * 接线：行线 P1.0~P1.3（输出），列线 P1.4~P1.7（输入）
 *
 * 按键布局：
 *   +-----+-----+-----+-----+
 *   |  1  |  2  |  3  |  4  |
 *   +-----+-----+-----+-----+
 *   |  5  |  6  |  7  |  8  |
 *   +-----+-----+-----+-----+
 *   |  9  | 10  | 11  | 12  |
 *   +-----+-----+-----+-----+
 *   | 13  | 14  | 15  | 16  |
 *   +-----+-----+-----+-----+
 */
uchar MatrixKey(void)
{
    uchar KeyNumber = 0;

    /* 扫描第1行（P1.0=0） */
    P1 = 0xFF;  P1_0 = 0;
    if (P1_4 == 0) { Delay(20); while (P1_4 == 0); Delay(20); KeyNumber = 1;  }
    if (P1_5 == 0) { Delay(20); while (P1_5 == 0); Delay(20); KeyNumber = 2;  }
    if (P1_6 == 0) { Delay(20); while (P1_6 == 0); Delay(20); KeyNumber = 3;  }
    if (P1_7 == 0) { Delay(20); while (P1_7 == 0); Delay(20); KeyNumber = 4;  }

    /* 扫描第2行（P1.1=0） */
    P1 = 0xFF;  P1_1 = 0;
    if (P1_4 == 0) { Delay(20); while (P1_4 == 0); Delay(20); KeyNumber = 5;  }
    if (P1_5 == 0) { Delay(20); while (P1_5 == 0); Delay(20); KeyNumber = 6;  }
    if (P1_6 == 0) { Delay(20); while (P1_6 == 0); Delay(20); KeyNumber = 7;  }
    if (P1_7 == 0) { Delay(20); while (P1_7 == 0); Delay(20); KeyNumber = 8;  }

    /* 扫描第3行（P1.2=0） */
    P1 = 0xFF;  P1_2 = 0;
    if (P1_4 == 0) { Delay(20); while (P1_4 == 0); Delay(20); KeyNumber = 9;  }
    if (P1_5 == 0) { Delay(20); while (P1_5 == 0); Delay(20); KeyNumber = 10; }
    if (P1_6 == 0) { Delay(20); while (P1_6 == 0); Delay(20); KeyNumber = 11; }
    if (P1_7 == 0) { Delay(20); while (P1_7 == 0); Delay(20); KeyNumber = 12; }

    /* 扫描第4行（P1.3=0） */
    P1 = 0xFF;  P1_3 = 0;
    if (P1_4 == 0) { Delay(20); while (P1_4 == 0); Delay(20); KeyNumber = 13; }
    if (P1_5 == 0) { Delay(20); while (P1_5 == 0); Delay(20); KeyNumber = 14; }
    if (P1_6 == 0) { Delay(20); while (P1_6 == 0); Delay(20); KeyNumber = 15; }
    if (P1_7 == 0) { Delay(20); while (P1_7 == 0); Delay(20); KeyNumber = 16; }

    return KeyNumber;
}

/* ==================== DAC0832 驱动 ==================== */

/**
 * @brief  向 DAC0832 写入 8 位数据并锁存输出
 * @param  Data  数字量 (0~255)，输出 0~5V 模拟电压
 *
 * DAC0832 采用直通方式（CS 和 WR 低电平有效触发写入）
 */
void DAC0832_Write(uchar Data)
{
    DAC_DataPort = Data;        /* 输出数据到总线 */
    DAC_CS = 0;                 /* 片选有效 */
    DAC_WR = 0;                 /* 写有效 */
    _nop_();                    /* 确保写入时序 */
    DAC_WR = 1;                 /* 写无效（锁存数据） */
    DAC_CS = 1;                 /* 片选无效 */
}

/* ==================== 系统初始化 ==================== */

/**
 * @brief  系统时钟初始化
 *         定时器0：计数器模式1（16位），对外部脉冲计数（T0引脚）
 *         定时器1：定时器模式1（16位），50ms 时基
 */
void Sys_Init(void)
{
    TMOD = 0x15;                /* T0: 计数器模式1(16位)，T1: 定时器模式1(16位) */
    TH0  = 0;
    TL0  = 0;
    TH1  = 0x4C;                /* 50ms @ 12MHz (实际11.0592MHz，略有偏差) */
    TL1  = 0x00;
    TR0  = 1;                   /* 启动计数器0 */
    TR1  = 1;                   /* 启动定时器1 */
    IE   = 0x88;                /* 开总中断 + 定时器1中断 */
}

/* ==================== 定时器1中断服务函数 ==================== */

/**
 * @brief  定时器1 中断（50ms × 20 = 1s 测量窗口）
 *
 * 每1秒读取一次 T0 计数值作为频率，然后清零计数器
 */
void Timer1_ISR(void) interrupt 3
{
    static uchar tick = 0;

    TH1 = 0x4C;
    TL1 = 0x00;

    if (++tick >= 20) {         /* 50ms × 20 = 1000ms */
        tick = 0;
        Frequency = (TH0 << 8) | TL0;  /* 读取1秒内脉冲计数值 */
        TH0 = 0;                       /* 清零计数器 */
        TL0 = 0;
        DataReady = 1;                 /* 通知主循环有新数据 */
    }
}

/* ==================== 湿度频率换算 ==================== */

/**
 * @brief  将 HS1101 频率值转换为湿度值
 * @param  freq  555振荡器输出频率（Hz）
 * @retval 湿度值 (%RH, 0~99)
 *
 * 频率-湿度对照表（经实验标定，减去基准偏移100Hz）：
 *   90%~99%: 5623~5766 Hz
 *   80%~89%: 5766~5901 Hz
 *   ...
 *    0%~ 9%: 6734~6852 Hz
 */
uchar HumidityFromFreq(uint freq)
{
    uchar tens = 0, ones = 0;

    freq -= 100;                /* 基准偏移校准 */

    if (freq < 5623 || freq > 6852) {
        return 0;               /* 超出有效范围 */
    }

    if      (freq > 6734 && freq <= 6852) { tens = 0; ones = (6852 - freq) * 10 / 118; }
    else if (freq > 6618 && freq <= 6734) { tens = 1; ones = (6734 - freq) * 10 / 116; }
    else if (freq > 6503 && freq <= 6618) { tens = 2; ones = (6618 - freq) * 10 / 115; }
    else if (freq > 6388 && freq <= 6503) { tens = 3; ones = (6503 - freq) * 10 / 115; }
    else if (freq > 6271 && freq <= 6388) { tens = 4; ones = (6388 - freq) * 10 / 117; }
    else if (freq > 6152 && freq <= 6271) { tens = 5; ones = (6271 - freq) * 10 / 119; }
    else if (freq > 6029 && freq <= 6152) { tens = 6; ones = (6152 - freq) * 10 / 123; }
    else if (freq > 5901 && freq <= 6029) { tens = 7; ones = (6029 - freq) * 10 / 128; }
    else if (freq > 5766 && freq <= 5901) { tens = 8; ones = (5901 - freq) * 10 / 135; }
    else if (freq >= 5623 && freq <= 5766) { tens = 9; ones = (5766 - freq) * 10 / 143; }

    return tens * 10 + ones;
}

/* ==================== 主函数 ==================== */

void main(void)
{
    uchar KeyNum;

    LCD_Init();
    Sys_Init();

    /* 显示静态标签 */
    LCD_ShowString(1, 1, "Now Humi: --%   ");
    LCD_ShowString(2, 1, "Set Humi: ");
    LCD_ShowNum(2, 11, TargetHumidity, 2);
    LCD_ShowString(2, 13, "%   ");

    while (1) {
        /* ---- 按键扫描 ---- */
        KeyNum = MatrixKey();
        if (KeyNum != 0) {
            if (KeyNum <= 10) {
                if (KeyNum == 10) KeyNum = 0;   /* 按键10 映射为数字0 */
                TargetHumidity = (TargetHumidity % 10) * 10 + KeyNum;
                if (TargetHumidity > 99) TargetHumidity = 99;
            }
            if (KeyNum == 11) TargetHumidity = 0;

            /* 刷新 LCD 第二行（目标湿度） */
            LCD_ShowNum(2, 11, TargetHumidity, 2);
            LCD_ShowString(2, 13, "%   ");
        }

        /* ---- 湿度采集与显示（1秒更新一次） ---- */
        Delay(1);
        if (DataReady) {
            DataReady = 0;

            CurrentHumidity = HumidityFromFreq(Frequency);

            /* 刷新 LCD 第一行（当前湿度） */
            LCD_ShowString(1, 1, "Now Humi: ");
            LCD_ShowNum(1, 11, CurrentHumidity, 2);
            LCD_ShowString(1, 13, "%   ");

            /* ---- 比例控制算法 ---- */
            if (CurrentHumidity > TargetHumidity && CurrentHumidity != 0) {
                Error     = CurrentHumidity - TargetHumidity;
                DA_Output = (int)(0.1 * Error * KP);
                if (DA_Output > 100) DA_Output = 100;
            } else {
                DA_Output = 0;
            }

            /* DAC0832 输出控制 LED 亮度（低于阈值则关闭LED） */
            DAC0832_Write((uchar)(155 - DA_Output));
        }
    }
}
