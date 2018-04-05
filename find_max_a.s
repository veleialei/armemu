.global find_max_a
.func find_max_a

find_max_a:
  sub sp, sp, #8
  str r5, [sp, #4]
  str r4, [sp]
  mov r2, #0 //i
  ldr r3, [r0] //max

loop:
  cmp r2, r1
  bge done
  lsl r4, r2, #2
  ldr r5, [r0, r4]
  cmp r5, r3
  movgt r3, r5
  str r5, [r0, r4]
  add r2, r2, #1
  b loop

done:
  ldr r5, [sp, #4]
  ldr r4, [sp]
  mov r0, r3
  bx lr
