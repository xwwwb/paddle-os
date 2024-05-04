struct sleeplock {
  uint locked;         // 是否拿着睡眠锁
  struct spinlock lk;  // 修改睡眠锁的操作是原子的

  char *name;
  int pid;
};