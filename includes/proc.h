// CPU结构体
struct cpu {
  int noff;    // 记录关中断的层级
  int intena;  // 记录关中断前 中断的状态
};

enum procstate { UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

// 进程描述符
struct proc {
  struct spinlock lock;  // 当修改进程描述符的值时必须上锁

  enum procstate state;  // 进程当前状态
  void *chan;            // 如果非空 在chan上睡眠
  int killed;            // 如果非空 进程已经被杀死的
  int xstate;            // 退出状态 父进程可以拿到
  int pid;               // 进程ID

  // 必须在有walt_lock的时候才能使用这个
  struct proc *parent;  // 父进程

  // 下面的信息由进程自己使用 不用锁
  uint64 kstack;          // 进程的内核栈
  uint64 sz;              // 进程的内存大小
  char name[16];          // 进程的名字
  pagetable_t pagetable;  // 用户态的页表

  // struct trapframe *trapframe;  // data page for trampoline.S
  // struct context context;       // swtch() here to run process
  // struct file *ofile[NOFILE];   // Open files
  // struct inode *cwd;            // Current directory
};