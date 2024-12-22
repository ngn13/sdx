.intel_syntax noprefix

// TODO: rewrite in GAS (AT&T) syntax

.equ STACK_SIZE, 0x10000

.section .data
  cpu_vendor:         .string "not detected"

  // supported CPU vendor IDs
  cpu_vendor_supported: .string "AuthenticAMDAMDisbetter!GenuineIntelTCGTCGTCGTCG KVMKVMKVM  VMwareVMwareVBoxVBoxVBox"

  // error messages
  err_no_cpuid:   .string "HALTING: Your CPU is not supported (no CPUID)"
  err_bad_vendor: .string "HALTING: Your CPU is not supported (unknown vendor)"
  err_no_magic:   .string "HALTING: Magic number is not set (did you boot with GRUB multiboot2?)"
  err_no_long:    .string "HALTING: Your CPU is not supported (no long mode, are you on x64?)"
  err_no_msr:     .string "HALTING: Your CPU is not supported (no MSR)"

.section .bss
  // simple stack
  .align 16
  .skip STACK_SIZE
  stack:

.section .text
.code32

.type __entry,        @function
.type __entry_fail,   @function
.type __check_vendor, @function

.global __entry

// checks if the CPU vendor is supported
__check_vendor:
  push eax // stores the return value
  push ebx // stores the supported vendor string
  push ecx // stores the CPU vendor string
  push edx // stores the current char at esi index
  push esi // stores the index for ebx
  push edi // stores the current CPU vendor string

  lea ebx, cpu_vendor_supported
  lea ecx, cpu_vendor
  xor edx, edx
  xor esi, esi

  .Lcheck_vendor_next:
    lea edi, [ebx+esi]

    push 12  // vendor string length
    push edi // supported vendor string (offset by current position)
    push ecx // CPU vendor string

    call __string_compare

    add esp, 12

    test eax, eax
    je .Lcheck_vendor_ok

    add esi, 12
    mov dh, [ebx+esi]

    test dh, dh
    je .Lcheck_vendor_fail

    jmp .Lcheck_vendor_next

  .Lcheck_vendor_fail:
    lea ecx, err_bad_vendor
    push ecx
    call __fail

  .Lcheck_vendor_ok:
    pop edi
    pop esi
    pop edx
    pop ecx
    pop ebx
    pop eax
    ret

// kernel loader entry point
__entry:
  // load the stack
  lea esp, [stack + STACK_SIZE]

  /*

   * verify the multiboot magic number
   * we have to make sure we are booted with GRUB multiboot2
   * otherwise the kernel will NOT work without the multiboot data

   * the hex value is the bootloader magic value, see the multiboot2
   * specification for details:
   * https://www.gnu.org/software/grub/manual/multiboot2/multiboot.html

  */
  mov ecx, 0x36d76289
  cmp eax, ecx
  jne .Lentry_fail_magic

  // now lets save the multiboot data
  push ebx

  // initalize video memory (pass the multiboot data as the first argument)
  call __video_init

  // obtain the kernel binary end address
  call __end_get_addr

  // detect if CPUID is available
  pushfd
  pop eax
  mov ecx, eax

  // https://en.wikipedia.org/wiki/FLAGS_register
  // 21 (ID): Able to use CPUID instruction (Pentium+)
  xor eax, 1 << 21

  push eax
  popfd

  pushfd
  pop eax

  push ecx
  popfd

  xor eax, ecx
  je .Lentry_fail_cpuid

  // obtain and check CPU vendor name
  xor eax, eax
  cpuid

  push ecx
  push edx
  push ebx

  xor eax, eax
  lea ebx, cpu_vendor
  xor ecx, ecx
  xor edx, edx

  .Lentry_read_vendor:
    pop eax

    mov byte [ebx+ecx-1], al
    mov byte [ebx+ecx], ah

    shr eax, 16

    mov byte [ebx+ecx+1], al
    mov byte [ebx+ecx+2], ah

    add ecx, 4

    inc edx
    cmp dl, 3

    jne .Lentry_read_vendor

  call __check_vendor

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
  mov eax, 1
  cpuid
  test edx, 1 << 5
  je .Lentry_fail_msr

  /*

   * check if extended CPUID is available

   * as provided in "Extended Function CPUID Information" section of
   * "Table 3-17. Information Returned by CPUID Instruction (Contd.)" (SDM Vol. 2)
   * when we call CPUID with EAX = 0x80000000, EAX is set to
   * "Maximum Input Value for Extended Function CPUID Information"

   * and for the long mode check we need at least 0x80000001

  */
  mov eax, 0x80000000
  cpuid
  cmp eax, 0x80000001
  jb .Lentry_fail_long

  // check if long mode is available
  mov eax, 0x80000001
  cpuid
  test edx, 1 << 29
  je .Lentry_fail_long

  // fast A20 (should work on all supported vendors)
  in al, 0x92
  or al, 2
  out 0x92, al

  // setup paging (also passing multiboot data as the first argument)
  call __pm_get
  mov cr3, eax

  mov eax, cr4
  or eax, 1 << 5
  mov cr4, eax

  // switch to long mode (compatibility)
  mov ecx, 0xC0000080
  rdmsr
  or eax, 1 << 8
  wrmsr

  // enable paging
  mov eax, cr0
  or eax, 1 << 31 | 1 << 0
  mov cr0, eax

  // setup GDT
  cli
  lgdt gdt_pointer

  // long jump to 64 bit mode (there's no other way to change cs)
  mov eax, gdt_desc_code_0_addr
  mov ebx, gdt_start_addr
  sub eax, ebx
  push eax

  lea eax, __entry64
  push eax

  // jmp 0x8:__entry64
  retf

.Lentry_fail_magic:
  lea eax, err_no_magic
  jmp .Lentry_fail_call

.Lentry_fail_cpuid:
  lea eax, err_no_cpuid
  jmp .Lentry_fail_call

.Lentry_fail_long:
  lea eax, err_no_long
  jmp .Lentry_fail_call

.Lentry_fail_msr:
  lea eax, err_no_msr
  jmp .Lentry_fail_call

.Lentry_fail_call:
  push eax
  call __fail

.code64
__entry64:
  // load the data segment offset
  mov eax, gdt_desc_data_0_addr
  mov ebx, gdt_start_addr
  sub eax, ebx

  // load the data segment
  mov ds, ax
  mov es, ax
  mov fs, ax
  mov gs, ax
  mov ss, ax

  // load the video memory address + multiboot data address
  pop rsi

  // clear the stack
  lea edi, [stack + STACK_SIZE]
  xor eax, eax
  mov ecx, STACK_SIZE
  rep stosd

  // reset stack
  xor rbp, rbp
  lea rsp, [stack + STACK_SIZE]

  // pass multiboot data address as the first argument
  // and video memory address as the second
  shrd rdi, rsi, 32
  shr rsi, 32
  shr rdi, 32

  // call kernel entry, should never return
  call entry
  call __halt
