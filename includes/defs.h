struct spinlock;

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
void *memset(void *, int, uint);  // 内存赋值

// vm.c
void kvminit(void);  // 内核虚拟内存初始化
void kvmmap(pagetable_t, uint64, uint64, uint64, int);  // 内核虚拟内存映射
int mappages(pagetable_t, uint64, uint64, uint64, int);  // 页映射
pte_t *walk(pagetable_t, uint64, int);                   // 请求pte
void kvminithart(void);                                  // 开启页表

// proc.c
void procinit(void);  // 进程描述表初始化
void proc_mapstacks(pagetable_t);

// trap.c
void trapinit(void);      // 初始化陷入
void trapinithart(void);  // 初始化陷入处理函数