.global fib_iter_a
.func fib_iter_a


fib_iter_a:
  sub sp, sp, #8
  str r4, [sp]
  mov r1, #0 //i
  mov r2, #0 //return value
  mov r3, #1 //first
  mov r4, #1 //second

loop:
  cmp r1, r0
  bge done
  cmp r1, #1
  movle r2, #1
  add r1, r1, #1
  ble loop
  bgt fib

fib:
  add r2, r3, r4
  mov r3, r4
  mov r4, r2
  b loop

done:
  ldr r4, [sp]
  add sp, sp, #8
  mov r0, r2
  bx lr
