CC := gcc

########## YOUR MAKEFILE BELOW ##########
CFLAGS := -g -O2 -Wall -Werror
EXE := call_wrr_info gen_cpu_proc gen_io_proc weight_test fork_many

all: weight_test fork_many call_wrr_info gen_cpu_proc gen_io_proc

weight_test: weight_test.c
	$(CC) $(CFLAGS) -o weight_test weight_test.c

fork_many: fork_many.c
	$(CC) $(CFLAGS) -o fork_many fork_many.c

call_wrr_info: call_wrr_info.c
	$(CC) $(CFLAGS) -o call_wrr_info call_wrr_info.c

gen_cpu_proc: gen_cpu_proc.c
	$(CC) $(CFLAGS) -o gen_cpu_proc gen_cpu_proc.c

gen_io_proc: gen_io_proc.c
	$(CC) $(CFLAGS) -o gen_io_proc gen_io_proc.c

clean:
	rm -f $(EXE)
