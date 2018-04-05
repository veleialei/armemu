.global fib_rec_a
.func fib_rec_a

fib_rec_a:
    sub sp, sp, #16     //create space to store data
    str r4, [sp, #12]   //
    str lr, [sp, #8]    //store return address
    str r0, [sp, #4]    //store return value

    cmp r0, #0          // = 0
    beq fib_0
    cmp r0, #1          // = 1
    beq fib_1
    b fib_else          // loop

fib_0:                    //if n = 0
    mov r3, #0
    b fib_ret

fib_1:                    //if n = 1
    mov r3, #1
    b fib_ret

fib_else:
    ldr r3, [sp, #4]      //let r3 = n
    sub r3, r3, #1        //n-1
    mov r0, r3            //store n-1
    bl fib_rec_a
    mov r4, r0            //store previous answer

    ldr r3, [sp, #4]      //let r3 = n
    sub r3, r3, #2        //n-2
    mov r0, r3            //store n-2
    bl fib_rec_a
    mov r3, r0            //store previous answer

    add r3, r4, r3        //calculate f(n-1) + f(n-2)
    b fib_ret

fib_ret:
    mov r0, r3
    ldr r4, [sp, #12]
    ldr lr, [sp, #8]
    add sp, sp, #16       //go back
    bx lr
