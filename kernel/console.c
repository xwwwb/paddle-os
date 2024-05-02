// 控制台
#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "file.h"

#include "spinlock.h"
#include "proc.h"

// 退格键
#define BACKSPACE 0x100

// 向控制台输出一个字符 只允许kernel使用
void consputc(int c) {
  if (c == BACKSPACE) {
    // 输出退格符然后用空格覆盖
    uartputc_sync('\b');
    uartputc_sync(' ');
    uartputc_sync('\b');
  } else
    uartputc_sync(c);
}

#define INPUT_BUF_SIZE 128

// 匿名结构体
struct {
  struct spinlock lock;
  char buf[INPUT_BUF_SIZE];
  uint r;  // 读索引
  uint w;  // 写索引
  uint e;  // 编辑索引
} cons;

// 用户使用write()函数写console会调用这里
int consolewrite(int user_src, uint64 src, int n) {
  // 涉及到了进程 先放一放
  return 0;
}

// 用户使用read()从console读数据调用这
int consoleread(int user_dst, uint64 dst, int n) {
  // 涉及到了进程 先放一放
  return 0;
}

// 控制台硬件的初始化
void consoleinit(void) {
  initlock(&cons.lock, "cons");

  uartinit();

  devsw[CONSOLE].read = consoleread;
  devsw[CONSOLE].write = consolewrite;
}
