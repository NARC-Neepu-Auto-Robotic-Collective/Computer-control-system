#include <REGX52.H>
#include "Delay.h"

/**
  * @brief  矩阵键盘扫描函数
  * @retval KeyNumber 按下按键的编号 (1~16)，无按键按下返回0
  */
unsigned char MatrixKey()
{
    unsigned char KeyNumber = 0;

    P1 = 0xFF; P1_0 = 0; // 扫描第一行
    if(P1_4 == 0){Delay(20); while(P1_4 == 0); Delay(20); KeyNumber = 1;}
    if(P1_5 == 0){Delay(20); while(P1_5 == 0); Delay(20); KeyNumber = 2;}
    if(P1_6 == 0){Delay(20); while(P1_6 == 0); Delay(20); KeyNumber = 3;}
    if(P1_7 == 0){Delay(20); while(P1_7 == 0); Delay(20); KeyNumber = 4;}

    P1 = 0xFF; P1_1 = 0; // 扫描第二行
    if(P1_4 == 0){Delay(20); while(P1_4 == 0); Delay(20); KeyNumber = 5;}
    if(P1_5 == 0){Delay(20); while(P1_5 == 0); Delay(20); KeyNumber = 6;}
    if(P1_6 == 0){Delay(20); while(P1_6 == 0); Delay(20); KeyNumber = 7;}
    if(P1_7 == 0){Delay(20); while(P1_7 == 0); Delay(20); KeyNumber = 8;}

    P1 = 0xFF; P1_2 = 0; // 扫描第三行
    if(P1_4 == 0){Delay(20); while(P1_4 == 0); Delay(20); KeyNumber = 9;}
    if(P1_5 == 0){Delay(20); while(P1_5 == 0); Delay(20); KeyNumber = 10;}
    if(P1_6 == 0){Delay(20); while(P1_6 == 0); Delay(20); KeyNumber = 11;}
    if(P1_7 == 0){Delay(20); while(P1_7 == 0); Delay(20); KeyNumber = 12;}

    P1 = 0xFF; P1_3 = 0; // 扫描第四行
    if(P1_4 == 0){Delay(20); while(P1_4 == 0); Delay(20); KeyNumber = 13;}
    if(P1_5 == 0){Delay(20); while(P1_5 == 0); Delay(20); KeyNumber = 14;}
    if(P1_6 == 0){Delay(20); while(P1_6 == 0); Delay(20); KeyNumber = 15;}
    if(P1_7 == 0){Delay(20); while(P1_7 == 0); Delay(20); KeyNumber = 16;}

    return KeyNumber;
}