struct spinlock;

// proc.c
int cpuid(void);

// console.c
void consoleinit();
void consputc(int);

// spinlock.c
void initlock(struct spinlock *lk, char *name);

// uart.c
void uartinit(void);      // 初始化uart设备
void uartputc_sync(int);  // 同步输出单字符

// printf.c
void printfinit(void);                         // 初始化printf
void printf(char *, ...);                      // 输出到控制台
void panic(char *) __attribute__((noreturn));  // 报错