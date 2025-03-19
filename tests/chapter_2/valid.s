.global main
main:
  movl $2, %eax
  negl %eax
  notl %eax
  ret
