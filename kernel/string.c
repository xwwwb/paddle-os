// 内存操作
#include "types.h"

// dst是内存地址 c是被赋予的值 n是大小 从dst到dst+n的内存等于c
void *memset(void *dst, int c, uint n) {
  // 以字节为单位
  char *_dst = (char *)dst;
  int i;
  for (i = 0; i < n; i++) {
    _dst[i] = c;
  }
  return dst;
}