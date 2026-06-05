#include "dht11.h"
#include <intrins.h>
#include "Delay.h"

// 11.0592MHz 的微秒延时保持不动，因为 DHT11 读数据需要微秒级微调
void DHT11_Delay_us(unsigned char us)
{
    while(us--); 
}

unsigned char DHT11_Read_Byte()
{
    unsigned char i, dat = 0;
    for(i=0; i<8; i++)
    {
        while(!DHT11_Data); 
        DHT11_Delay_us(15); // 11.0592MHz 判定高电平的关键
        dat <<= 1;
        if(DHT11_Data == 1)
        {
            dat |= 1;
            while(DHT11_Data);
        }
    }
    return dat;
}

unsigned char DHT11_Read_Data(unsigned char *temp, unsigned char *humi)
{
    unsigned char buffer[5] = {0};
    unsigned char i;
    
    DHT11_Data = 0;
    Delay(20);          // 直接调用你的一毫秒延时函数，延时 20ms，非常安全标准！
    DHT11_Data = 1;
    DHT11_Delay_us(15); 
    
    if(DHT11_Data == 0)
    {
        while(DHT11_Data == 0); 
        while(DHT11_Data == 1); 
        
        for(i=0; i<5; i++)
        {
            buffer[i] = DHT11_Read_Byte();
        }
        
        if((buffer[0] + buffer[1] + buffer[2] + buffer[3]) == buffer[4])
        {
            *humi = buffer[0];
            *temp = buffer[2];
            return 1;
        }
    }
    return 0;
}