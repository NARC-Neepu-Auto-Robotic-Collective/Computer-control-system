/**
 * MIT License
 *
 * Copyright (c) 2026 xiaoshijourney
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
 * 题目2：机柜温度控制系统
 * 功能：DS18B20 采集温度 → LCD1602 显示 → 按键设定目标温度 → PID 控制步进电机（风扇）
 * 实验箱资源：温度传感模块、步进电机、LCD1602、矩阵按键
 * MCU: AT89S52, XTAL: 11.0592MHz
 ******************************************************************************/

#include <reg52.h>
#include <intrins.h>
#include <math.h>

/* ==================== 类型定义 ==================== */
#define uchar unsigned char
#define uint  unsigned int

/* ==================== 硬件接口定义 ==================== */
#define LCD_DATA    P0          /* LCD1602 数据口 (P0.0~P0.7) */
#define FAN_PORT    P2          /* 步进电机驱动口 (P2.0~P2.3) */

sbit LCD_RS  = P3^4;            /* LCD1602 寄存器选择 */
sbit LCD_RW  = P3^5;            /* LCD1602 读/写选择 */
sbit LCD_EN  = P3^6;            /* LCD1602 使能 */

sbit DQ      = P1^4;            /* DS18B20 单总线 */

sbit KEY_ADD  = P1^0;           /* 目标温度粗调+（+1°C） */
sbit KEY_SUB  = P1^1;           /* 目标温度粗调-（-1°C） */
sbit KEY_ADD1 = P1^2;           /* 目标温度细调+（+0.1°C） */
sbit KEY_SUB1 = P1^3;           /* 目标温度细调-（-0.1°C） */

/* ==================== PID 控制参数 ==================== */
#define KP  20                  /* 比例系数 */
#define KI  1                   /* 积分系数 */
#define KD  3                   /* 微分系数 */

/* ==================== 步进电机 8 拍驱动表 ==================== */
uchar code STEP_TABLE[] = {
    0x02, 0x06, 0x04, 0x0C, 0x08, 0x09, 0x01, 0x03
};

/* ==================== 全局变量 ==================== */
int    target_temp = 250;       /* 目标温度（25.0°C，放大10倍） */
int    error;                   /* 当前误差 */
int    last_error;              /* 上一次误差 */
int    integral;                /* 积分累加 */
int    derivative;              /* 微分 */
int    pid_output;              /* PID 输出值 */
uint   fan_speed  = 0;          /* 风扇转速 (0~100) */
uint   speed_counter = 0;       /* 转速控制计数器 */
uint   speed_threshold = 0;     /* 转速阈值 */
uchar  step_idx = 0;            /* 步进电机当前拍号 */

/* ==================== 函数前置声明 ==================== */
void  Delay(uint ms);
void  Delay5us(uchar n);
void  LCD_CheckBusy(void);
void  LCD_WriteCommand(uchar cmd);
void  LCD_WriteData(uchar dat);
void  LCD_Init(void);
void  LCD_ShowString(uchar addr, uchar *str);
void  Timer0_Init(void);
void  PID_Control(uint current_temp);
void  DS18B20_Init(void);
void  DS18B20_WriteByte(uchar dat);
uchar DS18B20_ReadByte(void);
uint  DS18B20_ReadTemp(void);

/* ==================== 延时函数 ==================== */

/**
 * @brief  毫秒级延时（约1ms/次，@11.0592MHz）
 * @param  ms  延时毫秒数
 */
void Delay(uint ms)
{
    uchar i;
    while (ms--) {
        i = 250;
        while (--i);
        i = 249;
        while (--i);
    }
}

/**
 * @brief  微秒级短延时（约5us/次，@11.0592MHz）
 */
void Delay5us(uchar n)
{
    while (n--) {
        _nop_();
        _nop_();
        _nop_();
    }
}

/* ==================== LCD1602 驱动 ==================== */

/**
 * @brief  检测 LCD1602 忙状态
 */
void LCD_CheckBusy(void)
{
    uchar status;
    do {
        status = 0xFF;
        LCD_EN = 0;
        LCD_RS = 0;             /* 指令寄存器 */
        LCD_RW = 1;             /* 读模式 */
        LCD_EN = 1;
        status = LCD_DATA;      /* 读取状态字 */
    } while (status & 0x80);   /* D7=1 表示忙 */
    LCD_EN = 0;
}

/**
 * @brief  向 LCD1602 写入指令
 */
void LCD_WriteCommand(uchar cmd)
{
    LCD_CheckBusy();
    LCD_EN = 0;
    LCD_RS = 0;                 /* 指令寄存器 */
    LCD_RW = 0;                 /* 写模式 */
    LCD_DATA = cmd;
    LCD_EN = 1;
    _nop_();
    LCD_EN = 0;
    Delay(1);
}

/**
 * @brief  向 LCD1602 写入数据
 */
void LCD_WriteData(uchar dat)
{
    LCD_CheckBusy();
    LCD_EN = 0;
    LCD_RS = 1;                 /* 数据寄存器 */
    LCD_RW = 0;                 /* 写模式 */
    LCD_DATA = dat;
    LCD_EN = 1;
    _nop_();
    LCD_EN = 0;
    Delay(1);
}

/**
 * @brief  初始化 LCD1602（8位总线、双行显示、5×7点阵）
 */
void LCD_Init(void)
{
    LCD_WriteCommand(0x38);     /* 8位总线，双行显示，5×7点阵 */
    LCD_WriteCommand(0x0C);     /* 显示开，光标关，不闪烁 */
    LCD_WriteCommand(0x06);     /* 写入后光标右移，屏幕不滚动 */
    LCD_WriteCommand(0x01);     /* 清屏 */
    Delay(1);
}

/**
 * @brief  在 LCD1602 指定地址开始显示字符串
 * @param  addr  DDRAM 地址（0x80+列 为第一行，0xC0+列 为第二行）
 * @param  str   要显示的字符串（以'\0'结尾）
 */
void LCD_ShowString(uchar addr, uchar *str)
{
    LCD_WriteCommand(addr);
    while (*str) {
        LCD_WriteData(*str++);
        Delay(1);
    }
}

/* ==================== DS18B20 驱动 ==================== */

/**
 * @brief  DS18B20 初始化（主机发送复位脉冲，等待从机应答）
 */
void DS18B20_Init(void)
{
    DQ = 0;                     /* 拉低总线 */
    Delay5us(120);              /* 保持 480~960us */
    DQ = 1;                     /* 释放总线 */
    Delay5us(16);               /* 等待 15~60us */
    Delay5us(80);               /* 等待从机应答完成 */
}

/**
 * @brief  向 DS18B20 写入 1 字节
 */
void DS18B20_WriteByte(uchar dat)
{
    uchar i;
    for (i = 8; i > 0; i--) {
        DQ = 0;
        DQ = dat & 0x01;        /* 写"1"：拉低15us后释放；写"0"：拉低60us */
        Delay5us(12);
        DQ = 1;
        dat >>= 1;
        Delay5us(5);
    }
}

/**
 * @brief  从 DS18B20 读取 1 字节
 */
uchar DS18B20_ReadByte(void)
{
    uchar i, dat = 0;
    for (i = 8; i > 0; i--) {
        DQ = 0;
        Delay5us(1);
        DQ = 1;                 /* 释放总线，等待从机输出数据 */
        dat >>= 1;
        if (DQ) dat |= 0x80;
        Delay5us(11);
    }
    return dat;
}

/**
 * @brief  读取 DS18B20 温度值（返回值 = 实际温度 × 10）
 */
uint DS18B20_ReadTemp(void)
{
    uint  raw, temp;
    uchar low_byte, high_byte;

    DS18B20_Init();
    DS18B20_WriteByte(0xCC);    /* 跳过 ROM 匹配 */
    DS18B20_WriteByte(0x44);    /* 启动温度转换 */

    DS18B20_Init();
    DS18B20_WriteByte(0xCC);    /* 跳过 ROM 匹配 */
    DS18B20_WriteByte(0xBE);    /* 读暂存器 */

    low_byte  = DS18B20_ReadByte();
    high_byte = DS18B20_ReadByte();

    raw = high_byte;
    raw <<= 8;
    raw |= low_byte;
    temp = raw * 0.625;         /* 分辨率 0.0625°C，放大10倍 */

    return temp;
}

/* ==================== 定时器0（步进电机 PWM 调速） ==================== */

/**
 * @brief  定时器0初始化（模式1，16位，约1ms中断）
 */
void Timer0_Init(void)
{
    TMOD |= 0x01;               /* 定时器0，模式1 */
    TH0   = 0xF8;               /* 初值，定时约1ms */
    TL0   = 0xCD;
    ET0   = 1;                  /* 允许定时器0中断 */
    EA    = 1;                  /* 开总中断 */
    TR0   = 1;                  /* 启动定时器0 */
}

/**
 * @brief  定时器0中断服务函数（步进电机速度控制）
 */
void Timer0_ISR(void) interrupt 1
{
    TH0 = 0xF8;
    TL0 = 0xCD;

    if (fan_speed > 0) {
        speed_counter++;
        speed_threshold = 100 / fan_speed + 1;

        if (speed_counter >= speed_threshold) {
            speed_counter = 0;
            step_idx = (step_idx < 7) ? (step_idx + 1) : 0;
            FAN_PORT = STEP_TABLE[step_idx];
        }
    } else {
        /* 转速为0，停止步进电机 */
        FAN_PORT = 0x00;
        speed_counter = 0;
    }
}

/* ==================== PID 控制算法 ==================== */

/**
 * @brief  PID 控制器（控制风扇转速）
 * @param  current_temp  当前温度（×10）
 *
 * 输出 fan_speed (0~100)，通过定时器中断控制步进电机转速
 * 当 current_temp <= target_temp + 2 时，停止风扇（避免过调）
 */
void PID_Control(uint current_temp)
{
    /* 温度足够低时停止风扇 */
    if (current_temp <= target_temp + 2) {
        fan_speed = 0;
        return;
    }

    error = current_temp - target_temp;

    /* 积分累加并限幅（防积分饱和） */
    integral += error;
    if (integral > 1000)  integral = 1000;
    if (integral < -1000) integral = -1000;

    /* 微分项（误差变化率） */
    derivative = error - last_error;
    last_error = error;

    /* PID 输出计算 */
    pid_output = 0.1 * KP * error + 0.03 * KI * integral + 0.1 * KD * derivative;

    /* 限幅到 0~100 */
    if (pid_output > 100)      fan_speed = 100;
    else if (pid_output < 0)   fan_speed = 0;
    else                       fan_speed = pid_output;
}

/* ==================== 主函数 ==================== */

void main(void)
{
    int   temp, pre_temp = 0;
    int   pre_target_temp = 0;
    uchar disp_buf[4];

    FAN_PORT = 0x03;            /* 步进电机初始相位 */
    LCD_Init();
    Timer0_Init();

    /* 显示初始目标温度 */
    disp_buf[3] = target_temp % 10 + '0';
    disp_buf[1] = target_temp / 10 % 10 + '0';
    disp_buf[0] = target_temp / 100 % 10 + '0';
    disp_buf[2] = '.';
    LCD_ShowString(0xC0, "Set:");
    LCD_ShowString(0xC4, disp_buf);

    while (1) {
        /* ---- 按键处理 ---- */
        if (!KEY_ADD1) {
            target_temp++;
            Delay(5);
        } else if (!KEY_SUB1) {
            target_temp--;
            Delay(5);
        } else if (!KEY_ADD) {
            target_temp += 10;
            Delay(5);
        } else if (!KEY_SUB) {
            target_temp -= 10;
            Delay(5);
        }

        /* 温度采集与PID控制 */
        temp = DS18B20_ReadTemp();
        PID_Control(temp);

        /* 当前温度变化时刷新 LCD 第一行 */
        if (temp != pre_temp) {
            pre_temp = temp;
            disp_buf[3] = temp % 10 + '0';
            disp_buf[1] = temp / 10 % 10 + '0';
            disp_buf[0] = temp / 100 % 10 + '0';
            disp_buf[2] = '.';
            LCD_ShowString(0x80, "Cur:");
            LCD_ShowString(0x84, disp_buf);
        }

        /* 目标温度变化时刷新 LCD 第二行 */
        if (target_temp != pre_target_temp) {
            pre_target_temp = target_temp;
            disp_buf[3] = target_temp % 10 + '0';
            disp_buf[1] = target_temp / 10 % 10 + '0';
            disp_buf[0] = target_temp / 100 % 10 + '0';
            disp_buf[2] = '.';
            LCD_ShowString(0xC0, "Set:");
            LCD_ShowString(0xC4, disp_buf);
        }

        Delay(5);
    }
}
