#include <stdarg.h>

#include "defs.h"
#include "spinlock.h"

// 标记内核是否崩溃
volatile int panicked = 0;

// 创建匿名结构体 并且上锁 避免并发printf
static struct {
  struct spinlock lock;
  int locking;
} pr;

// 格式化输出字符到控制台
void printf(char *fmt, ...) {
  // 这里要用到c语言的标准库的可变长参数了
  va_list ap;
  int i, c, locking;  // i是循环参数 c是当前处理的字符 locking是锁
  char *s;

  locking = pr.locking;
  if (locking) {
    // 获取锁
  }

  if (fmt == 0) {
    panic("null fmt");
  }
}

// 此函数设计为 系统出错的时候执行 直接死机
void panic(char *s) {
  // 这里希望能够立即输出信息 关掉printf的锁要求
  pr.locking = 0;
  printf("pannic: ");
  printf(s);
  printf("\n");
  panicked = 1;  // 禁止所有CPU的uart输出
  for (;;)
    ;
}

void printfinit(void) {
  initlock(&pr.lock, "pr");
  pr.locking = 1;
}