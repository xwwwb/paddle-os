#include <stdarg.h>
#include "types.h"
#include "riscv.h"
#include "defs.h"

#include "spinlock.h"
#include "proc.h"

// 标记内核是否崩溃
volatile int panicked = 0;

// 创建匿名结构体 并且上锁 避免并发printf
static struct {
  struct spinlock lock;
  int locking;
} pr;

static char digits[] = "0123456789abcdef";

// 打印int类型 传入参数为数字 基数 是否有符号
static void printint(int num, int base, int sign) {
  char buf[16];
  int i;   // 循环参数
  uint x;  // 具体的数字

  // 当是无符号数时 sign仍为0 因为不会走 && 后面的代码
  // 当是有符号数时 正数时sign为0 负数时sign为1
  if (sign && (sign = num < 0)) {
    // 有符号数 且是负数 x取绝对值 此时sign为1
    x = -num;
  } else {
    // 有符号数 正数 或者无符号数 此时sign为0
    x = num;
  }
  i = 0;
  // 将数字按位转字符串
  do {
    buf[i++] = digits[x % base];
  } while ((x /= base) != 0);

  // 如果是负数 需要输出负号
  if (sign) {
    buf[i++] = '-';
  }
  while (--i >= 0) {
    consputc(buf[i]);
  }
}

// 打印地址
static void printptr(uint64 x) {
  int i;
  consputc('0');
  consputc('x');

  // 8个字节的数字 可以用16个16进制数表示
  // 这个数字转字符串之后是16位的
  for (i = 0; i < sizeof(uint64) * 2; i++) {
    // 64位二进制数 右移60位可以得到最高4位
    consputc(digits[x >> (sizeof(uint64) * 8 - 4)]);
    // 数字左移4位 下次右移60的时候可以得到次高4位
    x <<= 4;
  }
}

// 格式化输出字符到控制台 kernel使用
void printf(char *fmt, ...) {
  // 这里要用到c语言的标准库的可变长参数了
  va_list ap;
  // 预定义循环中的变量
  int i, c, locking;  // i是循环参数 c是当前处理的字符 locking是锁
  char *s;            // s是字符串

  locking = pr.locking;
  if (locking) {
    // 获取锁
    acquire(&pr.lock);
  }

  if (fmt == 0) {
    panic("null fmt");
  }

  // ap是va_list类型 va_start传入可变参数的起始地址和ap
  va_start(ap, fmt);
  // 遍历字符串fmt 每次取一个字节 直到结束
  for (i = 0; (c = fmt[i] & 0xff) != 0; i++) {
    if (c != '%') {
      // 如果当前字符不是%说明是普通字符 直接输出
      consputc(c);
      continue;
    }
    // 取下一个字符
    c = fmt[++i] & 0xff;
    if (c == 0) {
      // 下一个字符是0 说明字符串结束了 当前字符串以%结尾
      // 不进行输出
      break;
    }
    // 使用va_arg依次取出参数
    // 只能处理%d %x %p %s %%
    switch (c) {
      case 'd':
        // 十进制
        printint(va_arg(ap, int), 10, 1);
        break;
      case 'x':
        // 16进制
        printint(va_arg(ap, int), 16, 1);
        break;
      case 'p':
        // 指针
        printptr(va_arg(ap, uint64));
        break;
      case 's':
        if ((s = va_arg(ap, char *)) == 0) {
          s = "(null)";
        }
        for (; *s; s++) {
          consputc(*s);
        }
        break;
      case '%':
        // 输出% C语言规定%%输出百分号本身
        consputc('%');
        break;
      default:
        // %后跟了不认识的
        consputc('%');
        consputc(c);
        break;
    }
  }
  va_end(ap);

  // 释放锁
  if (locking) {
    release(&pr.lock);
  }
}

// 此函数设计为 系统出错的时候执行 直接死机
void panic(char *s) {
  // 这里希望能够立即输出信息 关掉printf的锁要求
  pr.locking = 0;
  printf("panic: ");
  printf(s);
  printf("\n");
  panicked = 1;  // 禁止所有CPU的uart输出
  for (;;)
    ;
}

void printfinit(void) {
  initlock(&pr.lock, "printf");
  pr.locking = 1;
}