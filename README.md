# OS-Practice
## A simple shell in C
This baby shell supports:\
execution\
exit\
cd [dir]\
history [-c] [number]\
!!\
!\
pipes such as /bin/ls | /usr/bin/wc

## A new system call in Linux
The system call will take two arguments and return the process tree information in a depth-first-search (DFS) order. \
prototype:
```
int ptree(struct prinfo *buf, int *nr);
```
structure definition:
```
struct prinfo {
	pid_t parent_pid;		/* process id of parent */
	pid_t pid;			/* process id */
	pid_t first_child_pid;  	/* pid of youngest child */
	pid_t next_sibling_pid;  	/* pid of older sibling */
	long state;			/* current state of process */
	uid_t uid;			/* user id of process owner */
	char comm[64];			/* name of program executed */
};
```
Syscall ptree works like this:
```
swapper/0,0,0,0,2,0,0
	kthreadd,2,1,0,1794,1,0
		kworker/0:2,1794,1,2,0,1677,0
		kworker/1:2,1677,1,2,0,1657,0
		...
		kworker/0:0,4,1,2,0,3,0
		ksoftirqd/0,3,1,2,0,0,0
	init,1,1,0,2695,0,0
		adbd,2695,1,1,2804,2087,0
			sh,2804,1,2695,2820,0,0
				test,2820,0,2804,0,0,0
		wpa_supplicant,2087,1,1,0,1857,1010
		...
		main,1844,1,1,2776,1843,0
			.quicksearchbox,2776,1,1844,0,2759,10061
			android.traceur,2759,1,1844,0,2724,10058
			...
			putmethod.latin,2018,1,1844,0,1896,10065
			system_server,1896,1,1844,0,0,1000
		...
		logd,1492,1,1,0,1145,1036
		ueventd,1145,1,1,0,1144,0
		init,1144,1,1,0,1143,0
		init,1143,1,1,0,0,0
 ```
 ## A user-space daemon to pass device orientation into the kernel & Orientation-based synchronization mechanism
 information structure:
 ```
 /*
 * Sets current device orientation in the kernel.
 * System call number 326.
 */
int set_orientation(struct dev_orientation *orient);

struct dev_orientation {
	int azimuth; /* angle between the magnetic north and the Y axis, around the
		      * Z axis (-180<=azimuth<=180)
		      */
	int pitch;   /* rotation around the X-axis: -90<=pitch<=90 */
	int roll;    /* rotation around Y-axis: +Y == -roll, -180<=roll<=180 */
};
```

API description:
```
/*
 * Create a new orientation event using the specified orientation range.
 * Return an event_id on success and appropriate error on failure.
 * System call number 327.
 */
int orientevt_create(struct orientation_range *orient);

/*
 * Destroy an orientation event and notify any processes which are
 * currently blocked on the event to leave the event.
 * Return 0 on success and appropriate error on failure.
 * System call number 328.
 */
int orientevt_destroy(int event_id);

/*
 * Block a process until the given event_id is notified. Verify that the
 * event_id is valid.
 * Return 0 on success and appropriate error on failure.
 * System call number 329.
 */
int orientevt_wait(int event_id);

struct orientation_range {
	struct dev_orientation orient;  /* device orientation */
	unsigned int azimuth_range;     /* +/- degrees around Z-axis */
	unsigned int pitch_range;       /* +/- degrees around X-axis */
	unsigned int roll_range;        /* +/- degrees around Y-axis */
};

/* Helper function to determine whether an orientation is within a range. */
static __always_inline bool orient_within_range(struct dev_orientation *orient,
						struct orientation_range *range)
{
	struct dev_orientation *target = &range->orient;
	unsigned int azimuth_diff = abs(target->azimuth - orient->azimuth);
	unsigned int pitch_diff = abs(target->pitch - orient->pitch);
	unsigned int roll_diff = abs(target->roll - orient->roll);

	return (!range->azimuth_range || azimuth_diff <= range->azimuth_range
			|| 360 - azimuth_diff <= range->azimuth_range)
		&& (!range->pitch_range || pitch_diff <= range->pitch_range)
		&& (!range->roll_range || roll_diff <= range->roll_range
			|| 360 - roll_diff <= range->roll_range);
}
```

## A new Linux scheduler, a CPU Affinity Weight Round-Robin scheduler with load-balancing feature
The algorithm should run in constant time and work as follows:
1. The new scheduling policy should serve as the default scheduling policy for init and all of its descendants.
2. Multicore systems must be fully supported.
3. Every task has a time slice (quantum), which is a multiple of a base 10ms time slice.
4. The multiple should be the weight of the process which can be set using Linux scheduling system calls.
5. When deciding which CPU a task should be assigned to, it should be assigned to the CPU with the least total weight. This means you have to keep track of the total weight of all the tasks running on each CPU. If there are multiple CPUs with the same weight, you should assign the task to the CPU with higher affinity, where affinity is measured by the number of other tasks assigned to the CPU that are part of the same thread group.
debugging functions:
```
#define MAX_CPUS 8 /* We will be testing only on the VMs */
struct wrr_info {
	
	int num_cpus;
	int nr_running[MAX_CPUS];
	int total_weight[MAX_CPUS];
}

/* This system call will fill the buffer with the required values 
 * i.e. the total number of CPUs, the number of WRR processes on each 
 * CPU and the total weight of WRR processes on each CPU. 
 * Return value will also be the total number of CPUs.
 * System call number 326.
 */ 
int get_wrr_info(struct *wrr_info);

/* This system call will change the weight from the default
 * value of 10.  Only a root user should be able to increase the weight.
 * The system call should return an error for any weight less than 1.
 * System call number 327.
 */ 
int set_wrr_weight(int weight);
```
## Inspect Process Address Space by User Space Mapped Page Tables & Investigate Linux Process Address Space
Obtaining the Mapping:
```
/*
 * Investigate the page table layout.
 *
 * The Page table layout varies over different architectures (eg. x86 vs arm).
 * It also changes with the change of system configuration. For example, setting
 * transparent_hugepage extends the pagesize from 4kb to 64kb. As a result,
 * indexing with Virtual Address in page table walking differs from the case
 * with 4k page size.
 *
 * You need to implement a system call to get the page table layout information
 * of the current system. Use syscall number 326.
 * @pgtbl_info : user address to store the related infomation
 * @size : the memory size reserved for pgtbl_info
 */
 struct pagetable_layout_info {
        uint32_t pgdir_shift;
        uint32_t pud_shift;
        uint32_t pmd_shift;
        uint32_t page_shift;
 };

int get_pagetable_layout(struct pagetable_layout_info __user *pgtbl_info,
                         int size);
```
```
/*
 * Map a target process's page table into the current process's address space.
 * Use syscall number 327.
 *
 * After successfully completing this call, page_table_addr will contain part of
 * page tables of the target process. To make it efficient for referencing the
 * re-mapped page tables in user space, your syscall is asked to build a fake
 * PGD, fake PUDs and fake PMDs. The fake PGD will be indexed by pgd_index(va),
 * the fake PUD will be indexed by pud_index(va), and the fake PMD will be
 * indexed by pmd_index(va). (where va is a given virtual address).
 *
 * @pid: pid of the target process you want to investigate, if pid == -1, you
 *       should dump the current process's page tables
 * @fake_pgd: base address of the fake PGD
 * @fake_puds: base address of the fake PUDs
 * @fake_pmds: base address of the fake PMDs
 * @page_table_addr: base address in user space the PTEs mapped to
 * [@begin_vaddr, @end_vaddr): remapped memory range of the target process
 */
struct expose_pgtbl_args {
        unsigned long fake_pgd;
        unsigned long fake_puds;
        unsigned long fake_pmds;
        unsigned long page_table_addr;
        unsigned long begin_vaddr;
        unsigned long end_vaddr;
};

int expose_page_table(pid_t pid, struct expose_pgtbl_args __user *args);
```
A program called vm_inspector to dump the page table entries of a process in given range. To dump the PTEs of a target process, you will have to specify a process identifier "pid" to vm_inspector.
```
./vm_inspector [-v] pid va_begin va_end
```
And following information will be printed:
```
[virt] [phys] [young bit] [dirty bit] [write bit] [user bit]
e.g.
0x7f3000000000 0x530000 1 0 0 1

If a page is not present and the -v option is used, print it in the
following format:
e.g.
0xdead00000000 0x0 0 0 0 0
```
## PtreeFS: Process Tree File System
A pseudo file system called PtreeFS, which will export kernel information about the process hierarchy of currently running processes.\
When PtreeFS is mounted at a point (assume /ptreefs) it provides the following tree structure. The contents of the root of the file system should be a directory named by the PID of the oldest currently running process in the system. Within that directory, there should be two kinds of special files. One file should be named as the name of the process with the respective PID with a file suffix of .name. All .name files are empty; the filename contains the important information. The other files should be directories named by the PIDs of the children of that process.

