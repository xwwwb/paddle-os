// ELF可执行文件的格式
// https://elixir.bootlin.com/glibc/glibc-2.39/source/elf/elf.h

// 原本的ELF_MAGIC是 7F 'E' 'L' 'F'
// 'E' 0x45 'L' 0x4C 'F' 0x46
// 这里是小端序的形式
#define ELF_MAGIC 0x464C457FU

// elf头
struct elfhdr {
  uint magic;  // 必须和魔法标志一样
  uchar elf[12];
  ushort type;
  ushort machine;
  uint version;
  uint64 entry;
  uint64 phoff;
  uint64 shoff;
  uint flags;
  ushort ehsize;
  ushort phentsize;
  ushort phnum;
  ushort shentsize;
  ushort shnum;
  ushort shstrndx;
};

// https://elixir.bootlin.com/glibc/glibc-2.39/source/elf/elf.h#L697
struct proghdr {
  uint32 type;    // 一般只关注LOAD
  uint32 flags;   // 读写执行
  uint64 off;     // 在文件中的偏移
  uint64 vaddr;   // 第一个字节在进程中的起始位置
  uint64 paddr;   // 物理装载地址
  uint64 filesz;  // segment在elf中的大小
  uint64 memsz;   // 在虚拟内存中的长度
  uint64 align;   // 对齐属性
};
// Loadable program segment
// https://elixir.bootlin.com/glibc/glibc-2.39/source/elf/elf.h#L718
#define ELF_PROG_LOAD 1

// https://elixir.bootlin.com/glibc/glibc-2.39/source/elf/elf.h#L742
#define ELF_PROG_FLAG_EXEC 1
#define ELF_PROG_FLAG_WRITE 2
#define ELF_PROG_FLAG_READ 4