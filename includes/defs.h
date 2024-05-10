struct spinlock;
struct sleeplock;
struct proc;
struct context;
struct superblock;
struct stat;
struct inode;

// bio.c 🎉
void binit(void);               // 初始化buffer双向链表
struct buf *bread(uint, uint);  // 给块号 返回带数据的buf
void brelse(struct buf *);      // 释放当前buf的锁
void bwrite(struct buf *);      // 向buf写
void bpin(struct buf *);        // 增加进程对buf的引用
void bunpin(struct buf *);      // 减少进程对buf的引用

// console.c 🎉
void consoleinit();  // 初始化控制台设备
void consputc(int);  // 放置一个字符到控制台 只允许kernel使用
void consoleintr(int);  // 有字符输入的时候 调用这里 处理回显和输入

// spinlock.c 🎉
void initlock(struct spinlock *, char *);  // 初始化锁
void acquire(struct spinlock *);           // 获取锁
void release(struct spinlock *);           // 释放锁
int holding(struct spinlock *);            // 检查是否持有锁
void push_off(void);                       // 关中断
void pop_off(void);                        // 开中断

// uart.c  🎉
void uartinit(void);      // 初始化uart设备
void uartintr(void);      // uart中断处理
void uartputc_sync(int);  // 同步输出单字符
int uartgetc(void);       // 从uart读取字符
void uartputc(int);       // 用户态写入

// printf.c 🎉
void printfinit(void);                         // 初始化printf
void printf(char *, ...);                      // 输出到控制台
void panic(char *) __attribute__((noreturn));  // 报错
void bootinfo();                               // 打印启动信息

// kalloc.c 🎉
void kinit(void);  // 物理内存页分配初始化
void kfree(void *);
void *kalloc();  // 分配一个页的物理内存

// string.c 🎉
void *memset(void *, int, uint);            // 内存赋值
void *memmove(void *, const void *, uint);  // 内存拷贝
char *safestrcpy(char *, const char *, int);  // 安全的字符串拷贝 确保以0结尾
int strncmp(const char *, const char *, uint);  // 字符串比大小
char *strncpy(char *, const char *, int);       // 字符串拷贝
int memcmp(const void *, const void *, uint);   // 内存比较
int strlen(const char *);                       // 字符长度

// vm.c 🎉
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

// proc.c 🎉
int wait(uint64);     // 父进程运行这个等待子进程的死亡
void wakeup(void *);  // 唤醒睡眠的进程 回到执行睡眠锁的位置
void yield(void);     // 放弃CPU 进入下一个任务
int cpuid(void);      // 返回CPU hart id
void exit(int);       // 进程的退出
int fork(void);       // 创建新进程
int growproc(int);    // 进程的内存扩大或者缩小
void sleep(void *, struct spinlock *);  // 进程睡眠 放弃CPU 睡在第一个参数chan上
void userinit(void);                    // 第一个用户进程的初始化
void proc_mapstacks(pagetable_t);                // 映射进程内核栈
pagetable_t proc_pagetable(struct proc *);       // 创建进程页表
void proc_freepagetable(pagetable_t, uint64);    // 清除进程页表
int kill(int);                                   // 给pid 杀掉这个进程
int killed(struct proc *);                       // 返回进程的killed字段
void setkilled(struct proc *);                   // 设置killed字段为1
struct cpu *mycpu(void);                         // 返回CPU的id
struct proc *myproc();                           // 返回进程描述结构
void procinit(void);                             // 初始化进程表
void scheduler(void) __attribute__((noreturn));  // 选择下一个运行的进程
void sched(void);                                // 调度进入scheduler
int either_copyout(int user_dst, uint64 dst, void *src,
                   uint64 len);  // 拷贝到用户或者内核
int either_copyin(void *dst, int user_src, uint64 src,
                  uint64 len);  // 从内核或者用户拷贝出
void procdump(void);            // 打印当前进程列表信息 调试用

// trap.c 🎉
void trapinit(void);               // 初始化陷入
void trapinithart(void);           // 初始化陷入处理函数
void usertrapret(void);            // 返回用户态
extern struct spinlock tickslock;  // 保持ticks变量的原子化
extern uint ticks;  // ticks 变量 用于进程睡眠系统调度

// plic.c 🎉
void plicinit(void);      // 初始化uart和虚拟io的优先级
void plicinithart(void);  // 让PLIC开始接受设备中断
int plic_claim(void);     // 询问中断设备
void plic_complete(int);  // 告诉PLIC中断完成了

// sleeplock.c 🎉
void initsleeplock(struct sleeplock *, char *);  // 初始化自旋锁
void acquiresleep(struct sleeplock *);  // 获得睡眠锁 如果有竞态 就睡觉
void releasesleep(struct sleeplock *);  // 释放睡眠锁 如果有人再等 就唤醒
int holdingsleep(struct sleeplock *);  // 查询锁的持有状态

// fs.c 🎉
void fsinit(int);  // 初始化文件系统 由第一个进程调用 因为用到了睡眠锁
int dirlink(struct inode *, char *, uint);
struct inode *dirlookup(struct inode *, char *, uint *);
struct inode *ialloc(uint, short);   // 分配inode到磁盘
struct inode *idup(struct inode *);  // inode引用次数自增
void iinit();                        // 初始化内存inode表
void ilock(struct inode *);          // 锁定inode
void iput(struct inode *);           // 减少inode引用
void iunlock(struct inode *);        // 解锁inode
void iunlockput(struct inode *);     // 解锁inode+减少inode引用
void iupdate(struct inode *);        // 更新inode到磁盘
int namecmp(const char *, const char *);
struct inode *namei(char *);
struct inode *nameiparent(char *, char *);
int readi(struct inode *, int, uint64, uint, uint);  // 从inode里面读内容
void stati(struct inode *, struct stat *);           // 修改stat的状态
int writei(struct inode *, int, uint64, uint, uint);
void itrunc(struct inode *);  // 舍弃inode

// file.c 🎉
struct file *filealloc(void);                 // 分配文件结构
void fileclose(struct file *);                // 关闭文件结构
struct file *filedup(struct file *);          // 增加引用
void fileinit(void);                          // 文件表初始化
int fileread(struct file *, uint64, int n);   // 文件读
int filestat(struct file *, uint64 addr);     // 读stat到用户空间
int filewrite(struct file *, uint64, int n);  // 文件写

// log.c 🎉
void initlog(int, struct superblock *);  // 事务初始化
void log_write(struct buf *);            // 写块 记录日志
void begin_op(void);                     // 操作开始
void end_op(void);                       // 操作结束

// swtch.S 🎉
void swtch(struct context *, struct context *);  // 内核进程上下文切换

// virtio_disk.c 🎉
void virtio_disk_init(void);
void virtio_disk_rw(struct buf *, int);
void virtio_disk_intr(void);

// syscall.c 🎉
void argint(int, int *);            // 拿到一个int类型的参数
int argstr(int, char *, int);       // 拿到一个str类型的参数
void argaddr(int, uint64 *);        // 拿到一个指针参数
int fetchstr(uint64, char *, int);  // 从用户态内存拿一个字符串
int fetchaddr(uint64, uint64 *);    // 从用户态内存拿一个64位数
void syscall();                     // 系统调用处理函数

// 固定大小的数组 返回元素数 🎉
#define NELEM(x) (sizeof(x) / sizeof((x)[0]))