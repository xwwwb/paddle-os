#include "spinlock.h"

// 初始化自旋锁
void initlock(struct spinlock *lk, char *name)
{
    lk->name = name;
    lk->locked = 0;
    // lk->cpu = 0;
}