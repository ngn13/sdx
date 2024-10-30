.intel_syntax noprefix

.section .text
.code32

.type __string_compare, @function
.type __error,          @function
.type __fail,           @function
.type __halt,           @function

.global __string_compare
.global __fail
.global __halt

// halts the system forever
__halt:
  cli

  .Lentry_halt_forever:
    hlt
    jmp .Lentry_halt_forever

/* 

 * accepts one argument, which is the error message
 * the message is provided to __error from util.s which will print it to the screen
 * after printing the error, it will halt the system

*/
__fail:
  mov eax, [esp+4] // get the error message
  // no need to preserve eax because this function never returns

  push eax     // set is as the first argument
  call __error // call __error to print it out
  add esp, 4   // remove the argument from the stack

  call __halt

// compares two strings
__string_compare:
  push ebx // preserve ebx (stores the first string)
  push ecx // preserve ecx (stores the second string)
  push edx // preserve edx (stores the string length)

  xor eax, eax // return value

  mov ebx, [esp+16]
  mov ecx, [esp+20]
  mov edx, [esp+24]

  .Lstring_compare_char:
    mov ah, [ebx+edx-1]
    mov al, [ecx+edx-1]

    cmp ah, al
    jne .Lstring_compare_fail

    dec edx

    test edx, edx
    jne .Lstring_compare_char

    xor eax, eax
    jmp .Lstring_compare_end

  .Lstring_compare_fail:
    mov eax, 1

  .Lstring_compare_end:
    pop edx
    pop ecx
    pop ebx

    ret

// prints a given error message
__error:
  push eax
  mov eax, [esp+8] // get the error message

  push edx
  mov edx, 0x0c    // get the color

  push edx
  push eax
  call __video_write

  add esp, 8

  // hide the cursor
  mov dx, 0x3D4
	mov al, 0xA
	out dx, al

	inc dx
	mov al, 0x20
	out dx, al

  // restore registers and return
  pop edx
  pop eax

  ret
