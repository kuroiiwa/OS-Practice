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
	struct wrr_info info;
	struct sched_param param;
	pid_t pid = getpid();
	int weight = atoi(argv[1]);
	int res;

	param.sched_priority = 0;

	syscall(__NR_setscheduler, pid, 7, &param);
	syscall(__NR_set_wrr_weight, weight);
	trial_division(N, 0);

	res = syscall(__NR_get_wrr_info, &info);
	if (!res) {
		printf("num_cpus: %d\n", info.num_cpus);
		for (int i = 0; i < info.num_cpus; i++) {
			printf("CPU_%d: %d %d\n", i, info.nr_running[i],
					info.total_weight[i]);
		}
	}
	return 0;
}
