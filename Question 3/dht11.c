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

#include "dht11.h"
#include <intrins.h>
#include "Delay.h"

/**
 * @brief  DHT11 微秒级延时（约1us/次，@11.0592MHz）
 */
static void DHT11_Delay_us(unsigned char us)
{
    while (us--);
}

/**
 * @brief  从 DHT11 读取 1 字节数据
 */
static unsigned char DHT11_Read_Byte(void)
{
    unsigned char i, dat = 0;
    for (i = 0; i < 8; i++) {
        while (!DHT11_Data);    /* 等待低电平结束 */
        DHT11_Delay_us(15);     /* 跳过数据起始高电平 */
        dat <<= 1;
        if (DHT11_Data == 1) {
            dat |= 1;
            while (DHT11_Data); /* 等待高电平结束 */
        }
    }
    return dat;
}

/**
 * @brief  读取 DHT11 温湿度数据
 * @param  temp  输出温度值（整数部分）
 * @param  humi  输出湿度值（整数部分）
 * @retval 1 读取成功，0 校验失败
 */
unsigned char DHT11_Read_Data(unsigned char *temp, unsigned char *humi)
{
    unsigned char buffer[5] = {0};
    unsigned char i;

    /* 主机发送起始信号：拉低 20ms 再拉高 */
    DHT11_Data = 0;
    Delay(20);
    DHT11_Data = 1;
    DHT11_Delay_us(15);

    /* 等待从机应答 */
    if (DHT11_Data == 0) {
        while (DHT11_Data == 0);  /* 等待从机响应低电平结束 */
        while (DHT11_Data == 1);  /* 等待从机响应高电平结束 */

        /* 读取 5 字节数据 */
        for (i = 0; i < 5; i++) {
            buffer[i] = DHT11_Read_Byte();
        }

        /* 校验和验证 */
        if ((buffer[0] + buffer[1] + buffer[2] + buffer[3]) == buffer[4]) {
            *humi = buffer[0];    /* 湿度整数 */
            *temp = buffer[2];    /* 温度整数 */
            return 1;
        }
    }
    return 0;
}
