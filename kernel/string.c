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

// 内存拷贝
// 目标 原地址 大小
void *memmove(void *dst, const void *src, uint n) {
  const char *s;
  char *d;
  if (n == 0) {
    return dst;
  }
  s = src;
  d = dst;
  if (s < d && s + n > d) {
    // 原地址小于目标地址 且内存重叠
    // 从后向前复制
    s += n;
    d += n;
    while (n-- > 0) {
      *--d = *--s;
    }
  } else {
    // 无重叠
    while (n-- > 0) {
      *d++ = *s++;
    }
  }
  return dst;
}

// 字符串拷贝
// 这个函数的作用是将源字符串的前n-1个字符复制到目标字符串，
// 并在末尾添加一个空字符。如果n小于等于0，函数不进行任何复制操作。
char *safestrcpy(char *s, const char *t, int n) {
  char *os;
  os = s;
  if (n <= 0) {
    return os;
  }
  while (--n > 0 && (*s++ = *t++) != 0);
  *s = 0;
  return os;
}

// 字符串比较大小
int strncmp(const char *p, const char *q, uint n) {
  while (n > 0 && *p && *p == *q) {
    n--, p++, q++;
  }
  if (n == 0) {
    return 0;
  }
  // 返回第一个不相同字符的大小比较
  return (uchar)*p - (uchar)*q;
}

// 字符串拷贝
char *strncpy(char *s, const char *t, int n) {
  char *os;

  os = s;
  while (n-- > 0 && (*s++ = *t++) != 0);
  while (n-- > 0) {
    *s++ = 0;
  }
  return os;
}

// 内存比较
int memcmp(const void *v1, const void *v2, uint n) {
  const uchar *s1, *s2;
  s1 = v1;
  s2 = v2;
  while (n-- > 0) {
    if (*s1 != *s2) {
      return *s1 - *s2;
    }
    s1++, s2++;
  }
  return 0;
}

// 字符串长度 找到字符串结尾
int strlen(const char *s) {
  int n;
  for (n = 0; s[n]; n++);
  return n;
}

// 替换掉gcc内置的memcpy 使用memmove
void *memcpy(void *dst, const void *src, uint n) {
  return memmove(dst, src, n);
}