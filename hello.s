    /* hello.s */
    .data
greeting:
    .asciz "Hello, World!\n"

    .text
    .global hello
    .func hello

hello:
    sub sp, sp, #8
    str lr, [sp]

    ldr r0, =greeting
    bl printf

    ldr lr, [sp]
    add sp, sp, #8
    bx lr

    .endfunc
