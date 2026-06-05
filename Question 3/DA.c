#include "DA.h"
#include <intrins.h> 

void DAC0832_Write(unsigned char Data)
{
    // 1. 将 8 位数字量直接送到 P3 口
    DAC_DataPort = Data; 
    
    // 2. 产生低电平脉冲，将数据锁存进 DAC0832
    DAC_CS = 0;          
    DAC_WR = 0;          
    _nop_();             // 短暂延时，确保数据写入成功
    DAC_WR = 1;          
    DAC_CS = 1;          
}