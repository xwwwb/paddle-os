#include "includes/types.h"
#include "includes/riscv.h"
#include "includes/defs.h"
#include "includes/memlayout.h"
#include "includes/spinlock.h"

// http://byterunner.com/16550.html

#define LCR_BAUD_LATCH (1 << 7)  // 修改波特率的模式
#define LCR_EIGHT_BITS (3 << 0)  // 修改字长是8 没有校验
#define IER_RX_ENABLE (1 << 0)   // rx 使能
#define IER_TX_ENABLE (1 << 1)   // tx 使能
// LSR第五位1或者0
// 1 transmitter hold register (or FIFO) is empty. CPU can load the next
// character. 0 transmit holding register is full. 16550 will not accept any
// data for transmission.
#define LSR_TX_IDLE (1 << 5)
// LSR第一位0或者1
// 0 = no data in receive holding register or FIFO.
// 1 = data has been receive and saved in the receive holding register or FIFO.
#define LSR_RX_IDLE (1 << 0)
#define FCR_FIFO_ENABLE (1 << 0)
#define FCR_FIFO_CLEAR (3 << 1)
#define UART_TX_BUF_SIZE 32

struct spinlock uart_tx_lock;
extern volatile int panicked;  // printf定义的 如果内核崩溃 所有输出都被禁止
void uartstart();

// 输出buf
char uart_tx_buf[UART_TX_BUF_SIZE];
uint64 uart_tx_w;  // 写入到 uart_tx_buf[uart_tx_w % UART_TX_BUF_SIZE]
uint64 uart_tx_r;  // 读取到uart_tx_buf[uart_tx_r % UART_TX_BUF_SIZE]

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

// 根据内存映射 确定uart的寄存器地址
volatile static struct uart_16550a_regs *regs =
    (struct uart_16550a_regs *)UART0;

// 初始化uart设备
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
    for (;;);
  }

  while ((regs->LSR & LSR_TX_IDLE) == 0);
  regs->RBR_THR_DLL = c;

  // 开中断
  pop_off();
}

// 从uart读字符
// 返回-1说明没有东西
int uartgetc(void) {
  if (regs->LSR & LSR_RX_IDLE) {
    return regs->RBR_THR_DLL;
  } else {
    return -1;
  }
}

// 添加字符到uart输出buf
// 因为这里会造成阻塞 不能在中断使用
// 只适用于 用户态的write
void uartputc(int c) {
  acquire(&uart_tx_lock);

  // 崩溃时 所有CPU都死循环
  if (panicked) {
    for (;;);
  }
  // 如果为真 说明buffer是满的
  while (uart_tx_w == uart_tx_r + UART_TX_BUF_SIZE) {
    // 等待uartstart唤醒
    sleep(&uart_tx_r, &uart_tx_lock);
  }
  // 写buf
  uart_tx_buf[uart_tx_w % UART_TX_BUF_SIZE] = c;
  // 写头自增
  uart_tx_w += 1;
  // 输出
  uartstart();
  release(&uart_tx_lock);
}

// 当uart可以输出的时候且有字符等待输出 就输出
void uartstart() {
  while (1) {
    if (uart_tx_w == uart_tx_r) {
      // 传输buffer是空
      return;
    }
    if ((regs->LSR & LSR_TX_IDLE) == 0) {
      // 没空下来
      return;
    }

    int c = uart_tx_buf[uart_tx_r % UART_TX_BUF_SIZE];
    // 读头自增
    uart_tx_r += 1;

    // 可能会有uartputc()等待空闲的buf
    wakeup(&uart_tx_r);

    // 输出
    regs->RBR_THR_DLL = c;
  }
}

// uart中断处理函数 有字符输入或者准备输出时被调用
// 从devintr()函数来
void uartintr(void) {
  while (1) {
    int c = uartgetc();
    if (c == -1) {
      break;
    }
    // 处理输入 和处理回显
    consoleintr(c);
  }

  // 发送缓存的字符
  acquire(&uart_tx_lock);
  // 用来清空uart缓冲区
  uartstart();
  release(&uart_tx_lock);
}