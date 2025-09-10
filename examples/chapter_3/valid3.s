.globl main
main:
  pushq %rbp
  movq %rsp, %rbp
  subq $16, %rsp
  movl $7, %eax
  movl %eax, %ecx
  movl $4, %eax
  xchgl %eax, %ecx
  cltd
  idivl %ecx
  movl %edx, %eax
  movl %eax, -4(%rbp)
  movl -4(%rbp), %eax
  leave
  ret
