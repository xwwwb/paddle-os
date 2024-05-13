// 执行新的app
#include "includes/types.h"
#include "includes/params.h"
#include "includes/riscv.h"
#include "includes/spinlock.h"
#include "includes/sleeplock.h"
#include "includes/proc.h"
#include "includes/elf.h"
#include "includes/defs.h"

static int loadseg(pde_t *, uint64, struct inode *, uint, uint);

// 这里flags是program header的flags
int flags2perm(int flags) {
  int perm = 0;
  if (flags & ELF_PROG_FLAG_EXEC) {
    perm = PTE_X;
  }
  if (flags & ELF_PROG_FLAG_WRITE) {
    perm |= PTE_W;
  }
  return perm;
}

// 替换进程
int exec(char *path, char **argv) {
  char *s, *last;
  int i, off;
  uint64 argc, sz = 0, sp, ustack[MAXARG], stackbase;
  struct elfhdr elf;
  struct inode *ip;
  struct proghdr ph;
  pagetable_t pagetable = 0, oldpagetable;
  struct proc *p = myproc();

  // 开始磁盘事务
  begin_op();

  // 根据path找文件inode
  if ((ip = namei(path)) == 0) {
    printf("exec cannot find executable file: %s\n", path);
    end_op();
    return -1;
  }
  // 锁定inode
  ilock(ip);
  // 读elf header
  if (readi(ip, 0, (uint64)&elf, 0, sizeof(elf)) != sizeof(elf)) {
    goto bad;
  }
  if (elf.magic != ELF_MAGIC) {
    goto bad;
  }
  if ((pagetable = proc_pagetable(p)) == 0) {
    goto bad;
  }
  // 装载程序到内存
  // 遍历program header program header是装载视图 规定了每个segment的装载类型
  for (i = 0, off = elf.phoff; i < elf.phnum; i++, off += sizeof(ph)) {
    // 读一个program header结构 写入ph
    if (readi(ip, 0, (uint64)&ph, off, sizeof(ph)) != sizeof(ph)) {
      goto bad;
    }
    if (ph.type != ELF_PROG_LOAD) {
      continue;
    }

    if (ph.memsz < ph.filesz) {
      goto bad;
    }
    if (ph.vaddr + ph.memsz < ph.vaddr) {
      goto bad;
    }
    if (ph.vaddr % PGSIZE != 0) {
      goto bad;
    }

    uint64 sz1;
    // oldsz 是sz newsz是ph.vaddr+ph.memsz
    // ph.vaddr是Segment第一个字节在虚拟地址的起始位置
    // ph.memsz是虚拟地址中所占长度
    // sz1是新大小
    // 因为编译的时候 代码段在虚拟地址0的位置
    // 所以第一次循环的sz 和ph.vaddr都是0 ph.memsz是真实长度
    // 第二次循环sz是第一次循环的新增的大小 ph.vaddr+ph.memsz是新的大小
    if ((sz1 = uvmalloc(pagetable, sz, ph.vaddr + ph.memsz,
                        flags2perm(ph.flags))) == 0) {
      goto bad;
    }

    sz = sz1;

    // 装载段到内存
    // 因为读文件 所以传入的是ph.filesz
    if (loadseg(pagetable, ph.vaddr, ip, ph.off, ph.filesz) < 0) {
      goto bad;
    }
  }
  // 不需要inode了
  iunlockput(ip);
  end_op();

  ip = 0;
  p = myproc();

  uint64 oldsz = p->sz;

  // 分配两个内存页做用户栈
  // 第一个做stack guard 第二个是用户栈
  sz = PGROUNDUP(sz);
  uint64 sz1;
  // 重新分配俩栈
  if ((sz1 = uvmalloc(pagetable, sz, sz + 2 * PGSIZE, PTE_W)) == 0) {
    goto bad;
  }
  // sz指向栈的最高地址
  sz = sz1;
  // 标记stack guard是用户不可访问的
  uvmclear(pagetable, sz - 2 * PGSIZE);
  sp = sz;
  // 栈顶 也是stack guard的栈底
  stackbase = sp - PGSIZE;

  // 放置参数进用户栈
  // 遍历存在的参数
  for (argc = 0; argv[argc]; argc++) {
    if (argc >= MAXARG) {
      goto bad;
    }
    sp -= strlen(argv[argc]) + 1;
    // 栈地址必须16字节对齐
    sp -= sp % 16;
    if (sp < stackbase) {
      // 栈溢出
      goto bad;
    }
    // 拷贝参数到栈
    // 拷贝 argv[argc] 到 sp
    if (copyout(pagetable, sp, argv[argc], strlen(argv[argc]) + 1) < 0) {
      goto bad;
    }
    // 压栈后的地址
    // 此时数组里面存的是地址
    ustack[argc] = sp;
  }
  // 字符数组的结束
  ustack[argc] = 0;
  // argv是一个二阶数组
  // 接着放 argv[] 数组
  sp -= (argc + 1) * sizeof(uint64);
  sp -= sp % 16;
  if (sp < stackbase) {
    goto bad;
  }
  if (copyout(pagetable, sp, (char *)ustack, (argc + 1) * sizeof(uint64)) < 0) {
    goto bad;
  }
  /**
   * 到此为止的内存布局 比如说exec(/sh -a -v)
   * --栈顶--
   * 内容是"-a"
   * 内容是 "-v"
   * 内容是 0
   * argv[1] 内容是字符串"-v的地址"
   * argv[0] 内容是字符串"-a"的地址  sp存的地址指向此处
   */

  // main(argc, argv) 两个参数argc放a0 argv放a1
  // 此时char **argv 指向argv[0] argv[0]只想字符串的地址 也就是"-a"
  // 一般调用约定 会将返回值放到a0 所以return 的时候return argc;
  p->trapframe->a1 = sp;
  // exec("/sh") last等于sh
  for (last = s = path; *s; s++) {
    if (*s == '/') {
      last = s + 1;
    }
  }
  // 拷贝进程名字为sh
  safestrcpy(p->name, last, sizeof(p->name));

  // 配置用户寄存器 让其可以运行
  oldpagetable = p->pagetable;
  // 上面新开的页表
  p->pagetable = pagetable;
  p->sz = sz;
  // 程序入口虚拟地址
  p->trapframe->epc = elf.entry;
  // 用户栈 sp当前指向的是 放置argc和argv完之后的地址 其余空白的地址都是用户栈
  p->trapframe->sp = sp;
  // 清除老的用户页表
  proc_freepagetable(oldpagetable, oldsz);

  return argc;

bad:
  if (pagetable) {
    proc_freepagetable(pagetable, sz);
  }
  if (ip) {
    iunlockput(ip);
    end_op();
  }
  return -1;
}

// 装载 program segment 到页表的va地址中
// va必须是页对齐的
// 并且va要是映射过物理内存的
// 页表 虚拟地址 inode 文件中的偏移 文件中的大小
static int loadseg(pagetable_t pagetable, uint64 va, struct inode *ip,
                   uint offset, uint sz) {
  uint i, n;
  uint64 pa;
  for (i = 0; i < sz; i += PGSIZE) {
    pa = walkaddr(pagetable, va + i);
    if (pa == 0) {
      panic("loadseg: address should exist");
    }
    // 不足物理页一页的时候 读入剩余
    if (sz - i < PGSIZE) {
      n = sz - i;
    } else {
      n = PGSIZE;
    }
    if (readi(ip, 0, (uint64)pa, offset + i, n) != n) {
      return -1;
    }
  }
  return 0;
}