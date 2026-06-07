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

#include "DA.h"
#include <intrins.h>

/**
 * @brief  向 DAC0832 写入 8 位数据并锁存输出
 * @param  Data  数字量 (0~255)，输出 0~5V 模拟电压
 *
 * DAC0832 采用直通方式（CS 和 WR 低电平有效触发写入）
 */
void DAC0832_Write(unsigned char Data)
{
    DAC_DataPort = Data;        /* 输出数据到总线 */
    DAC_CS = 0;                 /* 片选有效 */
    DAC_WR = 0;                 /* 写有效 */
    _nop_();                    /* 确保写入时序 */
    DAC_WR = 1;                 /* 写无效（锁存数据） */
    DAC_CS = 1;                 /* 片选无效 */
}
