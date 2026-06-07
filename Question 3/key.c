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

#include <REGX52.H>
#include "Delay.h"
#include "key.h"

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
unsigned char MatrixKey(void)
{
    unsigned char KeyNumber = 0;

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
