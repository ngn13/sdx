.section .text
.code64

.type syscall, @function
.global syscall

syscall:
  mov %rdi, %rax // rdi = syscall number
  mov %rsi, %rdi // rsi = first argument
  mov %rdx, %rsi // rdx = second argument
  mov %rcx, %rdx // rcx = third argument
  mov %r8, %rcx  // r8  = fourth argument
  syscall
  ret
