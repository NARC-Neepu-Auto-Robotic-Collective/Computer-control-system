// 11.0592MHz 专用 精确毫秒延时
void Delay(unsigned int xms)
{
    unsigned char data i, j;
    while(xms--)
    {
        i = 2;
        j = 152;   // 只改这里 239 → 152
        do
        {
            while (--j);
        } while (--i);
    }
}