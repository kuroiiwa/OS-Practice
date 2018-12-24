#include <linux/syscalls.h>
#include <linux/sched.h>
#include <linux/prinfo.h>
#include <linux/list.h>
#include <linux/uaccess.h>
#include <linux/uidgid.h>
#include <linux/slab.h>

static int copy_prinfo_k(struct task_struct *task, struct prinfo *task_k,
			  int num_p, int num_tab)
{
	struct task_struct *tmp;

	if (num_p == 0)
		task_k[num_p].parent_pid = 0;
	else
		task_k[num_p].parent_pid = (task->parent)->pid;
	if (list_empty(&task->children))
		task_k[num_p].first_child_pid = 0;
	else {
		tmp = list_last_entry(&task->children,
					struct task_struct, sibling);
		task_k[num_p].first_child_pid = tmp->pid;
	}
	if (list_empty(&task->sibling) 
		|| task->sibling.prev == &(task->parent->children))
		task_k[num_p].next_sibling_pid = 0;
	else {
		tmp = list_last_entry(&task->sibling,
				    struct task_struct, sibling);
		task_k[num_p].next_sibling_pid = tmp->pid;
	}
	task_k[num_p].pid = task->pid;
	task_k[num_p].state = task->state;
	task_k[num_p].uid = (task->cred->uid).val;
	get_task_comm(task_k[num_p].comm, task);
	return 0;
}
/*
 *traverse the list_task in DFS
 */
static int traverse_prc(struct task_struct *task, struct prinfo *task_k,
			  int *num_p, int num_u, int num_tab)
{
	struct list_head *list;
	struct task_struct *tmp;

	if (*num_p < num_u)
		copy_prinfo_k(task, task_k, *num_p, num_tab);
	(*num_p)++;
	if ((task->children).next != (task->children).prev) {
		list_for_each_prev(list, &task->children) {
			tmp = list_entry(list, struct task_struct, sibling);
			traverse_prc(tmp, task_k, num_p, num_u, num_tab+1);
		}
	}
	return 0;

}
/*
 *ptree syscall definition
 */
SYSCALL_DEFINE2(ptree, struct prinfo __user *, buf, int __user *, nr)
{
	struct prinfo *task_k;
	int *numr;
	int num_p;

/*user arguments validation*/
	if (!buf || !nr)
		return -EINVAL;
	if (!access_ok(VERIFY_WRITE, nr, sizeof(int)))
		return -EFAULT;
	numr = kmalloc(sizeof(*numr), GFP_KERNEL);
	if (!numr)
		return -EFAULT;
	if (copy_from_user(numr, nr, sizeof(int)))
		return -EFAULT;
	if ((*numr) < 1)
		return -EINVAL;
	if (!access_ok(VERIFY_WRITE, buf, sizeof(struct prinfo) * (*numr)))
		return -EFAULT;
	task_k = kmalloc(sizeof(struct prinfo) * (*numr), GFP_KERNEL);
	if (!task_k)
		return -EFAULT;
	num_p = 0;
	read_lock(&tasklist_lock);
	traverse_prc(&init_task, task_k, &num_p, *numr, 0);
	read_unlock(&tasklist_lock);
	if (copy_to_user(buf, task_k, sizeof(struct prinfo) * (*numr)))
		return -EFAULT;

	if (copy_to_user(nr, &num_p, sizeof(int)))
		return -EFAULT;
	if (num_p > *numr)
		num_p = *numr;
	kfree(numr);
	kfree(task_k);
	return num_p;
}
