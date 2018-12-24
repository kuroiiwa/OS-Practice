#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <stdlib.h>

#define SYS_ptree 326
#define BUF_SIZE 300

struct prinfo {
	pid_t parent_pid;
	pid_t pid;
	pid_t first_child_pid;
	pid_t next_sibling_pid;
	long state;
	uid_t uid;
	char comm[64];
};

int main(int argc, char **argv)
{
	struct prinfo *buf;
	struct prinfo p;
	int nr;
	int a[10000];

	buf = (struct prinfo *)malloc(sizeof(struct prinfo) * BUF_SIZE);
	if (buf == NULL)
		printf("error: %s\n", "allocate error");
	nr = BUF_SIZE;
	long res = syscall(SYS_ptree, buf, &nr);
	pid_t q;

	if (nr > BUF_SIZE)
		nr = BUF_SIZE;
	for (int i = 0; i < nr; i++) {
		p = buf[i];
		a[p.pid] = p.parent_pid;
	}
	for (int i = 0; i < nr; i++) {
		p = buf[i];
		q = p.parent_pid;
		while (q != 0) {
			q = a[q];
			printf("        ");
		}
		if (p.pid != 0)
			printf("        ");
		printf("%s,%d,%ld,%d,%d,%d,%u\n", p.comm, p.pid, p.state,
p.parent_pid, p.first_child_pid, p.next_sibling_pid, p.uid);
	}
	free(buf);
	return res;
}
