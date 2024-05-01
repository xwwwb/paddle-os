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

// kalloc.c
void kinit(void);  // 物理内存页分配初始化
void kfree(void *);
void *kalloc();  // 分配一个页的物理内存

// string.c
void *memset(void *, int, uint);  // 内存赋值

// vm.c
void kvminit(void);