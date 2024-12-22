/*

 * crt0.S is used to create crt0.o, which is the startfile
 * for the programs that use slibc

 * so _start() gets called when the ELF binary is run,
 * which initliazies slibc, then calls main()

 * to be clear, i obv didn't come up with this:
 * https://en.wikipedia.org/wiki/Crt0

*/

.section .text

.type _slibc_start, @function // defined in slibc.c
.type _slibc_end, @function // defined in slibc.c
.type main, @function // defined by the program
.type _start, @function

.extern _slibc_start
.extern _slibc_end
.extern main
.global _start

// here comes the entrypoint
_start:
  // pop the argv and envp
  pop %rsi
  pop %rdx

  // calculate argc
  xor %rdi, %rdi
  mov %rsi, %rbx

  .Lstart_argc_loop:
    mov (%rbx), %rax

    add $8, %rbx
    inc %rdi

    test %rax, %rax
    jne .Lstart_argc_loop

  dec %rdi

  // initalize slibc internal stuff
  call _slibc_start

  // call main and save return value
  call main
  mov %rax, %rdi

  // cleanup slibc internal stuff
  call _slibc_end

  // exit with the return value
  xor %rax, %rax
  syscall
