#include "sched.h"
#include "linux/init_task.h"
/*
 * for get_cpu()
 */
#include "linux/smp.h"
#include <linux/kernel.h>

const struct sched_class wrr_sched_class;

static inline struct task_struct *wrr_task_of(struct sched_wrr_entity *wrr)
{
	return container_of(wrr, struct task_struct, wrr);
}

	static void
enqueue_wrr_entity(struct rq *rq, struct sched_wrr_entity *wrr_se, bool head)
{
	struct list_head *queue = &rq->wrr.wrr_task_list;

	if (head)
		list_add(&wrr_se->wrr_task_list, queue);
	else
		list_add_tail(&wrr_se->wrr_task_list, queue);
	++rq->wrr.wrr_nr_running;
	rq->wrr.total_weight += wrr_se->wrr_weight;
}

	static void
dequeue_wrr_entity(struct rq *rq, struct sched_wrr_entity *wrr_se)
{
	list_del_init(&wrr_se->wrr_task_list);
	rq->wrr.total_weight -= wrr_se->wrr_weight;
	--rq->wrr.wrr_nr_running;
}

void init_wrr_rq(struct wrr_rq *wrr_rq)
{
	wrr_rq->total_weight = 0;
	INIT_LIST_HEAD(&wrr_rq->wrr_task_list);
	wrr_rq->wrr_nr_running = 0;
}

static void watchdog(struct rq *rq, struct task_struct *p)
{
	unsigned long soft, hard;

	/* max may change after cur was read, this will be fixed next tick */
	soft = task_rlimit(p, RLIMIT_RTTIME);
	hard = task_rlimit_max(p, RLIMIT_RTTIME);

	if (soft != RLIM_INFINITY) {
		unsigned long next;

		p->wrr.timeout++;

		next = DIV_ROUND_UP(min(soft, hard), USEC_PER_SEC/HZ);
		if (p->wrr.timeout > next)
			p->cputime_expires.sched_exp = p->se.sum_exec_runtime;
	}
}

static void update_curr_wrr(struct rq *rq)
{
	struct task_struct *curr = rq->curr;
	u64 delta_exec;

	if (curr->sched_class != &wrr_sched_class)
		return;

	delta_exec = rq->clock_task - curr->se.exec_start;
	if (unlikely((s64)delta_exec <= 0))
		delta_exec = 0;

	schedstat_set(curr->se.statistics.exec_max,
			max(curr->se.statistics.exec_max, delta_exec));

	curr->se.sum_exec_runtime += delta_exec;
	account_group_exec_runtime(curr, delta_exec);
	curr->se.exec_start = rq->clock_task;
	cpuacct_charge(curr, delta_exec);
}

	static void
enqueue_task_wrr(struct rq *rq, struct task_struct *p, int flags)
{
	struct sched_wrr_entity *wrr_se = &p->wrr;
	int cpu;

	if (flags & ENQUEUE_WAKEUP)
		wrr_se->timeout = 0;

	if (!list_empty(&wrr_se->wrr_task_list))
		return;
	enqueue_wrr_entity(rq, wrr_se, flags & ENQUEUE_HEAD);
	raw_spin_lock(&p->group_leader->wrr.wrr_lock);
	cpu = rq->cpu;
	p->group_leader->wrr.cpus[cpu]++;
	raw_spin_unlock(&p->group_leader->wrr.wrr_lock);
	add_nr_running(rq, 1);
}

static void dequeue_task_wrr(struct rq *rq, struct task_struct *p, int flags)
{
	struct sched_wrr_entity *wrr_se = &p->wrr;
	int cpu;

	update_curr_wrr(rq);
	dequeue_wrr_entity(rq, wrr_se);
	raw_spin_lock(&p->group_leader->wrr.wrr_lock);
	cpu = rq->cpu;
	p->group_leader->wrr.cpus[cpu]--;
	raw_spin_unlock(&p->group_leader->wrr.wrr_lock);
	sub_nr_running(rq, 1);
}

static void requeue_task_wrr(struct rq *rq, struct task_struct *p, int head)
{
	struct sched_wrr_entity *wrr_se = &p->wrr;
	struct list_head *queue = &(rq->wrr.wrr_task_list);

	if (head)
		list_move(&wrr_se->wrr_task_list, queue);
	else
		list_move_tail(&wrr_se->wrr_task_list, queue);
}

static void yield_task_wrr(struct rq *rq)
{
	requeue_task_wrr(rq, rq->curr, 0);
}

/*No preemption involved*/
	static void
check_preempt_curr_wrr(struct rq *rq, struct task_struct *p, int flags)
{
}

	static struct task_struct *
pick_next_task_wrr(struct rq *rq, struct task_struct *prev)
{
	struct task_struct *next;
	struct sched_wrr_entity *next_ent;

	if (!rq->wrr.wrr_nr_running)
		return NULL;

	next_ent = list_first_entry(&rq->wrr.wrr_task_list,
			struct sched_wrr_entity, wrr_task_list);
	next = wrr_task_of(next_ent);
	if (!next)
		return NULL;

	next->se.exec_start = rq->clock_task;
	return next;
}

static void put_prev_task_wrr(struct rq *rq, struct task_struct *prev)
{
	update_curr_wrr(rq);
	prev->se.exec_start = 0;
}

#ifdef CONFIG_SMP

static int
multiple_cpu_exist(int min_cpu, int min_cpu_weight, int *same_cpus)
{
	int i;
	int multiple = 0;

	/*No need to compare affinity if total_weight == 0*/
	if (min_cpu_weight == 0)
		return multiple;
	for_each_possible_cpu(i) {
		struct wrr_rq *wrr_rq = &cpu_rq(i)->wrr;

		if (min_cpu == i)
			continue;

		if (wrr_rq->total_weight == min_cpu_weight) {
			same_cpus[i] = 1;
			multiple = 1;
		}
	}
	return multiple;
}

static int
affinity_cpu(int *same_cpus, struct task_struct *p)
{
	struct sched_wrr_entity *gl_se = &p->group_leader->wrr;
	int min_cpu, affinity, i;

	raw_spin_lock(&gl_se->wrr_lock);
	affinity = 0;
	for (i = 0; i < MAX_CPUS; i++) {
		/*Compare cpus with same weight*/
		if (same_cpus[i]) {
			/*Find minimal affinity (might be the same)*/
			if (affinity <= gl_se->cpus[i]) {
				affinity = gl_se->cpus[i];
				min_cpu = i;
			}
		}
	}
	raw_spin_unlock(&gl_se->wrr_lock);
	return min_cpu;
}

	static int
select_task_rq_wrr(struct task_struct *p, int cpu, int sd_flag, int flags)
{
	int i;
	int min_cpu;
	int min_cpu_weight;
	int this_cpu_weight;
	int same_cpus[MAX_CPUS];

	rcu_read_lock();

	/*
	 * get this cpu's total weight and suppose it is min
	 */
	min_cpu_weight = this_rq()->wrr.total_weight;
	min_cpu = get_cpu();

	for_each_possible_cpu(i) {
		struct wrr_rq *wrr_rq = &cpu_rq(i)->wrr;

		this_cpu_weight = wrr_rq->total_weight;
		if (this_cpu_weight < min_cpu_weight) {
			min_cpu_weight = this_cpu_weight;
			min_cpu = i;
		}
	}

	if (multiple_cpu_exist(min_cpu, min_cpu_weight, same_cpus))
		min_cpu = affinity_cpu(same_cpus, p);

	rcu_read_unlock();

	return min_cpu;
}
#endif

static void set_curr_task_wrr(struct rq *rq)
{
	struct task_struct *p = rq->curr;

	p->se.exec_start = rq->clock_task;
}

static void task_tick_wrr(struct rq *rq, struct task_struct *p, int queued)
{
	struct sched_wrr_entity *wrr_se = &p->wrr;

	update_curr_wrr(rq);

	watchdog(rq, p);

	if (p->policy != SCHED_WRR)
		return;

	if (--wrr_se->time_slice)
		return;

	wrr_se->time_slice = wrr_se->wrr_weight * BASE_WRR_TIMESLICE;

	/*
	 * Requeue to the end of queue if we (and all of our ancestors) are not
	 * the only element on the queue
	 */

	if (wrr_se->wrr_task_list.prev != wrr_se->wrr_task_list.next) {
		requeue_task_wrr(rq, p, 0);
		resched_curr(rq);
	}
}

/*No prio involved*/
	static void
prio_changed_wrr(struct rq *rq, struct task_struct *p, int oldprio)
{
}

static void switched_to_wrr(struct rq *rq, struct task_struct *p)
{
	struct sched_wrr_entity *wrr_se = &p->wrr;

	wrr_se->wrr_weight = DEFAULT_WRR_WEIGHT;
	wrr_se->time_slice = wrr_se->wrr_weight * BASE_WRR_TIMESLICE;
}


	static unsigned int
get_rr_interval_wrr(struct rq *rq, struct task_struct *task)
{
	return task->wrr.wrr_weight * BASE_WRR_TIMESLICE;
}

const struct sched_class wrr_sched_class = {
	.next			= &fair_sched_class,
	.enqueue_task		= enqueue_task_wrr,
	.dequeue_task		= dequeue_task_wrr,
	.yield_task		= yield_task_wrr,

	.check_preempt_curr	= check_preempt_curr_wrr,

	.pick_next_task		= pick_next_task_wrr,
	.put_prev_task		= put_prev_task_wrr,

#ifdef CONFIG_SMP
	.select_task_rq		= select_task_rq_wrr,
	.set_cpus_allowed	= set_cpus_allowed_common,
#endif

	.set_curr_task          = set_curr_task_wrr,
	.task_tick		= task_tick_wrr,

	.get_rr_interval	= get_rr_interval_wrr,

	.prio_changed		= prio_changed_wrr,
	.switched_to		= switched_to_wrr,

	.update_curr		= update_curr_wrr,
};

#ifdef CONFIG_SMP
void wrr_pull_task(int dst_cpu)
{
	int src_cpu;
	int i;
	struct rq *dst_rq = cpu_rq(dst_cpu);
	struct rq *src_rq;
	struct task_struct *p;
	struct sched_wrr_entity *wrr_se;
	int max_wrr_weight;

	if (!cpu_active(dst_cpu))
		return;

	src_cpu = -1;
	max_wrr_weight = 0;
	rcu_read_lock();
	for_each_possible_cpu(i) {
		if (i == dst_cpu)
			continue;

		src_rq = cpu_rq(i);

		if (src_rq->wrr.wrr_nr_running <= 1)
			continue;

		if (max_wrr_weight < src_rq->wrr.total_weight) {
			src_cpu = i;
			max_wrr_weight = src_rq->wrr.total_weight;
		}
	}
	rcu_read_unlock();
	if (src_cpu == -1)
		return;
	/*Found the adequate src_cpu*/
	src_rq = cpu_rq(src_cpu);
	double_rq_lock(dst_rq, src_rq);
	/*Iterate each process to find the task to be pulled*/
	list_for_each_entry(wrr_se, &src_rq->wrr.wrr_task_list,
			wrr_task_list) {
		p = list_entry(wrr_se, struct task_struct, wrr);

		if (task_running(src_rq, p) ||
				p->policy != SCHED_WRR ||
				!cpumask_test_cpu(dst_cpu, tsk_cpus_allowed(p)))
			continue;

		if (p->on_rq) {
			deactivate_task(src_rq, p, 0);
			set_task_cpu(p, dst_cpu);
			activate_task(dst_rq, p, 0);
			check_preempt_curr(dst_rq, p, 0);

			double_rq_unlock(dst_rq, src_rq);
			return;
		}
	}
	double_rq_unlock(dst_rq, src_rq);
}
#endif

#ifdef CONFIG_SCHED_DEBUG
extern void print_wrr_rq(struct seq_file *m, int cpu, struct wrr_rq *wrr_rq);
void print_wrr_stats(struct seq_file *m, int cpu)
{
	struct wrr_rq *wrr_rq;
	struct rq *temp_rq;

	rcu_read_lock();

	temp_rq = cpu_rq(cpu);
	wrr_rq = &temp_rq->wrr;
	print_wrr_rq(m, cpu, wrr_rq);

	rcu_read_unlock();
}
#endif /* CONFIG_SCHED_DEBUG */
