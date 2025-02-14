.type __halt,  @function
.type __fail,  @function
.type __cmpn,  @function
.type __error, @function

.global __cmpn
.global __fail
.global __halt

.section .text
.code32

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
  // no need to preserve any registers because this function never returns
  mov 4(%esp), %eax // obtain the error message

  push %eax    // set is as the first argument
  call __error // call __error to print it out
  // no need to remove the argument from the stack as well

  call __halt  // halt forever

// compare specific amount of chars from two strings
__cmpn:
  push %ebx // preserve ebx (will store the first string)
  push %ecx // preserve ecx (will store the second string)
  push %edx // preserve edx (will store the string length)

  mov $1, %eax // return value
  mov 16(%esp), %ebx // get the first string in ebx
  mov 20(%esp), %ecx // get the second string in ecx
  mov 24(%esp), %edx // and get the length in edx

  .Lcmpn_char:
    mov -1(%ebx,%edx,1), %ah
    mov -1(%ecx,%edx,1), %al

    cmp %ah, %al
    jne .Lcmpn_end

    dec %edx

    test %edx, %edx
    jne .Lcmpn_char

    xor %eax, %eax // change return value

  .Lcmpn_end:
    pop %edx
    pop %ecx
    pop %ebx

    ret

// prints a given error message
__error:
  push %eax
  push %edx

  mov 12(%esp), %eax // get the message

  push $0xc // color
  push %eax // message
  call __video_write // write to screen

  add $8, %esp // remove args from the stack

  // hide the cursor
  mov $0x3d4, %dx
	mov $0xa, %al
	out %al, %dx

	inc %dx
	mov $0x20, %al
	out %al, %dx

  // restore registers and return
  pop %edx
  pop %eax

  ret
