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
#include "LCD1602.h"
#include "key.h"
#include "DA.h"
#include "Delay.h"

/* ==================== 硬件接口定义 ==================== */
sbit HS1101_Pin = P3^4;         /* 555振荡器输出引脚（接 T0 计数器输入） */

/* ==================== 控制参数 ==================== */
#define KP  10                  /* 比例系数 */

/* ==================== 全局变量 ==================== */
bit    DataReady = 0;           /* 1秒测量完成标志 */
uint   Frequency = 0;           /* 1秒内脉冲计数（频率，Hz） */
uchar  CurrentHumidity = 0;     /* 当前湿度 (%RH, 0~99) */
uchar  TargetHumidity  = 50;    /* 目标湿度 (%RH, 0~99) */
int    Error;                   /* 湿度误差 */
int    DA_Output;               /* DAC 输出值 */

/* ==================== 初始化系统时钟 ==================== */

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
    static unsigned char tick = 0;

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
            DAC0832_Write((unsigned char)(155 - DA_Output));
        }
    }
}
