#include "boot/boot.h"
#include "syscall.h"

#include "util/panic.h"
#include "util/asm.h"

struct syscall syscalls[] = {
    {.code = 0, .func = sys_exit},
    {.code = 1, .func = sys_fork},
    {.code = 2, .func = sys_exec},
    {.code = 3, .func = sys_wait},
    {.code = 4, .func = sys_open},
    {.code = 5, .func = sys_close},
    {.code = 6, .func = sys_read},
    {.code = 7, .func = sys_write},
    {.code = 8, .func = sys_mount},
    {.code = 9, .func = sys_umount},
    {.func = NULL},
};

int32_t sys_setup() {
  /*

   * see Table 2-2. IA-32 Architectural MSRs (Contd.) and
   * see SDM Vol 3, 6.8.8 Fast System Calls in 64-Bit Mode

   * to enable SYSCALL/SYSRET, first thing we need to set bit 0 (SCE) in the EFER MSR,
   * then to get them to actually work, we need to set some other MSRs

   * STAR[47:32] = code segment (CS) for kernel (used for syscall)
   * stack segment (SS) is calculated by STAR[47:32] + 8

   * STAR[63:48] + 16 = code segment (CS) for user (used for sysret)
   * stack segment is calculated by STAR[63:48] + 8... why? fuck you that's why

   * LSTAR stores the 64 bit address for the handler that will be called by syscall
   * and FMASK stores a mask for RFLAGS, so when we call syscall, CPU will AND the
   * FMASK's complement with the current RFLAGS to calculate the new RFLAGS... except the
   * first 32 bits are reserved so its actually calculating the EFLAGS... why? well fuck you

  */
  if (gdt_offset(gdt_desc_kernel_code_addr) + 8 != gdt_offset(gdt_desc_kernel_data_addr))
    panic("Invalid GDT structure (bad ring 0 SS offset)");

  if (gdt_offset(gdt_desc_user_data_addr) + 8 != gdt_offset(gdt_desc_user_code_addr))
    panic("Invalid GDT structure (bad ring 3 CS offset)");

  uint64_t efer = _msr_read(MSR_EFER);
  _msr_write(MSR_EFER, efer | 1); // just enable SCE

  _msr_write(
      MSR_STAR, (gdt_offset(gdt_desc_kernel_code_addr) << 32) | ((gdt_offset(gdt_desc_user_data_addr) - 5) << 48));
  _msr_write(MSR_LSTAR, (uint64_t)sys_handler);
  _msr_write(MSR_FMASK,
      UINT64_MAX - (1 << 1)); // bit 1 reserved in eflags (see https://en.wikipedia.org/wiki/FLAGS_register#FLAGS)

  return 0;
}
