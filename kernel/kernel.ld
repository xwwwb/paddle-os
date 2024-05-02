OUTPUT_ARCH( "riscv" )
ENTRY( _entry )

SECTIONS
{
  . = 0x80000000;

  .text : {
    *(.text .text.*)
    . = ALIGN(0x1000);
    _trampoline = .;
    *(trampsec)
    . = ALIGN(0x1000);
    /* 链接脚本的align是向上对齐的所以只要trampsec有内容 就至少能分到一个4k页 */
    /* 如果表达是为0 则报错 要保证trampsec大小是一个页才不报错 */
    /* 为什么要保证是一个页呢？ 因为在内核页表初始化的时候 对trampoline的size写的是一页 */
    ASSERT(. - _trampoline == 0x1000, "linker error: trampoline larger than one page");
    PROVIDE(etext = .);
  }

  .rodata : {
    . = ALIGN(16);
    *(.srodata .srodata.*)
    . = ALIGN(16);
    *(.rodata .rodata.*)
  }

  .data : {
    . = ALIGN(16);
    *(.sdata .sdata.*)
    . = ALIGN(16);
    *(.data .data.*)
  }

  .bss : {
    . = ALIGN(16);
    *(.sbss .sbss.*)
    . = ALIGN(16);
    *(.bss .bss.*)
  }

  PROVIDE(end = .);
}
