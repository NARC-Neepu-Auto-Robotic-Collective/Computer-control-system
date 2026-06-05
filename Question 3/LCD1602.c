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

#include <REGX52.H>
#include "LCD1602.h"

/* ==================== 硬件接口定义 ==================== */
#define LCD_DataPort  P0

sbit LCD_RS = P3^0;
sbit LCD_RW = P3^1;
sbit LCD_EN = P3^2;

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief  LCD1602 内部延时（@11.0592MHz）
 */
static void LCD_Delay(void)
{
    unsigned char i = 2, j = 152;
    do {
        while (--j);
    } while (--i);
}

/**
 * @brief  写指令到 LCD1602
 */
static void LCD_WriteCommand(unsigned char Command)
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
static void LCD_WriteData(unsigned char Data)
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
static void LCD_SetCursor(unsigned char Line, unsigned char Column)
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
    unsigned char i;
    for (i = 0; i < Y; i++) {
        Result *= X;
    }
    return Result;
}

/* ==================== 公开接口函数 ==================== */

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
 * @brief  在指定位置显示一个字符
 */
void LCD_ShowChar(unsigned char Line, unsigned char Column, char Char)
{
    LCD_SetCursor(Line, Column);
    LCD_WriteData(Char);
}

/**
 * @brief  在指定位置显示字符串
 */
void LCD_ShowString(unsigned char Line, unsigned char Column, char *String)
{
    unsigned char i;
    LCD_SetCursor(Line, Column);
    for (i = 0; String[i] != '\0'; i++) {
        LCD_WriteData(String[i]);
    }
}

/**
 * @brief  在指定位置显示无符号数字
 */
void LCD_ShowNum(unsigned char Line, unsigned char Column, unsigned int Number, unsigned char Length)
{
    unsigned char i;
    LCD_SetCursor(Line, Column);
    for (i = Length; i > 0; i--) {
        LCD_WriteData(Number / LCD_Pow(10, i - 1) % 10 + '0');
    }
}

/**
 * @brief  在指定位置显示有符号数字（带正负号）
 */
void LCD_ShowSignedNum(unsigned char Line, unsigned char Column, int Number, unsigned char Length)
{
    unsigned int  Number1;
    unsigned char i;
    LCD_SetCursor(Line, Column);

    if (Number >= 0) {
        LCD_WriteData('+');
        Number1 = Number;
    } else {
        LCD_WriteData('-');
        Number1 = -Number;
    }

    for (i = Length; i > 0; i--) {
        LCD_WriteData(Number1 / LCD_Pow(10, i - 1) % 10 + '0');
    }
}

/**
 * @brief  在指定位置显示十六进制数字
 */
void LCD_ShowHexNum(unsigned char Line, unsigned char Column, unsigned int Number, unsigned char Length)
{
    unsigned char i, SingleNumber;
    LCD_SetCursor(Line, Column);
    for (i = Length; i > 0; i--) {
        SingleNumber = Number / LCD_Pow(16, i - 1) % 16;
        if (SingleNumber < 10) {
            LCD_WriteData(SingleNumber + '0');
        } else {
            LCD_WriteData(SingleNumber - 10 + 'A');
        }
    }
}

/**
 * @brief  在指定位置显示二进制数字
 */
void LCD_ShowBinNum(unsigned char Line, unsigned char Column, unsigned int Number, unsigned char Length)
{
    unsigned char i;
    LCD_SetCursor(Line, Column);
    for (i = Length; i > 0; i--) {
        LCD_WriteData(Number / LCD_Pow(2, i - 1) % 2 + '0');
    }
}
