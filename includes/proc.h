// 任务调度的时候 从用户态陷入内核态时 保存内核态的状态
// 主要由swtch.S调用
struct context {
  uint64 ra;
  uint64 sp;

  // callee-saved 寄存器
  uint64 s0;
  uint64 s1;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 s9;
  uint64 s10;
  uint64 s11;
};

// CPU结构体
struct cpu {
  struct proc *proc;  // 运行在该CPU上的进程
  int noff;           // 记录关中断的层级
  int intena;         // 记录关中断前 中断的状态
  struct context context;  // 用户态陷入的时候 记录陷入后内核态的状态
};

enum procstate { UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

// 汇编中访问的时候 依次地址加8
// 该地址被进程页表映射到仅次于trampoline代码的位置
// 用于陷入发生的时候保存用户态上下文以及保存一些陷入处理过程中
// 内核态所需要的状态
struct trapframe {
  uint64 kernel_satp;    // 内核页表
  uint64 kernel_sp;      // 内核栈顶
  uint64 kernel_trap;    // usertrap() trap处理函数
  uint64 epc;            // 陷入发生时的保存的地址
  uint64 kernel_hartid;  // 保存内核态的tp
  // riscv全部非特权指令集 除x0 zero寄存器外
  uint64 ra;
  uint64 sp;
  uint64 gp;
  uint64 tp;
  uint64 t0;
  uint64 t1;
  uint64 t2;
  uint64 s0;
  uint64 s1;
  uint64 a0;
  uint64 a1;
  uint64 a2;
  uint64 a3;
  uint64 a4;
  uint64 a5;
  uint64 a6;
  uint64 a7;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 s9;
  uint64 s10;
  uint64 s11;
  uint64 t3;
  uint64 t4;
  uint64 t5;
  uint64 t6;
};

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

  struct trapframe *trapframe;  // 发生陷入的时候保存上下文用
  struct context context;       // swtch在这里保存内核态上下文
  // struct file *ofile[NOFILE];   // Open files
  // struct inode *cwd;            // Current directory
};
