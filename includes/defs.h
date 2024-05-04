struct spinlock;
struct sleeplock;

// proc.c
int cpuid(void);
struct cpu *mycpu(void);

// console.c
void consoleinit();
void consputc(int);  // 放置一个字符到控制台 只允许kernel使用

// spinlock.c
void initlock(struct spinlock *, char *);  // 初始化锁
void acquire(struct spinlock *);           // 获取锁
void release(struct spinlock *);           // 释放锁
int holding(struct spinlock *);            // 检查是否持有锁
void push_off(void);                       // 关中断
void pop_off(void);                        // 开中断

// uart.c
void uartinit(void);      // 初始化uart设备
void uartintr(void);      // uart中断处理
void uartputc_sync(int);  // 同步输出单字符

// printf.c
void printfinit(void);                         // 初始化printf
void printf(char *, ...);                      // 输出到控制台
void panic(char *) __attribute__((noreturn));  // 报错
void bootinfo();                               // 打印启动信息

// kalloc.c
void kinit(void);  // 物理内存页分配初始化
void kfree(void *);
void *kalloc();  // 分配一个页的物理内存

// string.c
void *memset(void *, int, uint);            // 内存赋值
void *memmove(void *, const void *, uint);  // 内存拷贝

// vm.c
void kvminit(void);  // 内核虚拟内存初始化
void kvmmap(pagetable_t, uint64, uint64, uint64, int);  // 内核虚拟内存映射
void kvminithart(void);                                 // 开启内核页表
int mappages(pagetable_t, uint64, uint64, uint64, int);  // 页映射
pte_t *walk(pagetable_t, uint64, int);                   // 请求pte
uint64 walkaddr(pagetable_t, uint64);       // 用户态寻找物理地址
pagetable_t uvmcreate(void);                // 创建用户态页表
void uvmfirst(pagetable_t, uchar *, uint);  // 拷贝启动代码到内存
uint64 uvmalloc(pagetable_t, uint64, uint64, int);  // 用户态分配空间
uint64 uvmdealloc(pagetable_t, uint64, uint64);     // 用户态缩减空间
void uvmunmap(pagetable_t, uint64, uint64, int);  // 用户态 取消地址映射
void uvmclear(pagetable_t, uint64);  // 用户态 标记用户态不可访问的地址
int uvmcopy(pagetable_t, pagetable_t, uint64);  //  拷贝新老页表和物理内存
void uvmfree(pagetable_t, uint64);              // 清除用户态物理内存
int copyout(pagetable_t, uint64, char *, uint64);  // 内核态拷贝到用户态
int copyin(pagetable_t, char *, uint64, uint64);  // 用户态拷贝到内核态
int copyinstr(pagetable_t, char *, uint64, uint64);  // 用户态拷贝到内核态

// proc.c
void procinit(void);  // 进程描述表初始化
void proc_mapstacks(pagetable_t);

// trap.c
void trapinit(void);      // 初始化陷入
void trapinithart(void);  // 初始化陷入处理函数

// plic.c
void plicinit(void);      // 初始化uart和虚拟io的优先级
void plicinithart(void);  // 让PLIC开始接受设备中断
int plic_claim(void);     // 询问中断设备
void plic_complete(int);  // 告诉PLIC中断完成了

// sleeplock.c
void initsleeplock(struct sleeplock *, char *);  // 初始化自旋锁

// bio.c
void binit(void);  // 初始化IO缓存双向循环链表

// fs.c
void iinit(void);  // 初始化inode表

// file.c
void fileinit(void);
