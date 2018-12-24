#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_CPUS 6 /* We will be testing only on the VMs */
#define __NR_get_wrr_info 326
#define __NR_set_wrr_weight 327
#define __NR_setscheduler 144
#define N 1874919423


struct wrr_info {
	int num_cpus;
	int nr_running[MAX_CPUS];
	int total_weight[MAX_CPUS];
};
struct sched_param {
	int sched_priority;
};
void trial_division(int n, int io)
{
	int f = 2;

	while (n > 1) {
		if (n % f == 0) {
			if (io)
				printf("%d", f);
			n /= f;
		} else
			f += 1;
	}
}

int main(int argc, char **argv)
{
	struct sched_param param;
	pid_t pid = getpid();

	param.sched_priority = 0;

	syscall(__NR_setscheduler, pid, 7, &param);

	for (int i = 0; i < 100; ++i) {
		pid_t pid = fork();

		if (pid < 0)
			return -1;

		if (pid == 0) {
			trial_division(N, 0);
			exit(0);
		} else {
			wait(NULL);
		}
	}


	return 0;
}
