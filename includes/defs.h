struct spinlock;

// proc.c
int cpuid(void);

// console.c
void consoleinit();

// spinlock.c
void initlock(struct spinlock *lk, char *name);

// uart.c
void uartinit(void);