#include "boot/boot.h"

.type __entry, @function
.global __entry

.section .data
// error messages
err_no_cpuid: .string "HALTING: Your CPU is not supported (no CPUID)"
err_no_long:  .string "HALTING: Your CPU is not supported (no long mode, are you on x64?)"
err_no_msr:   .string "HALTING: Your CPU is not supported (no MSR)"

.section .bss
// simple stack
.align 16
.fill BOOT_STACK_SIZE, 1, 0
stack:

.section .text
.code32

// kernel loader entry point
__entry:
  // load the stack
  mov $stack, %esp

  /*

   * we have to make sure we are booted with GRUB multiboot2
   * we push eax as the first arg, which should have the  multiboot2 magic value
   * then we push ebx as the second arg, which should point to the boot info

  */
  push %eax
  push %ebx
  call __mb_check

  // initalize temporary framebuffer video driver (pass the multiboot data as the first argument)
  call __video_init

  // obtain the kernel binary end address
  call __end_get_addr

  // check if CPUID is available
  pushfl
  pop %eax
  mov %eax, %ecx

  /*

   * 21 (ID): Able to use CPUID instruction (Pentium+)
   * https://en.wikipedia.org/wiki/FLAGS_register

  */
  xor $(1 << 21), %eax

  push %eax
  popfl

  pushfl
  pop %eax

  push %ecx
  popfl

  xor %ecx, %eax
  je .Lentry_fail_cpuid

  // check CPU vendor ID (obtained with CPUID, see vendor.S)
  call __vendor_check

  /*

   * check if MSR is available

   * we need MSR for SYSCALL/SYSRET, and we can check it by calling CPUID with EAX = 1
   * to get "Feature Information" in EDX, and bit 5 of this information stores 1 if MSR is
   * supported (Table 3-20. More on Feature Information Returned in the EDX Register from SDM Vol. 2)

   * also SDM says SEP bit is "The SYSENTER and SYSEXIT and associated MSRs are supported"
   * so i guess we don't need to check it for SYSCALL/SYSRET support?

   * we probably don't as we control them with EFER, which should be enabled if we have long mode
   * which will check in a minute

  */
  mov $1, %eax
  cpuid
  test $(1 << 5), %edx
  je .Lentry_fail_msr

  /*

   * check if extended CPUID is available

   * as provided in "Extended Function CPUID Information" section of
   * "Table 3-17. Information Returned by CPUID Instruction (Contd.)" (SDM Vol. 2)
   * when we call CPUID with EAX = 0x80000000, EAX is set to
   * "Maximum Input Value for Extended Function CPUID Information"

   * and for the long mode check we need at least 0x80000001

  */
  mov $0x80000000, %eax
  cpuid
  cmp $0x80000001, %eax
  jb .Lentry_fail_long

  // check if long mode is available
  mov $0x80000001, %eax
  cpuid
  test $(1 << 29), %edx
  je .Lentry_fail_long

  // fast A20 (should work on all supported vendors)
  in $0x92, %al
  or $2, %al
  out %al, $0x92

  // setup paging
  call __paging_setup
  mov %eax, %cr3

  // enable PAE
  mov %cr4, %eax
  or $(1 << 5), %eax
  mov %eax, %cr4

  // switch to long mode (compatibility)
  mov $0xC0000080, %ecx
  rdmsr
  or $(1 << 8), %eax
  wrmsr

  // enable paging
  mov %cr0, %eax
  or $(1 << 31 | 1 << 0), %eax
  mov %eax, %cr0

  // setup GDT
  cli
  lgdt gdt_pointer

  // load code segment offset to eax
  movl $gdt_desc_kernel_code, %eax
  movl $gdt_start, %ebx
  sub %ebx, %eax

  // long jump to 64 bit mode (there's no other way to change cs)
  push %eax
  push $__entry64_lower
  // jmp 0x8:__entry64_lower
  retf

.Lentry_fail_cpuid:
  push $err_no_cpuid
  call __fail

.Lentry_fail_long:
  push $err_no_long
  call __fail

.Lentry_fail_msr:
  push $err_no_msr
  call __fail

.code64
__entry64_lower:
  // load the data segment offset to eax
  movl $gdt_desc_kernel_data, %eax
  sub %ebx, %eax

  // load the data segment
  mov %ax, %ds
  mov %ax, %es
  mov %ax, %fs
  mov %ax, %gs
  mov %ax, %ss

  // jump to the higher half
  movabs $(__entry64 + BOOT_KERNEL_ADDR), %rax
  jmp *%rax

__entry64:
  // load the multiboot data address
  pop %rsi
  mov %esi, %esi // fix the 32 bit address

  /*

   * clear the stack, to do so we use the stos (store string) instruction
   * which writes %al to %rdi %rcx times (incrementing %rdi by one byte each time)

  */
  mov $(stack - BOOT_STACK_SIZE), %rdi
  mov $BOOT_STACK_SIZE, %rcx
  xor %al, %al
  rep stosb

  // reset stack
  xor %rbp, %rbp
  movabs $(stack + BOOT_KERNEL_ADDR), %rsp

  // first argument for the entry is multiboot info address
  mov %rsi, %rdi

  /*

   * call kernel entry, should never return
   * if we somehow do return, halt

  */
  call entry
  call __halt
