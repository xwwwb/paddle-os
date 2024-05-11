// 进程相关的系统调用
#include "types.h"
#include "riscv.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "params.h"
#include "proc.h"
#include "defs.h"

// 进程退出 参数是退出状态
uint64 sys_exit(void) {
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // 其实到不了这里 因为exit里面将状态写为了僵尸 并且调度出去了
             // 不会再调度了
}

// 获得pid
uint64 sys_getpid(void) { return myproc()->pid; }

// 进程复刻
uint64 sys_fork(void) { return fork(); }

// 父进程等子进程死亡 参数是一个地址 会将子进程的退出状态码拷贝到p这个地址里
uint64 sys_wait(void) {
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

// 修改进程空间 参数可正可负
uint64 sys_sbrk(void) {
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if (growproc(n) < 0) {
    return -1;
  }
  return addr;
}

// 杀死进程 参数是pid 设置进程是被杀死的 下次调度就调用exit
uint64 sys_kill(void) {
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// 进程睡眠
uint64 sys_sleep(void) {
  int n;
  uint ticks0;
  // 拿到睡眠的时长为n
  argint(0, &n);
  acquire(&tickslock);
  // ticks0是发生系统调用时的系统周期数
  ticks0 = ticks;

  while (ticks - ticks0 < n) {
    if (killed(myproc())) {
      release(&tickslock);
      return -1;
    }
    // 睡眠的时候会睡在ticks上
    // 并且释放tickslock锁 这样每次时钟更新的时候 系统都可以更新
    sleep(&ticks, &tickslock);
    // 时钟更新 从中断处理函数中被唤醒 运行到这里
    // 这时的while比较的参数 ticks0还是发生sleep的时候的周期
    // ticks已经是系统的最新周期了
    // 如果大于了睡眠时长 就退出循环了 进程从调用sleep系统调用的地方接着运行
  }
  release(&tickslock);
  return 0;
}

// 返回系统周期数
uint64 sys_uptime(void) {
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
