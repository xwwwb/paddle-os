struct spinlock;

// proc.c
int cpuid(void);

// console.c
void consoleinit();

// spinlock.c
void initlock(struct spinlock *lk, char *name);

// uart.c
void uartinit(void);  // 初始化uart设备

// printf.c
void printfinit(void);                         // 初始化printf
void printf(char *, ...);                      // 输出到控制台
void panic(char *) __attribute__((noreturn));  // 报错