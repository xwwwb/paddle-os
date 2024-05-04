struct buf {
  int valid;  // 数据是否有效 也就是数据是否从磁盘读进来了
  int dist;   // 磁盘是否拥有当前buf
  uint dev;   // 设备
  uint blockno;           // 块号
  struct sleeplock lock;  // 读写缓存块耗时会长 用睡眠锁
  uint refcnt;
  struct buf *prev;  // LRU 缓存列表
  struct buf *next;
  uchar data[1024];
};