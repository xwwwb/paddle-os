// 初始化操作系统
#include "defs.h"

int main()
{
    // 初始化第一个CPU
    if (cpuid() == 0)
    {
        // 初始化控制台
        consoleinit();
    }
    else
    {
        while (1)
        {
        }
    }
}