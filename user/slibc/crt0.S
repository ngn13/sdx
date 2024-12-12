.intel_syntax noprefix

/*

 * crt0.S is used to create crt0.o, which is the startfile
 * for the programs that use slibc

 * so _start() gets called when the ELF binary is run,
 * which initliazies slibc, then calls main()

 * to be clear, i obv didn't come up with this:
 * https://en.wikipedia.org/wiki/Crt0

*/

.section .text

// see slibc.c
.extern _slibc_start
.extern _slibc_end

// should be defined by the program
.extern main

// entrypoint defined here
.global _start

_start:
  // initalize slibc internal stuff
  call _slibc_start

  // call main and save result
  call main
  mov rbx, rax

  // cleanup slibc internal stuff
  call _slibc_end

  // return saved result
  mov rax, rbx
  ret
