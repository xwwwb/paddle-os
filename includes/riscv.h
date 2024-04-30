// 定义一些riscv寄存器操作函数

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

// 执行mret的时候会跳转到mepc寄存器的值
static inline void w_mepc(uint64 x) {
  asm volatile("csrw mepc, %0" : : "r"(x));
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