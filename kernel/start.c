// 初始化寄存器
#include "params.h"
#include "riscv.h"
#include "types.h"

// main.c中定义
void main();
void timerinit();

// 给每一个CPU分配4096大小的栈 在entry.S中会使用
__attribute__((aligned(16))) char stack0[4096 * NCPU];

void start() {
  // 拿到当前的mstatus寄存器 mstatus中有MPP位 下次执行mret的时候
  // CPU会根据MPP的值切换状态
  unsigned long x = r_mstatus();
  // 清除MPP位
  x &= ~MSTATUS_MPP_MASK;
  // 设置为S模式
  x |= MSTATUS_MPP_S;
  // 应用设置
  w_mstatus(x);

  // 直接设置mepc的值 让执行mret的时候 pc指向main
  w_mepc((uint64)main);

  // 暂时关闭分页
  w_satp(0);

  // 设置中断委托 让S模式处理所有中断和异常
  w_medeleg(0xffff);
  w_mideleg(0xffff);

  // 打开S模式下的三个中断
  w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);

  // 暂时关闭PMP保护
  w_pmpaddr0(0x3fffffffffffffull);
  w_pmpcfg0(0xf);

  // 定时器中断初始化
  timerinit();

  // 设置每一个hart的id 保存到tp寄存器
  int id = r_mhartid();
  w_tp(id);

  asm volatile("mret");
}

void timerinit() {
  // 初始化定时器
}