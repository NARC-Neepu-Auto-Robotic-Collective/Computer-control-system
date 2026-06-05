#ifndef __DA_H__
#define __DA_H__

#include <REGX52.H>

// --- 接口定义（坚守 P3 口方案） ---
#define DAC_DataPort P2 // DA 数据线独占 P3 端口 (P3.0 ~ P3.7)

sbit DAC_CS = P3^6;     // DA 片选信号接入 P2.3
sbit DAC_WR = P3^5;     // DA 写控制信号接入 P2.4

// --- 函数声明 ---
void DAC0832_Write(unsigned char Data);

#endif