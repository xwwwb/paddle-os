#include "types.h"

// 映射设备号到设备函数
struct devsw {
  int (*read)(int, uint64, int);
  int (*write)(int, uint64, int);
};

// 向外声明有名为devsw的数组 实际定义在file.c中
extern struct devsw devsw[];

// 控制台设备号是1
#define CONSOLE 1