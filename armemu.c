#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int fib_iter_a(int x);
int fib_rec_a(int x);
int sum_array_a(int *array, int n);
int find_max_a(int *array, int n);
int find_str_a(char *s, char *sub);
int fib_iter_c(int x);
int fib_rec_c(int x);
int sum_array_c(int *array, int n);
int find_max_c(int *array, int n);
int find_str_c(char *s, char *sub);
int hello();

#define NREGS 16
#define SP 13
#define LR 14 
#define PC 15
#define STACK_SIZE 4096
// DEBUG_FLAG:
// 0 shows nothing,
// 1 shows my chosen information in developing,
// 2 shows reg state after execution of each instruction,
// -1 shows instruction analysis after execution of each instruction
#define DEBUG_FLAG (0)
// STEP_FLAG:
// 0 shows nothing,
// 1 provides single step interactive interface
#define STEP_FLAG 0

void throwError(int type) {
    switch (type) {
        case 101:
            fprintf(stderr, "Undefined instruction!\n");
            break;
        case 102:
            fprintf(stderr, "Undefined cpsr condition!\n");
            break;
        default:
            fprintf(stderr, "Undefined error!\n");
            break;
    }
    //exit(type);
}

struct arm_state {
    unsigned int regs[NREGS];
    unsigned int stack_size;
    unsigned int heap_top;
    unsigned int N, Z, C, V;
    unsigned char *stack;
    unsigned int b_count, dp_count, sdt_count;
};

struct dp_instruction {
    unsigned int opcode, cond;
    unsigned int rn, rd, I, S;
    unsigned int operand2;
};

struct sdt_instruction {
    unsigned int I, P, U, B, W, L;
    unsigned int rn, rd;
    unsigned int offset, cond;
    unsigned int operand2;
};

struct arm_state *arm_state_new(unsigned int stack_size, unsigned int *func,
                                unsigned int arg0, unsigned int arg1,
                                unsigned int arg2, unsigned int arg3) {
    struct arm_state *as;
    int i;

    as = (struct arm_state *) malloc(sizeof(struct arm_state));
    if (as == NULL) {
        printf("malloc() failed, exiting.\n");
        exit(-1);
    }

    as->stack = (unsigned char *) malloc(stack_size);
    if (as->stack == NULL) {
        printf("malloc() failed, exiting.\n");
        exit(-1);
    }
    
    as->stack_size = stack_size;

    /* Initialize all registers to zero. */
    for (i = 0; i < NREGS; i++) {
        as->regs[i] = 0;
    }

    as->regs[PC] = (unsigned int) func;
    as->regs[0] = arg0;
    as->regs[1] = arg1;
    as->regs[2] = arg2;
    as->regs[3] = arg3;
    as->N = 0;
    as->Z = 0;
    as->C = 0;
    as->V = 0;
    as->b_count = 0;
    as->sdt_count = 0;
    as->dp_count = 0;
    as->regs[SP] = stack_size;
    as->heap_top = 0;
    return as;
}

void arm_state_free(struct arm_state *as) {
    free(as->stack);
    free(as);
}

void arm_state_print(struct arm_state *as) {
    int i;
    
    printf("stack size = %d\n", as->stack_size);
    for (i = 0; i < NREGS; i++) {
        printf("regs[%d] = (%X) %d\n", i, as->regs[i], (int) as->regs[i]);
    }
}

void arm_stack_print(struct arm_state *as, int start, int end) {
    for (int i = start; i < end; i++) {
        printf("%2x", as->stack[i]);
        if (i % 32 == 31) {
            printf("\n");
        } else if (i % 4 == 3) {
            printf(" ");
        }
    }
    printf("heap_top is set as %d\n", as->heap_top);
    printf("stack pointer is set as %d\n", as->regs[SP]);
}

// utilities
unsigned int rotate_imm(unsigned int rotate, unsigned int imm) {
    if (rotate == 0)
        return imm;
    unsigned int remain = 0;
    if (rotate >= 1 && rotate < 4)
        remain = imm >> (rotate * 2);
    return (imm << ((16 - rotate) * 2)) + remain;
}

unsigned int shift(struct arm_state *as, unsigned int shift, unsigned int rm) {
    unsigned int shift_amount;
    unsigned int shift_type = (shift >> 1) & 0b11;
    unsigned int shift_imm = as->regs[rm];

    if ((shift & 0b1) == 0) {
        shift_amount = (shift >> 3) & 0b11111;
    } else {
        shift_amount = as->regs[(shift >> 4) & 0b1111];
    }
    switch (shift_type) {
        case 0b00:                  // logical left
            return shift_imm << shift_amount;
        case 0b01:                  // logical left
            return shift_imm >> shift_amount;
        case 0b10:                  // arithmetic left
            return (unsigned int) ((int) shift_imm >> shift_amount);
        case 0b11:
            return rotate_imm(shift_amount, shift_imm);
        default:
            throwError(0);
    }
}

unsigned int judge_cpsr(struct arm_state *as, unsigned int cond) {
    if (DEBUG_FLAG == 1)
        printf("given cpsr condition code as %x, while NZCV as %d%d%d%d\n", cond, as->N, as->Z, as->C, as->V);
    switch (cond) {
        case 0x0:
            if (as->Z == 1) return 1;
            break;
        case 0x1:
            if (as->Z == 0) return 1;
            break;
        case 0x2:
            if (as->C == 1) return 1;
            break;
        case 0x3:
            if (as->C == 0) return 1;
            break;
        case 0x4:
            if (as->N == 1) return 1;
            break;
        case 0x5:
            if (as->N == 0) return 1;
            break;
        case 0x6:
            if (as->V == 1) return 1;
            break;
        case 0x7:
            if (as->V == 0) return 1;
            break;
        case 0x8:
            if (as->Z == 0 && as->C == 1) return 1;
            break;
        case 0x9:
            if (as->Z == 1 || as->C == 0) return 1;
            break;
        case 0xa:                                           // GT flag
            if (as->N == as->V) return 1;
            break;
        case 0xb:
            if (as->N != as->V) return 1;
            break;
        case 0xc:
            if (as->Z == 0 && (as->N == as->V)) return 1;
            break;
        case 0xd:
            if (as->Z == 1 || (as->N != as->V)) return 1;
            break;
        case 0xe:
            return 1;
            break;
        default:
            throwError(102);
    }
    return 0;
}

void set_cpsr(struct arm_state *as, unsigned int rd, unsigned int rn, unsigned int rm) {
    unsigned int sign1 = (rn >> 31) & 0b1;
    unsigned int sign2 = (rm >> 31) & 0b1;
    as->N = ((rd >> 31) & 0b1);
    as->Z = (rd == 0 ? 1 : 0);
    as->C = (rn >= rm ? 1 : 0);
    if (as->N == 0b1 && sign1 == 0b0 && sign2 == 0b1) {
        //+ + + = - // + - - = -
        as->V = 0b1;
    } else if (as->N == 0b0 && sign1 == 0b1 && sign2 == 0b0) {
        //- + - = + // - - + = +
        as->V = 0b1;
    } else {
        as->V = 0b0;
    }
}

unsigned int address_offset(struct arm_state *as, struct sdt_instruction *sdt) {
    if (sdt->U == 0b1) {
        return as->regs[sdt->rn] + sdt->operand2;
    } else if (sdt->U == 0b0) {
        return as->regs[sdt->rn] - sdt->operand2;
    }
}

// memory I/O & allocation
unsigned int read_stack_int(struct arm_state *as, unsigned int address) {
    return as->stack[address] + (as->stack[address + 1] << 8) + (as->stack[address + 2] << 16) + (as->stack[address + 3] << 24);
}

void write_stack_int(struct arm_state *as, unsigned int address, unsigned int source) {
    as->stack[address] = (unsigned char) (source & 0xff);
    as->stack[address + 1] = (unsigned char) ((source >> 8) & 0xff);
    as->stack[address + 2] = (unsigned char) ((source >> 16) & 0xff);
    as->stack[address + 3] = (unsigned char) ((source >> 24) & 0xff);
}

unsigned int heap_alloc_char(struct arm_state *as, const unsigned char *ptr, unsigned int heap_size) {
    unsigned int address = as->heap_top;
    for (unsigned int i = 0; i < heap_size; i++) {
        as->stack[address + i] = *(ptr + i);
    }
    as->heap_top += heap_size;
    return address;
}

unsigned int heap_alloc_int(struct arm_state *as, const int *ptr, unsigned int heap_size) {
    unsigned int address = as->heap_top;
    for (unsigned int i = 0; i < heap_size; i++) {
        write_stack_int(as, address + i * 4, (unsigned int) *(ptr + i));
    }
    as->heap_top += (heap_size * 4);
    return address;
}

// 4 types: Data Processing
bool iw_is_dp_instruction(unsigned int iw) {
    unsigned int op;
    op = (iw >> 26) & 0b11;
    return (op == 0);
}

struct dp_instruction* decode_dp_instruction(struct arm_state *as, unsigned int iw) {
    struct dp_instruction *dp = (struct dp_instruction *) malloc(sizeof(struct dp_instruction));
    if (dp == NULL) {
        printf("malloc() failed, exiting.\n");
        exit(-1);
    }
    dp->opcode = (iw >> 21) & 0b1111;
    dp->cond = (iw >> 28) & 0b1111;

    dp->rd = (iw >> 12) & 0b1111;
    dp->rn = (iw >> 16) & 0b1111;
    dp->I = (iw >> 25) & 0b1;
    dp->S = (iw >> 20) & 0b1;

    if (dp->I == 0b0) {
        dp->operand2 = shift(as, (iw >> 4) & 0b11111111, iw & 0b1111);
    } else if (dp->I == 0b1) {
        dp->operand2 = rotate_imm((iw >> 8) & 0b1111, iw & 0b11111111);
    }
    return dp;
}

void dp_instruction_free(struct dp_instruction *dp) {
    free(dp);
}

void execute_add_instruction(struct arm_state *as, struct dp_instruction* dp) {
    as->regs[dp->rd] = as->regs[dp->rn] + dp->operand2;
}

void execute_sub_instruction(struct arm_state *as, struct dp_instruction* dp) {
    as->regs[dp->rd] = as->regs[dp->rn] - dp->operand2;
}

void execute_mov_instruction(struct arm_state *as, struct dp_instruction* dp) {
    as->regs[dp->rd] = dp->operand2;
}

void execute_cmp_instruction(struct arm_state *as, struct dp_instruction* dp) {
    set_cpsr(as, as->regs[dp->rn] - dp->operand2, as->regs[dp->rn], dp->operand2);
}

// 4 types: Data Processing
bool iw_is_sdt_instruction(unsigned int iw) {
    unsigned int op;
    op = (iw >> 26) & 0b11;
    return (op == 0b01);
}

struct sdt_instruction* decode_sdt_instruction(struct arm_state *as, unsigned int iw) {
    struct sdt_instruction *sdt = (struct sdt_instruction *) malloc(sizeof(struct sdt_instruction));
    if (sdt == NULL) {
        printf("malloc() failed, exiting.\n");
        exit(-1);
    }
    sdt->cond = (iw >> 28) & 0b1111;

    sdt->rd = (iw >> 12) & 0b1111;
    sdt->rn = (iw >> 16) & 0b1111;
    sdt->I = (iw >> 25) & 0b1;
    sdt->P = (iw >> 24) & 0b1;
    sdt->U = (iw >> 23) & 0b1;
    sdt->B = (iw >> 22) & 0b1;
    sdt->W = (iw >> 21) & 0b1;
    sdt->L = (iw >> 20) & 0b1;

    if (sdt->I == 0b0) {
        sdt->operand2 = iw & 0b111111111111;
    } else if (sdt->I == 0b1) {
        sdt->operand2 = shift(as, (iw >> 4) & 0b11111111, iw & 0b1111);
    }
    return sdt;
}

void execute_sdt_instruction(struct arm_state *as, struct sdt_instruction *sdt) {
    unsigned int address = as->regs[sdt->rn];
    if (sdt->P == 0b1) {
        address = address_offset(as, sdt);
        //printf("pre-assigning address: %d\n", address);
    }
    if (sdt->L == 0b1) {                // load
        if (sdt->B == 0b1) {            // by byte
            as->regs[sdt->rd] = as->stack[address];
        } else if (sdt->B == 0b0) {     // by word
            as->regs[sdt->rd] = read_stack_int(as, address);
        }
    } else if (sdt->L == 0b0) {         // store
        if (sdt->B == 0b1) {            // by byte
            as->stack[address] = (unsigned char) (as->regs[sdt->rd] & 0xff);
        } else if (sdt->B == 0b0) {     // by word
            write_stack_int(as, address, as->regs[sdt->rd]);
        }
    }
    if (sdt->P == 0b0) {
        address = address_offset(as, sdt);
        //printf("post-assigning address: %d\n", address);
    }
    if (sdt->W == 0b1 && (sdt->cond == 0b1110 || judge_cpsr(as, sdt->cond))) {
        as->regs[sdt->rn] = address;
    }
}

void sdt_instruction_free(struct sdt_instruction *sdt) {
    free(sdt);
}

// 4 types: branch & exchange
bool iw_is_bx_instruction(unsigned int iw) {
    return ((iw >> 4) & 0xFFFFFF) == 0b000100101111111111110001;
}

void execute_bx_instruction(struct arm_state *as, unsigned int iw) {
    unsigned int rn = iw & 0b1111;
    as->regs[PC] = as->regs[rn];
}

// 4 types: branch or branch & link
bool iw_is_b_instruction(unsigned int iw) {
    return ((iw >> 25) & 0b111) == 0b101;
}

void execute_b_instruction(struct arm_state *as, unsigned int iw) {
    unsigned int cond = (iw >> 28) & 0b1111;
    unsigned int L = (iw >> 24) & 0b1;

    if (cond == 14 || judge_cpsr(as, cond)) {
        if (L == 1) {
            as->regs[LR] = as->regs[PC] + 4;
        }
        unsigned int offset = iw & 0xffffff;
        if (offset >= 0x800000)
            offset += 0xff000000;
        if (DEBUG_FLAG == 1)
            printf("offset is set as %d\n", ((int) offset << 2));
        as->regs[PC] = as->regs[PC] + ((int) offset << 2) + 8;
    } else {
        as->regs[PC] = as->regs[PC] + 4;
    }
}

void arm_state_execute_one(struct arm_state *as, unsigned int debug_flag) {
    unsigned int iw;
    unsigned int *pc;

    pc = (unsigned int *) as->regs[PC];
    iw = *pc;

    if (iw_is_bx_instruction(iw)) {
        as->b_count ++;
        if (debug_flag == 1)
            printf("===========on %d instruction, executing branch exchange instruction\n", as->regs[PC]);
        execute_bx_instruction(as, iw);
    } else if (iw_is_b_instruction(iw)) {
        as->b_count ++;
        if (debug_flag == 1)
            printf("===========on %d instruction, executing branch / branch link instruction\n", as->regs[PC]);
        execute_b_instruction(as, iw);
    } else if (iw_is_dp_instruction(iw)) {
        as->dp_count ++;
        struct dp_instruction *dp = decode_dp_instruction(as, iw);
        if (dp->cond == 0b1110 || judge_cpsr(as, dp->cond)) {
            switch (dp->opcode) {
                case 0b0010:
                    if (debug_flag == 1)
                        printf("===========on %d instruction, executing sub instruction\n", as->regs[PC]);
                    execute_sub_instruction(as, dp);
                    break;
                case 0b0100:
                    if (debug_flag == 1)
                        printf("===========on %d instruction, executing add instruction\n", as->regs[PC]);
                    execute_add_instruction(as, dp);
                    break;
                case 0b1101:
                    if (debug_flag == 1)
                        printf("===========on %d instruction, executing mov instruction\n", as->regs[PC]);
                    execute_mov_instruction(as, dp);
                    break;
                case 0b1010:
                    if (debug_flag == 1)
                        printf("===========on %d instruction, executing cmp instruction\n", as->regs[PC]);
                    execute_cmp_instruction(as, dp);
                    break;
                default:
                    throwError(101);
            }
        }
        as->regs[PC] = as->regs[PC] + 4;
        dp_instruction_free(dp);
    } else if (iw_is_sdt_instruction(iw)) {
        as->sdt_count ++;
        if (debug_flag == 1)
            printf("===========on %d instruction, executing sdt instruction\n", as->regs[PC]);
        struct sdt_instruction *sdt = decode_sdt_instruction(as, iw);
        execute_sdt_instruction(as, sdt);
        as->regs[PC] = as->regs[PC] + 4;
        sdt_instruction_free(sdt);
    } else {
        if (debug_flag == 1)
            printf("===========on %d instruction, undefined instruction %x\n", as->regs[PC], iw);
    }
    if (DEBUG_FLAG == -1) {
        printf("====================================\n");
        printf("| %d instructions have been executed\n", as->b_count + as->dp_count + as->sdt_count);
        printf("| %d of them is branch instruction\n", as->b_count);
        printf("| %d of them is data processing instruction\n", as->dp_count);
        printf("| %d of them is single data transfer instruction\n", as->sdt_count);
        printf("====================================\n");
    }
    if (DEBUG_FLAG == 2)
        arm_state_print(as);
    if (STEP_FLAG == 1) {
        printf("Press any key to the next instruction...");
        getchar();
    }
}

unsigned int arm_state_execute(struct arm_state *as, unsigned int debug_flag) {
    while (as->regs[PC] != 0) {
        arm_state_execute_one(as, debug_flag);
    }
    printf("====================================\n");
    printf("| %d instructions have been executed\n", as->b_count + as->dp_count + as->sdt_count);
    printf("| %d of them is branch instruction\n", as->b_count);
    printf("| %d of them is data processing instruction\n", as->dp_count);
    printf("| %d of them is single data transfer instruction\n", as->sdt_count);
    printf("====================================\n");
    printf("The final ARM register state is:\n");
    printf("====================================\n");
    arm_state_print(as);
    printf("====================================\n");
    return as->regs[0];
}

void test_fib_iter_a(unsigned int x) {
    struct arm_state *as;
    unsigned int rv;

    printf("My asm codes in Project 2 of fib_iter(%d)\n", x);
    as = arm_state_new(STACK_SIZE, (unsigned int *) fib_iter_a, x, 0, 0, 0);
    rv = arm_state_execute(as, DEBUG_FLAG);
    printf("rv = %d\n", rv);
    printf("====================================\n");
}

void test_fib_rec_a(unsigned int x) {
    struct arm_state *as;
    unsigned int rv;

    printf("My asm codes in Project 2 of fib_rec(%d)\n", x);
    as = arm_state_new(STACK_SIZE, (unsigned int *) fib_rec_a, x, 0, 0, 0);
    rv = arm_state_execute(as, DEBUG_FLAG);
    printf("rv = %d\n", rv);
    printf("====================================\n");
}

void test_find_str_a() {
    struct arm_state *as;
    unsigned int rv;

    unsigned char *haystack = (unsigned char *) calloc(10, sizeof(char));
    unsigned char *needle = (unsigned char *) calloc(3, sizeof(char));

    strcpy((char *)haystack, "atbbcwwtz");
    strcpy((char *)needle, "cw");

    printf("My asm codes in Project 2 of find_str(\"atbbcwwtz\", \"cw\")\n");
    as = arm_state_new(STACK_SIZE, (unsigned int *) find_str_a, 0, 10, 0, 0);
    heap_alloc_char(as, haystack, strlen((char *)haystack) + 1);
    heap_alloc_char(as, needle, strlen((char *)needle) + 1);

    rv = arm_state_execute(as, DEBUG_FLAG);
    printf("rv = %d\n", rv);
    printf("====================================\n");
}

void test_sum_array_a() {
    struct arm_state *as;
    unsigned int rv;

    int *array = (int *) calloc(5, sizeof(int));
    array[0] = 20;
    array[1] = 45;
    array[2] = 37;
    array[3] = 24;
    array[4] = 13;

    printf("My asm codes in Project 2 of sum_array() in [20, 45, 37, 24, 13]\n");
    as = arm_state_new(STACK_SIZE, (unsigned int *) sum_array_a, 0, 5, 0, 0);
    int ptr = heap_alloc_int(as, array, 5);  // it's actually 0
    rv = arm_state_execute(as, DEBUG_FLAG);
    printf("rv = %d\n", rv);
    printf("====================================\n");
}

void test_find_max_a() {
    struct arm_state *as;
    unsigned int rv;

    int *array = (int *) calloc(5, sizeof(int));
    array[0] = 20;
    array[1] = 45;
    array[2] = 37;
    array[3] = 24;
    array[4] = 13;

    printf("My asm codes in Project 2 of find_max() in [20, 45, 37, 24, 13]\n");
    as = arm_state_new(STACK_SIZE, (unsigned int *) find_max_a, 0, 5, 0, 0);
    int ptr = heap_alloc_int(as, array, 5);  // it's actually 0
    rv = arm_state_execute(as, DEBUG_FLAG);
    printf("rv = %d\n", rv);
    printf("====================================\n");
}

void test_fib_iter_c(unsigned int x) {
    struct arm_state *as;
    unsigned int rv;

    printf("The GCC compiled asm codes of fib_iter(%d)\n", x);
    as = arm_state_new(STACK_SIZE, (unsigned int *) fib_iter_c, x, 0, 0, 0);
    rv = arm_state_execute(as, DEBUG_FLAG);
    printf("rv = %d\n", rv);
    printf("====================================\n");
}

void test_fib_rec_c(unsigned int x) {
    struct arm_state *as;
    unsigned int rv;

    as = arm_state_new(STACK_SIZE, (unsigned int *) fib_rec_c, x, 0, 0, 0);
    rv = arm_state_execute(as, DEBUG_FLAG);
    printf("rv = %d\n", rv);
}

void test_find_str_c() {
    struct arm_state *as;
    unsigned int rv;

    unsigned char *haystack = (unsigned char *) calloc(10, sizeof(char));
    unsigned char *needle = (unsigned char *) calloc(3, sizeof(char));

    strcpy((char *)haystack, "atbbcwwtz");
    strcpy((char *)needle, "cw");

    printf("The GCC compiled asm codes of find_str(\"atbbcwwtz\", \"cw\")\n");
    as = arm_state_new(STACK_SIZE, (unsigned int *) find_str_c, 0, 10, 0, 0);
    heap_alloc_char(as, haystack, strlen((char *)haystack) + 1);
    heap_alloc_char(as, needle, strlen((char *)needle) + 1);

    rv = arm_state_execute(as, DEBUG_FLAG);
    printf("rv = %d\n", rv);
    printf("====================================\n");
}

void test_sum_array_c() {
    struct arm_state *as;
    unsigned int rv;

    int *array = (int *) calloc(5, sizeof(int));
    array[0] = 20;
    array[1] = 45;
    array[2] = 37;
    array[3] = 24;
    array[4] = 13;

    printf("The GCC compiled asm codes of sum_array() in [20, 45, 37, 24, 13]\n");
    as = arm_state_new(STACK_SIZE, (unsigned int *) sum_array_c, 0, 5, 0, 0);
    int ptr = heap_alloc_int(as, array, 5);  // it's actually 0
    rv = arm_state_execute(as, DEBUG_FLAG);
    printf("rv = %d\n", rv);
    printf("====================================\n");
}

void test_find_max_c() {
    struct arm_state *as;
    unsigned int rv;

    int *array = (int *) calloc(5, sizeof(int));
    array[0] = 20;
    array[1] = 45;
    array[2] = 37;
    array[3] = 24;
    array[4] = 13;

    printf("The GCC compiled asm codes of find_max() in [20, 45, 37, 24, 13]\n");
    as = arm_state_new(STACK_SIZE, (unsigned int *) find_max_c, 0, 5, 0, 0);
    int ptr = heap_alloc_int(as, array, 5);  // it's actually 0
    rv = arm_state_execute(as, DEBUG_FLAG);
    printf("rv = %d\n", rv);
    printf("====================================\n");
}

void test_hello() {
    struct arm_state *as;
    unsigned int rv;

    as = arm_state_new(STACK_SIZE, (unsigned int *) hello, 0, 0, 0, 0);
    rv = arm_state_execute(as, DEBUG_FLAG);
    arm_state_print(as);
    printf("rv = %d\n", rv);
}

int main(int argc, char **argv) {
    printf("====================================\n");
    test_fib_iter_a(20);
    printf("\n");
    printf("\n");
    printf("====================================\n");
    test_fib_rec_a(20);
    printf("\n");
    printf("\n");
    printf("====================================\n");
    test_find_str_a();
    printf("\n");
    printf("\n");
    printf("====================================\n");
    test_sum_array_a();
    printf("\n");
    printf("\n");
    printf("====================================\n");
    test_find_max_a();
    printf("\n");
    printf("\n");
    printf("====================================\n");

    test_fib_iter_c(20);
    printf("\n");
    printf("\n");
    printf("====================================\n");
    //test_fib_rec_c(10);
    test_find_str_c();
    printf("\n");
    printf("\n");
    printf("====================================\n");
    test_sum_array_c();
    printf("\n");
    printf("\n");
    printf("====================================\n");
    test_find_max_c();
    //test_hello();
    return 0;
}
