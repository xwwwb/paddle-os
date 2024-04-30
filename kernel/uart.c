#include "types.h"
#include "memlayout.h"
#include "spinlock.h"
#include "defs.h"
// http://byterunner.com/16550.html
struct uart_16550a_regs
{
    uint8 RBR_THR_DLL; // 0x00 Receiver Buffer Register / Transmitter Holding Register / Divisor Latch LSB
    uint8 IER_DLM;     // 0x01 Interrupt EnableRegister / Divisor Latch MSB
    uint8 IIR_FCR;     // 0x02 Interrupt Status Register / FIFO Control Register
    uint8 LCR;         // 0x03 Line Control Register
    uint8 MCR;         // 0x04 Modem Control Register
    uint8 LSR;         // 0x05 LineStatus Register
    uint8 MSR;         // 0x06 Modem Status Register
    uint8 SPR;         // 0x07 ScratchPad Register
};

#define LCR_BAUD_LATCH (1 << 7) // 修改波特率的模式
#define LCR_EIGHT_BITS (3 << 0) // 修改字长是8 没有校验

#define IER_RX_ENABLE (1 << 0) // rx 使能
#define IER_TX_ENABLE (1 << 1) // tx 使能

#define FCR_FIFO_ENABLE (1 << 0)
#define FCR_FIFO_CLEAR (3 << 1)

// 根据内存映射 确定uart的寄存器地址
volatile static struct uart_16550a_regs *regs = (struct uart_16550a_regs *)UART0;

struct spinlock uart_tx_lock;

void uartinit(void)
{
    // 关中断
    regs->IER_DLM = 0;

    // 打开修改波特率的设置开关
    regs->LCR = (regs->LCR | LCR_BAUD_LATCH);

    // 写入DLL和DLM 存入数字0x003
    regs->RBR_THR_DLL = 0x03;
    regs->IER_DLM = 0x00;

    // 设置字长为8 和无校验 关闭设置波特率的开关
    regs->LCR = regs->LCR | LCR_EIGHT_BITS;

    // 重设并开启FIFO
    regs->IIR_FCR = (FCR_FIFO_ENABLE | FCR_FIFO_CLEAR);

    // 打开rx和tx的中断
    regs->IER_DLM = (IER_TX_ENABLE | IER_RX_ENABLE);

    // 初始化锁
    initlock(&uart_tx_lock, "uart");
}
