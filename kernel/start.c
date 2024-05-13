// 初始化寄存器
#include "includes/types.h"
#include "includes/memlayout.h"
#include "includes/params.h"
#include "includes/riscv.h"

// main.c中定义
void main();
void timerinit();

// 给每一个CPU分配4096大小的栈 在entry.S中会使用
__attribute__((aligned(16))) char stack0[4096 * NCPU];

// 每个CPU都有5 * 64 位处理定时器中断 定时器配置
uint64 timer_scratch[NCPU][5];

// M 模式下时钟中断处理函数
// kernelvec.S 中
extern void timervec();

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

// 初始化定时器
void timerinit() {
  // 拿到CPU id
  int id = r_mhartid();

  // 设置周期时间 在qemu上 周期大概是 1/10th秒
  int intervel = 1000000;
  *(uint64 *)CLINT_MTIMECMP(id) = *(uint64 *)CLINT_MTIME + intervel;

  // 修改scratch的内容 scratch可以看作是 定时器配置
  // scratch[0,1,2] 用于时钟中断处理函数保存寄存器用
  // scratch[3,4] 分别用于存mtimecmp寄存器位置和周期
  uint64 *scratch = &timer_scratch[id][0];
  scratch[3] = CLINT_MTIMECMP(id);
  scratch[4] = intervel;

  // scratch寄存器存入当前定时器的配置
  w_mscratch((uint64)scratch);

  // 时钟中断控制器
  w_mtvec((uint64)timervec);

  // M中断使能 以及M模式时钟中断使能
  w_mstatus(r_mstatus() | MSTATUS_MIE);

  w_mie(r_mie() | MIE_MTIE);
}