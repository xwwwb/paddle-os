#include "types.h"
#include "defs.h"
#include "proc.h"
#include "spinlock.h"
#include "riscv.h"

// 初始化自旋锁
void initlock(struct spinlock *lk, char *name) {
  lk->name = name;
  lk->locked = 0;
  lk->cpu = 0;
}

// 自旋锁的获取锁 会一直循环直到拿到锁为止
void acquire(struct spinlock *lk) {
  // 关中断
  // 如果不关中断 临界区代码如果有可能触发中断 会导致死锁
  push_off();

  // 判断是否持有锁
  if (holding(lk)) {
    panic("acquire");
  }

  /** 使用gcc内嵌的函数实现test & set
   *  在riscv平台上 会被编译成
   *  a5 = 1
   *  s1 = &lk->locked
   *  amoswap.w.aq a5, a5, (s1)
   *  将a5写入s1地址处 并返回s1地址处原本的值
   *  将1写入锁 如果返回1 则说明暂时拿不到锁 别人还在用 写入1也不影响
   *  如果返回0 说明拿到了锁 并且同时写入了1 将锁占领起来
   */
  while (__sync_lock_test_and_set(&lk->locked, 1) != 0)
    ;
  // 让GCC确保没有loads或者stores跨越这里
  // 这是为了保证对临界区内存的访问严格发生在锁被获取之后
  // 在RISC-V架构上，这会生成一个fence指令
  // fence指令将会保证位于这条指令前后的访存指令，都互相不越界
  __sync_synchronize();

  // 给锁指定当前的CPU
  lk->cpu = mycpu();
}

// 释放当前锁
void release(struct spinlock *lk) {
  // 判断是否持有锁
  if (!holding(lk)) {
    panic("release");
  }

  lk->cpu = 0;

  // 确保对临界区内存的访问发生在锁被释放之前
  __sync_synchronize();

  // 使用gcc内置原子函数 锁置为0
  __sync_lock_release(&lk->locked);

  // 开中断
  pop_off();
}

// 检查当前CPU是否持有锁
int holding(struct spinlock *lock) {
  int r;
  r = (lock->locked && lock->cpu == mycpu());
  return r;
}

// push_off和pop_off在开关中断的基础上增加了层级关系
// 保证了开关中断的嵌套调用 还保证了最还原最外层的中断状态
void push_off(void) {
  // 拿到当前的中断状态
  int old_intr_status = intr_get();

  // 关中断
  intr_off();
  // 如果当前是第一层关中断 那么记录之前的状态
  // 如果是第二层就没必要记录了 因为肯定是关的状态
  if (mycpu()->noff == 0) {
    mycpu()->intena = old_intr_status;
  }
  // 层级加一
  mycpu()->noff += 1;
}

void pop_off(void) {
  // 拿到当前处理器
  struct cpu *c = mycpu();
  // 如果当前处理器没有关中断 说明单独出现了pop_off 出错
  if (intr_get()) {
    panic("pop_off error!");
  }
  // 如果层级小于1 也说明没有push_off就执行了pop_off 出错
  if (c->noff < 1) {
    panic("pop_off error");
  }
  c->noff -= 1;

  // 如果层级为0 说明是最外层的pop_off 需要开中断
  // 如果层级不是0 说明外层还可以调用pop_off 由外层的pop_off来开中断
  if (c->noff == 0 && c->intena) {
    intr_on();
  }
}