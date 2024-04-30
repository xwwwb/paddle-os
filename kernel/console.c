// 控制台
#include "types.h"
#include "spinlock.h"
#include "defs.h"
#define INPUT_BUF_SIZE 128

// 匿名结构体
struct
{
    struct spinlock lock;
    char buf[INPUT_BUF_SIZE];
    uint r; // 读索引
    uint w; // 写索引
    uint e; // 编辑索引
} cons;

// 控制台硬件的初始化
void consoleinit(void)
{
    initlock(&cons.lock, "cons");

    uartinit();

    // // connect read and write system calls
    // // to consoleread and consolewrite.
    // devsw[CONSOLE].read = consoleread;
    // devsw[CONSOLE].write = consolewrite;
}
