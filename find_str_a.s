.global find_str_a
.func find_str_a

//r0 str, r1 substr

find_str_a:
  sub sp, sp, #8
  str r5, [sp, #4]
  str r4, [sp]
  mov r2, #0 //i
  mov r3, #0 //j

loop:
  add r4, r2, r3 //i + j
  ldrb r5, [r0, r4] // str + i + j
  ldrb r6, [r1, r3] // substr + j
  cmp r6, #0        //end of substr, find it
  beq ret2
  cmp r5, #0        //end of str, cannot find
  beq ret1
  cmp r6, r5
  addeq r3, r3, #1  //j++
  movne r3, #0      //j = 0, restart
  addne r2, r2, #1  //i++
  b loop

ret1:
  mov r0, #-1
  bx lr

ret2:
  ldr r4, [sp]
  ldr r5, [sp, #4]
  add sp, sp, #8
  mov r0, r2
  bx lr
