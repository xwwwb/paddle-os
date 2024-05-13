#include "includes/types.h"
#include "includes/riscv.h"
#include "includes/spinlock.h"
#include "includes/sleeplock.h"
#include "includes/params.h"
#include "includes/proc.h"
#include "includes/defs.h"

// 初始化睡眠锁
void initsleeplock(struct sleeplock *lk, char *name) {
  initlock(&lk->lk, "sleep lock");
  lk->name = name;
  lk->locked = 0;
  lk->pid = 0;
}

// 拿到睡眠锁 如果可以拿到锁就直接拿到
// 如果拿不到锁 就把当前内核线程暂停到这里 因为有sched函数的存在
void acquiresleep(struct sleeplock *lk) {
  acquire(&lk->lk);
  while (lk->locked) {
    // 当走到这里 说明当前的锁有人再用 先放弃当前内核线程
    // 暂停到这里
    sleep(lk, &lk->lk);
    // 走到这里 说明调用了wakeup 且是releasesleep的wakeup
    // 同时lk->locked已经变成0了 直接退出循环
  }
  // 拿到锁
  lk->locked = 1;
  lk->pid = myproc()->pid;
  release(&lk->lk);
}

// 释放睡眠锁
void releasesleep(struct sleeplock *lk) {
  acquire(&lk->lk);
  lk->locked = 0;
  lk->pid = 0;
  wakeup(lk);  // 唤醒在等待锁的进程
  release(&lk->lk);
}

// 锁的持有状态
// 只有相同的进程才有资格访问锁的状态
int holdingsleep(struct sleeplock *lk) {
  int r;
  acquire(&lk->lk);
  r = lk->locked && (lk->pid == myproc()->pid);
  release(&lk->lk);
  return r;
}