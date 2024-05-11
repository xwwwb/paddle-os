#include "types.h"
#include "params.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "syscall.h"
#include "riscv.h"
#include "proc.h"
#include "defs.h"

// 定义在其他文件中的 系统调用处理函数
extern uint64 sys_fork(void);
extern uint64 sys_exit(void);
extern uint64 sys_wait(void);
// extern uint64 sys_pipe(void);
extern uint64 sys_read(void);
extern uint64 sys_kill(void);
extern uint64 sys_exec(void);
extern uint64 sys_fstat(void);
extern uint64 sys_chdir(void);
extern uint64 sys_dup(void);
extern uint64 sys_getpid(void);
extern uint64 sys_sbrk(void);
extern uint64 sys_sleep(void);
extern uint64 sys_uptime(void);
extern uint64 sys_open(void);
extern uint64 sys_write(void);
extern uint64 sys_mknod(void);
extern uint64 sys_unlink(void);
extern uint64 sys_link(void);
extern uint64 sys_mkdir(void);
extern uint64 sys_close(void);

// 系统调用列表 函数指针列表
// 映射调用号到实际的系统调用函数
static uint64 (*syscalls[])(void) = {
    [SYS_fork] sys_fork,
    [SYS_exit] sys_exit,
    [SYS_wait] sys_wait,
    // [SYS_pipe] sys_pipe,
    [SYS_read] sys_read,
    [SYS_kill] sys_kill,
    [SYS_exec] sys_exec,
    [SYS_fstat] sys_fstat,
    [SYS_chdir] sys_chdir,
    [SYS_dup] sys_dup,
    [SYS_getpid] sys_getpid,
    [SYS_sbrk] sys_sbrk,
    [SYS_sleep] sys_sleep,
    [SYS_uptime] sys_uptime,
    [SYS_open] sys_open,
    [SYS_write] sys_write,
    [SYS_mknod] sys_mknod,
    [SYS_unlink] sys_unlink,
    [SYS_link] sys_link,
    [SYS_mkdir] sys_mkdir,
    [SYS_close] sys_close,
};

void syscall(void) {
  int num;
  struct proc *p = myproc();

  // 取系统调用号
  // 发生陷入的时候 把调用号存到a7 这是Linux的行为 模仿一下
  num = p->trapframe->a7;
  if (num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    // 将返回值放入a0 这也是Linux的行为
    // 执行系统调用处理函数
    p->trapframe->a0 = syscalls[num]();
  } else {
    // 未知系统调用
    printf("%d %s: unknown sys call %d\n", p->pid, p->name, num);
    p->trapframe->a0 = -1;
  }
}

// 取原始的系统调用参数
// 根据ABI 使用a0-a5寄存器存放参数
static uint64 argraw(int n) {
  struct proc *p = myproc();
  switch (n) {
    case 0:
      return p->trapframe->a0;
    case 1:
      return p->trapframe->a1;
    case 2:
      return p->trapframe->a2;
    case 3:
      return p->trapframe->a3;
    case 4:
      return p->trapframe->a4;
    case 5:
      return p->trapframe->a5;
  }
  panic("argraw");
  return -1;
}

// 传入用户态的地址 和写入的buf地址 max是长度
// 返回字符串长度 并且将字符拷贝到buf中
int fetchstr(uint64 addr, char *buf, int max) {
  struct proc *p = myproc();
  if (copyinstr(p->pagetable, buf, addr, max) < 0) {
    return -1;
  }
  return strlen(buf);
}

// 获取一个64位的数 从给定的用户态地址addr获得
// 从addr取到地址后写入ip中
int fetchaddr(uint64 addr, uint64 *ip) {
  struct proc *p = myproc();
  // 如果地址在页表以外 或地址+64位后再页表以外
  if (addr >= p->sz || addr + sizeof(uint64) > p->sz) {
    // 不通过
    return -1;
  }
  if (copyin(p->pagetable, (char *)ip, addr, sizeof(*ip)) != 0) {
    return -1;
  }
  return 0;
}

// 取int系统调用参数 32位
void argint(int n, int *ip) { *ip = argraw(n); }

// 取参数中的指针
void argaddr(int n, uint64 *ip) { *ip = argraw(n); }

// 取参数中的字符串
// 由于寄存器中存的是地址 所以用argaddr先取地址
// 然后把地址中的字符串拷贝到内核空间
int argstr(int n, char *buf, int max) {
  uint64 addr;
  // 取出字符串参数的地址
  argaddr(n, &addr);
  // 返回字符串长度和将字符写入buf
  return fetchstr(addr, buf, max);
}