#ifndef __KEY_H__
#define __KEY_H__

#include <REGX52.H>

/**
  * @brief  矩阵键盘扫描函数
  * @param  无
  * @retval unsigned char 按下按键的键码值(1~16)，无按键按下返回0
  */
unsigned char MatrixKey();

#endif