PROGS = armemu
OBJS =

CFLAGS = -g

all : ${PROGS}

armemu : armemu.c fib_iter_a.s fib_rec_a.s find_str_a.s find_max_a.s sum_array_a.s hello.s fib_iter_c.s fib_rec_c.s find_str_c.s find_max_c.s sum_array_c.s
	gcc ${CFLAGS} -o armemu armemu.c fib_iter_a.s fib_rec_a.s find_str_a.s find_max_a.s sum_array_a.s hello.s fib_iter_c.s fib_rec_c.s find_str_c.s find_max_c.s sum_array_c.s

clean:
	rm -rf ${PROGS} ${OBJS}
