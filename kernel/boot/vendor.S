.type cpu_vendor,     @common
.type __vendor_get,   @function
.type __vendor_check, @function

.global cpu_vendor
.global __vendor_check

.section .data

cpu_vendor:           .string "not detected"
err_bad_vendor:       .string "HALTING: Your CPU is not supported (unknown vendor)"
cpu_vendor_supported: .string "AuthenticAMDAMDisbetter!GenuineIntelTCGTCGTCGTCG KVMKVMKVM  VMwareVMwareVBoxVBoxVBox"

.section .text
.code32

// obtain the CPU vendor using CPUID
__vendor_get:
  push %ebx
  push %ecx
  push %edx

  /*

   * calling cpuid with eax = 0 will return the vendor ID
   * the ID will be copied into ebx, ecx and edx, with the
   * following format:

   *       MSB         LSB
   * ebx = 'u' 'n' 'e' 'G'
   * edx = 'I' 'e' 'n' 'i'
   * ecx = 'l' 'e' 't' 'n'

   * here the vendor ID is "GenuineIntel", which is the vendor
   * ID for the Intel CPUs

  */
  xor %eax, %eax
  cpuid

  mov $cpu_vendor, %eax
  movl %ebx, 0(%eax)
  movl %edx, 4(%eax)
  movl %ecx, 8(%eax)

  pop %edx
  pop %ecx
  pop %ebx

  ret

// checks if the CPU vendor is supported
__vendor_check:
  push %ebx
  push %ecx
  push %edx

  // first obtain the vendor name with CPUID
  call __vendor_get

  mov $cpu_vendor_supported, %ebx
  xor %ecx, %ecx

  .Lvendor_check_next:
    // edx will point to the start of the next supported vendor ID
    mov %ebx, %edx
    add %ecx, %edx

    push $12  // vendor ID length
    push %edx // supported vendor string
    push %eax // vendor string

    call __cmpn

    add $12, %esp // remove arguments from the stack

    test %eax, %eax
    je .Lvendor_check_ok

    // move onto the enxt supported vendor ID
    add $12, %ecx

    // check if the previous ID was the last one
    mov (%ebx,%ecx,1), %dh
    test %dh, %dh
    jne .Lvendor_check_next

  mov $err_bad_vendor, %eax
  push %eax
  call __fail

  .Lvendor_check_ok:
    pop %edx
    pop %ecx
    pop %ebx

    ret
