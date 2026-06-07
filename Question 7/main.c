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
 * 题目7：定速巡航小车
 * 功能：ADC0809 采集模拟电压（模拟车速）→ LCD1602 显示 → 按键设定目标速度
 *       → PID 控制步进电机转速（模拟车轮驱动）
 * 实验箱资源：手动调压模块、AD模块、步进电机、LCD1602、按键
 * MCU: AT89S52, XTAL: 11.0592MHz
 *
 * 硬件接线：
 *   ADC0809: DB0~DB7→P0, CS→P27, EOC→P34, WR→P36, RD→P37, CLK→ALE
 *   LCD1602: DB0~DB7→P1, RS→P20, RW→P21, EN→P22
 *   步进电机: 驱动口→P3 (低4位)
 *   按键: ADD→P23, SUB→P24
 ******************************************************************************/

#include <reg52.h>
#include <absacc.h>
#include <intrins.h>
#include <math.h>

/* ==================== 类型定义 ==================== */
#define uchar unsigned char
#define uint  unsigned int

/* ==================== 硬件接口定义 ==================== */
#define LCD_DATA    P1          /* LCD1602 数据口 (P1.0~P1.7) */
#define MOTOR_PORT  P3          /* 步进电机驱动口 (P3.0~P3.3) */

/* ADC0809 端口地址（P27 片选，A/B/C 通道选择接地→IN0） */
#define ADC0809     XBYTE[0x7FFF]

sbit LCD_RS  = P2^0;            /* LCD1602 寄存器选择 */
sbit LCD_RW  = P2^1;            /* LCD1602 读/写选择 */
sbit LCD_EN  = P2^2;            /* LCD1602 使能 */

sbit ADC_EOC = P3^4;            /* ADC0809 转换结束标志 */

sbit KEY_ADD = P2^3;            /* 目标速度+ */
sbit KEY_SUB = P2^4;            /* 目标速度- */

/* ==================== PID 控制参数 ==================== */
#define KP  5                   /* 比例系数 */
#define KI  0                   /* 积分系数 */
#define KD  0                   /* 微分系数 */

/* ==================== 步进电机 8 拍驱动表 ==================== */
uchar code STEP_TABLE[] = {
    0x02, 0x06, 0x04, 0x0C, 0x08, 0x09, 0x01, 0x03
};

/* ==================== 7段数码管段码表（共阳极） ==================== */
uchar code SEG_CODE[] = {
    0xC0, 0xF9, 0xA4, 0xB0,  /* 0, 1, 2, 3 */
    0x99, 0x92, 0x82, 0xF8,  /* 4, 5, 6, 7 */
    0x80, 0x90, 0xFF          /* 8, 9, off */
};

/* ==================== 全局变量 ==================== */
int   target_speed = 0;         /* 目标速度（0~500，对应ADC电压×100） */
int   error;                    /* 当前误差 */
int   last_error;               /* 上一次误差 */
int   integral;                 /* 积分累加 */
int   derivative;               /* 微分 */
int   pid_output;               /* PID 输出值 */
uint  motor_speed = 0;          /* 电机转速 (0~100) */
uint  speed_counter = 0;        /* 转速控制计数器 */
uint  speed_threshold = 0;      /* 转速阈值 */
uchar step_idx = 0;             /* 步进电机当前拍号 */

/* ==================== 函数前置声明 ==================== */
void  Delay(uint ms);
void  LCD_CheckBusy(void);
void  LCD_WriteCommand(uchar cmd);
void  LCD_WriteData(uchar dat);
void  LCD_Init(void);
void  LCD_ShowString(uchar addr, uchar *str);
void  Timer0_Init(void);
void  PID_Control(uint current_speed);
uint  ADC0809_Read(void);

/* ==================== 延时函数 ==================== */

/**
 * @brief  毫秒级延时（约1ms/次，@11.0592MHz）
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
        LCD_RS = 0;
        LCD_RW = 1;
        LCD_EN = 1;
        status = LCD_DATA;
    } while (status & 0x80);
    LCD_EN = 0;
}

/**
 * @brief  向 LCD1602 写入指令
 */
void LCD_WriteCommand(uchar cmd)
{
    LCD_CheckBusy();
    LCD_EN = 0;
    LCD_RS = 0;
    LCD_RW = 0;
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
    LCD_RS = 1;
    LCD_RW = 0;
    LCD_DATA = dat;
    LCD_EN = 1;
    _nop_();
    LCD_EN = 0;
    Delay(1);
}

/**
 * @brief  初始化 LCD1602
 */
void LCD_Init(void)
{
    LCD_WriteCommand(0x38);     /* 8位总线，双行显示，5×7点阵 */
    LCD_WriteCommand(0x0C);     /* 显示开，光标关 */
    LCD_WriteCommand(0x06);     /* 写入后光标右移 */
    LCD_WriteCommand(0x01);     /* 清屏 */
    Delay(1);
}

/**
 * @brief  在 LCD1602 指定地址开始显示字符串
 */
void LCD_ShowString(uchar addr, uchar *str)
{
    LCD_WriteCommand(addr);
    while (*str) {
        LCD_WriteData(*str++);
        Delay(1);
    }
}

/* ==================== ADC0809 驱动 ==================== */

/**
 * @brief  读取 ADC0809 通道0 的转换结果
 * @return 8位转换值（0~255），对应 0~5V
 */
uint ADC0809_Read(void)
{
    uchar val;
    ADC0809 = 0x0F;             /* 启动转换（任意写入） */
    while (!ADC_EOC);           /* 等待转换完成 */
    val = ADC0809;              /* 读取结果 */
    return val;
}

/* ==================== 定时器0（步进电机 PWM 调速） ==================== */

/**
 * @brief  定时器0初始化（模式1，16位，约1ms中断）
 */
void Timer0_Init(void)
{
    TMOD |= 0x01;
    TH0   = 0xF8;
    TL0   = 0xCD;
    ET0   = 1;
    EA    = 1;
    TR0   = 1;
}

/**
 * @brief  定时器0中断服务函数（步进电机速度控制）
 */
void Timer0_ISR(void) interrupt 1
{
    TH0 = 0xF8;
    TL0 = 0xCD;

    if (motor_speed > 0) {
        speed_counter++;
        speed_threshold = 100 / motor_speed + 1;

        if (speed_counter >= speed_threshold) {
            speed_counter = 0;
            step_idx = (step_idx < 7) ? (step_idx + 1) : 0;
            MOTOR_PORT = (MOTOR_PORT & 0xF0) | STEP_TABLE[step_idx];
        }
    } else {
        /* 停止电机 */
        MOTOR_PORT = (MOTOR_PORT & 0xF0) | 0x00;
        speed_counter = 0;
    }
}

/* ==================== PID 控制算法 ==================== */

/**
 * @brief  PID 控制器（控制电机转速）
 * @param  current_speed  当前速度值（0~500，ADC电压×100）
 *
 * 输出 motor_speed (0~100)，通过定时器中断控制步进电机转速
 */
void PID_Control(uint current_speed)
{
    error = abs(current_speed - target_speed);

    integral += error;
    if (integral > 1000)  integral = 1000;
    if (integral < -1000) integral = -1000;

    derivative = error - last_error;
    last_error = error;

    pid_output = 0.1 * KP * error + 0.03 * KI * integral + 0.1 * KD * derivative;

    if (pid_output > 100)      motor_speed = 100;
    else if (pid_output < 0)   motor_speed = 0;
    else                       motor_speed = pid_output;
}

/* ==================== 主函数 ==================== */

void main(void)
{
    uint  speed_val, prev_speed = 0;
    uchar disp_buf[4];

    MOTOR_PORT = (MOTOR_PORT & 0xF0) | 0x03;  /* 步进电机初始相位 */
    LCD_Init();
    Timer0_Init();

    while (1) {
        /* ---- 读取当前速度 ---- */
        speed_val = ADC0809_Read();
        /* 转换为 0~500 范围（0~5V 映射到 0~500，放大100倍便于显示） */
        speed_val = speed_val * 1.0 / 255 * 500;

        /* ---- 速度变化时刷新 LCD 第一行 ---- */
        if (speed_val != prev_speed) {
            prev_speed = speed_val;
            disp_buf[3] = speed_val % 10 + '0';
            disp_buf[2] = speed_val / 10 % 10 + '0';
            disp_buf[1] = speed_val / 100 % 10 + '0';
            disp_buf[0] = speed_val / 1000 + '0';
            LCD_ShowString(0x84, disp_buf);
        }

        /* ---- PID 控制 ---- */
        PID_Control(speed_val);

        /* ---- 按键处理（调整目标速度） ---- */
        if (!KEY_ADD) {
            target_speed += 100;
            disp_buf[3] = target_speed % 10 + '0';
            disp_buf[2] = target_speed / 10 % 10 + '0';
            disp_buf[1] = target_speed / 100 % 10 + '0';
            disp_buf[0] = target_speed / 1000 + '0';
            LCD_ShowString(0xC4, disp_buf);
        } else if (!KEY_SUB) {
            target_speed -= 100;
            disp_buf[3] = target_speed % 10 + '0';
            disp_buf[2] = target_speed / 10 % 10 + '0';
            disp_buf[1] = target_speed / 100 % 10 + '0';
            disp_buf[0] = target_speed / 1000 + '0';
            LCD_ShowString(0xC4, disp_buf);
        }
    }
}
