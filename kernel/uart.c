#include "types.h"
#include "defs.h"
#include "memlayout.h"
#include "spinlock.h"

// http://byterunner.com/16550.html
struct uart_16550a_regs {
  uint8 RBR_THR_DLL;  // 0x00 Receiver Buffer Register / Transmitter Holding
                      // Register / Divisor Latch LSB
  uint8 IER_DLM;      // 0x01 Interrupt EnableRegister / Divisor Latch MSB
  uint8 IIR_FCR;      // 0x02 Interrupt Status Register / FIFO Control Register
  uint8 LCR;          // 0x03 Line Control Register
  uint8 MCR;          // 0x04 Modem Control Register
  uint8 LSR;          // 0x05 LineStatus Register
  uint8 MSR;          // 0x06 Modem Status Register
  uint8 SPR;          // 0x07 ScratchPad Register
};

#define LCR_BAUD_LATCH (1 << 7)  // 修改波特率的模式
#define LCR_EIGHT_BITS (3 << 0)  // 修改字长是8 没有校验
#define LSR_TX_IDLE (1 << 5)     // THR可以接受其他字符了
#define IER_RX_ENABLE (1 << 0)   // rx 使能
#define IER_TX_ENABLE (1 << 1)   // tx 使能

#define FCR_FIFO_ENABLE (1 << 0)
#define FCR_FIFO_CLEAR (3 << 1)

// 根据内存映射 确定uart的寄存器地址
volatile static struct uart_16550a_regs *regs =
    (struct uart_16550a_regs *)UART0;

struct spinlock uart_tx_lock;
extern volatile int panicked;  // printf定义的 如果内核崩溃 所有输出都被禁止

void uartinit(void) {
  // 关中断
  regs->IER_DLM = 0;

  // 打开修改波特率的设置开关
  regs->LCR = (regs->LCR | LCR_BAUD_LATCH);

  // 写入DLL和DLM 存入数字0x003
  regs->RBR_THR_DLL = 0x03;
  regs->IER_DLM = 0x00;

  // 设置字长为8 和无校验 关闭设置波特率的开关
  // 注意这里是等于 不是|=
  regs->LCR = LCR_EIGHT_BITS;

  // 重设并开启FIFO
  regs->IIR_FCR = (FCR_FIFO_ENABLE | FCR_FIFO_CLEAR);

  // 打开rx和tx的中断
  regs->IER_DLM = (IER_TX_ENABLE | IER_RX_ENABLE);

  // 初始化锁
  initlock(&uart_tx_lock, "uart");
}

// 输出字符到设备 内核使用这个函数
void uartputc_sync(int c) {
  // 关中断
  push_off();
  // 崩溃时 所有CPU都死循环
  if (panicked) {
    for (;;)
      ;
  }

  while ((regs->LSR & LSR_TX_IDLE) == 0)
    ;
  regs->RBR_THR_DLL = c;

  // 开中断
  pop_off();
}