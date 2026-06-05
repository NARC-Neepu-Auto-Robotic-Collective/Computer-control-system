#ifndef __DHT11_H__
#define __DHT11_H__

#include <REGX52.H>

// --- 硬件引脚连接 ---
sbit DHT11_Data = P2^2; // 杜邦线将 DHT11 的 DATA 引脚连到单片机的 P2.0

// --- 函数声明 ---
unsigned char DHT11_Read_Data(unsigned char *temp, unsigned char *humi);

#endif