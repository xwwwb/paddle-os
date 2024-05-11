// 控制台设备
#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "params.h"
#include "proc.h"
#include "fs.h"
#include "file.h"

// 退格键
#define BACKSPACE 0x100
#define C(x) ((x) - '@')  // Control-x 当这两个按键一同按下 发送一个信号

// 向控制台输出一个字符 只允许kernel使用
void consputc(int c) {
  if (c == BACKSPACE) {
    // 输出退格符然后用空格覆盖
    uartputc_sync('\b');
    uartputc_sync(' ');
    uartputc_sync('\b');
  } else {
    uartputc_sync(c);
  }
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
  int i;
  for (i = 0; i < n; i++) {
    char c;
    // 一次往c里面写一个字符
    if (either_copyin(&c, user_src, src + i, 1) == -1) {
      break;
    }
    uartputc(c);
  }
  return i;
}

// 用户使用read()从console读数据调用这
// user_dst决定目的是用户空间还是内核空间
int consoleread(int user_dst, uint64 dst, int n) {
  uint target;
  int c;
  char cbuf;

  target = n;
  acquire(&cons.lock);
  while (n > 0) {
    // 当读头和写头一样的时候 说明是空的
    // 等待输入中断写入
    while (cons.r == cons.w) {
      if (killed(myproc())) {
        release(&cons.lock);
        return -1;
      }
      // 睡在这里 consoleintr 会唤醒
      sleep(&cons.r, &cons.lock);
    }
    c = cons.buf[cons.r++ % INPUT_BUF_SIZE];

    // 如果字符是文件结束符
    if (c == C('D')) {
      if (n < target) {
        // 结束符先不读入
        cons.r--;
      }
      break;
    }
    cbuf = c;
    // 拷贝一个字符到用户空间
    if (either_copyout(user_dst, dst, &cbuf, 1) == -1) {
      break;
    }

    dst++;
    --n;

    if (c == '\n') {
      // 读完了一行
      break;
    }
  }
  release(&cons.lock);

  return target - n;
}

// console的输入中断处理程序
// uartintr会调用这里
void consoleintr(int c) {
  acquire(&cons.lock);
  switch (c) {
    case C('P'): {
      // 打印进程列表
      procdump();
      break;
    }
    case C('U'): {
      // 删除当前行
      // 写一行的退格
      while (cons.e != cons.w &&
             cons.buf[(cons.e - 1) % INPUT_BUF_SIZE] != '\n') {
        cons.e--;
        consputc(BACKSPACE);
      }
      break;
    }
    case C('H'):  // 退格
    case '\x7f':  // 删除
    {
      if (cons.e != cons.w) {
        // 编辑头自减
        cons.e--;
        // 写一个退格
        consputc(BACKSPACE);
      }
      break;
    }
    // 以上代码不涉及到控制台缓冲
    default: {
      // 当输入的字符有效 且 缓冲区有空闲
      if (c != 0 && cons.e - cons.r < INPUT_BUF_SIZE) {
        // 如果遇见了回档 也按换行来
        c = (c == '\r') ? '\n' : c;

        // 回显输入
        consputc(c);
        // 编辑头自增
        // 等待consoleread读入
        cons.buf[cons.e++ % INPUT_BUF_SIZE] = c;
        // 如果遇见换行或结束符 或者没有输入缓冲了
        if (c == '\n' || c == C('D') || cons.e - cons.r == INPUT_BUF_SIZE) {
          // 设置写头为编辑头
          cons.w = cons.e;
          // 唤醒consoleread
          wakeup(&cons.r);
        }
      }
      break;
    }
  }

  release(&cons.lock);
}

// 控制台硬件的初始化
void consoleinit(void) {
  initlock(&cons.lock, "cons");

  uartinit();

  devsw[CONSOLE].read = consoleread;
  devsw[CONSOLE].write = consolewrite;
}
