// CPU结构体
struct cpu {
  int noff;    // 记录关中断的层级
  int intena;  // 记录关中断前 中断的状态
};