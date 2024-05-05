// 定义一些riscv平台的操作 包括读写常规寄存器和特权寄存器
// 汇编引入头文件时 只能引入声明 用GCC内置的宏区分开
#ifndef __ASSEMBLER__
// 读取当前的hartid
static inline uint64 r_mhartid() {
  uint64 x;
  asm volatile("csrr %0, mhartid" : "=r"(x));
  return x;
}

// tp寄存器的读写
static inline uint64 r_tp() {
  uint64 x;
  asm volatile("mv %0, tp" : "=r"(x));
  return x;
}

// 写tp寄存器
static inline void w_tp(uint64 x) { asm volatile("mv tp, %0" : : "r"(x)); }

// mstatus的位
#define MSTATUS_MPP_MASK (3L << 11)  // MPP位
#define MSTATUS_MPP_M (3L << 11)     // MPP位的值 M mode
#define MSTATUS_MPP_S (1L << 11)     // MPP位的值 S mode
#define MSTATUS_MPP_U (0L << 11)     // MPP位的值 U mode
#define MSTATUS_MIE (1L << 3)        // 机器模式下中断使能位

static inline uint64 r_mstatus() {
  uint64 x;
  asm volatile("csrr %0, mstatus" : "=r"(x));
  return x;
}

static inline void w_mstatus(uint64 x) {
  asm volatile("csrw mstatus, %0" : : "r"(x));
}

// S 模式下陷入发生的原因
static inline uint64 r_scause() {
  uint64 x;
  asm volatile("csrr %0, scause" : "=r"(x));
  return x;
}

// S 模式下 异常原因
static inline uint64 r_stval() {
  uint64 x;
  asm volatile("csrr %0, stval" : "=r"(x));
  return x;
}

// S 模式下的中断 pending寄存器
// 主动写入可以制造软中断
static inline uint64 r_sip() {
  uint64 x;
  asm volatile("csrr %0, sip" : "=r"(x));
  return x;
}

static inline void w_sip(uint64 x) { asm volatile("csrw sip, %0" : : "r"(x)); }

// M模式 中断使能寄存器MIE
#define MIE_MEIE (1L << 11)  // external
#define MIE_MTIE (1L << 7)   // timer
#define MIE_MSIE (1L << 3)   // software
static inline uint64 r_mie() {
  uint64 x;
  asm volatile("csrr %0, mie" : "=r"(x));
  return x;
}

static inline void w_mie(uint64 x) { asm volatile("csrw mie, %0" : : "r"(x)); }

// 执行mret的时候会跳转到mepc寄存器的值
static inline void w_mepc(uint64 x) {
  asm volatile("csrw mepc, %0" : : "r"(x));
}

// 写入m模式的scratch寄存器
static inline void w_mscratch(uint64 x) {
  asm volatile("csrw mscratch, %0" : : "r"(x));
}
// M 模式的陷入控制函数
static inline void w_mtvec(uint64 x) {
  asm volatile("csrw mtvec, %0" : : "r"(x));
}

// sie寄存器是S模式下的中断使能寄存器
#define SIE_SEIE (1L << 9)  // 外部中断
#define SIE_STIE (1L << 5)  // 计时器中断
#define SIE_SSIE (1L << 1)  // 软中断
static inline uint64 r_sie() {
  uint64 x;
  asm volatile("csrr %0, sie" : "=r"(x));
  return x;
}

static inline void w_sie(uint64 x) { asm volatile("csrw sie, %0" : : "r"(x)); }

// 中断委托
static inline uint64 r_mideleg() {
  uint64 x;
  asm volatile("csrr %0, mideleg" : "=r"(x));
  return x;
}

static inline void w_mideleg(uint64 x) {
  asm volatile("csrw mideleg, %0" : : "r"(x));
}

// 异常委托
static inline uint64 r_medeleg() {
  uint64 x;
  asm volatile("csrr %0, medeleg" : "=r"(x));
  return x;
}

static inline void w_medeleg(uint64 x) {
  asm volatile("csrw medeleg, %0" : : "r"(x));
}

// stap寄存器是页表基址寄存器
static inline uint64 r_satp() {
  uint64 x;
  asm volatile("csrr %0, satp" : "=r"(x));
  return x;
}

static inline void w_satp(uint64 x) {
  asm volatile("csrw satp, %0" : : "r"(x));
}

// 物理内存保护
static inline void w_pmpcfg0(uint64 x) {
  asm volatile("csrw pmpcfg0, %0" : : "r"(x));
}

static inline void w_pmpaddr0(uint64 x) {
  asm volatile("csrw pmpaddr0, %0" : : "r"(x));
}

#define SSTATUS_SPP (1L << 8)  // 陷入发生时 存发生时的模式 1是S 0是U
#define SSTATUS_SIE (1L << 1)   // S模式下的中断使能位
#define SSTATUS_SPIE (1L << 5)  // 进入陷入时 SIE的值

// 拿s状态的sstatus寄存器
static inline uint64 r_sstatus() {
  uint64 x;
  asm volatile("csrr %0, sstatus" : "=r"(x));
  return x;
}
// 写s状态的sstatus寄存器
static inline void w_sstatus(uint64 x) {
  asm volatile("csrw sstatus, %0" : : "r"(x));
}

// 写s模式的陷入处理寄存器
static inline void w_stvec(uint64 x) {
  asm volatile("csrw stvec, %0" : : "r"(x));
}

// 读写sepc 记录了陷入发生时的指针
static inline void w_sepc(uint64 x) {
  asm volatile("csrw sepc, %0" : : "r"(x));
}

static inline uint64 r_sepc() {
  uint64 x;
  asm volatile("csrr %0, sepc" : "=r"(x));
  return x;
}

// 关中断
static inline void intr_off() {
  // sstatus的SIE位清零
  w_sstatus(r_sstatus() & ~SSTATUS_SIE);
}
// 开中断
static inline void intr_on() {
  // sstatus的SIE位置1
  w_sstatus(r_sstatus() | SSTATUS_SIE);
}

// 拿中断状态
static inline int intr_get() {
  uint64 x = r_sstatus();
  return (x & SSTATUS_SIE) != 0;
}

// 开启riscv的sv39分页模式 S U的仿存走分页 M模式不讨论
// satp的60-64位存mode mode为8是sv39模式
#define SATP_SV39 (8L << 60)
// satp的PPN位存的是物理页号
#define MAKE_SATP(pagetable) (SATP_SV39 | (((uint64)pagetable) >> 12))

// 刷新快表TLB
static inline void sfence_vma() {
  // zero, zero是清除所有快表项
  asm volatile("sfence.vma zero, zero");
}

// pte是页表项一个64位数
typedef uint64 pte_t;
// 页表是一个指向64位长度空间的指针
// 一个页表有512个项 也就是512 * 64 / 1024 / 8 = 4k
// 正好一个页表是一个页的大小
typedef uint64 *pagetable_t;

#endif

// 内存对齐配置
#define PGSIZE 4096  // 页的大小是4k
#define PGSHIFT 12   // 虚拟地址中 低12位为页内偏移地址
// 向上对齐和向下对齐
#define PGROUNDUP(sz) (((sz) + PGSIZE - 1) & ~(PGSIZE - 1))
#define PGROUNDDOWN(a) (((a)) & ~(PGSIZE - 1))

// PTE从第0位开始是 V R W X U G A D
#define PTE_V (1L << 0)  // 有效位
#define PTE_R (1L << 1)  // 可读
#define PTE_W (1L << 2)  // 可写
#define PTE_X (1L << 3)  // 可执行
#define PTE_U (1L << 4)  // 用户态可访问

// 最大的虚拟地址数量
// 只管理256G
#define MAXVA (1L << (9 + 9 + 9 + 12 - 1))

#define PXMASK 0x1ff  // 9个比特
// level = 2 结果是30 level = 1 结果是21 level = 0 结果12
#define PXSHIFT(level) (PGSHIFT + (9 * (level)))
// 传入虚拟地址 和层级 按层级取值
// 将地址右移30 21 或者 12 然后取9位即可得到当前层级的地址值
#define PX(level, va) ((((uint64)(va)) >> PXSHIFT(level)) & PXMASK)

// PTE中的地址号转物理地址
// 具体为什么这么处理 是硬件规定的
// PTE中低10位是FLAG位 先右移10位去掉flag 再左移12得到真实地址
#define PTE2PA(pte) (((pte) >> 10) << 12)
// 物理地址转PTE中的号码 右移12是为了得到页号 左移10位是腾出flag位
#define PA2PTE(pa) (((((uint64)pa)) >> 12) << 10)

// 取前9位
#define PTE_FLAGS(pte) ((pte) & 0x3FF)
