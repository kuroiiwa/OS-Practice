#include <linux/pgtable_remap.h>
#include <linux/syscalls.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <asm/pgtable_types.h>
#include <linux/rcupdate.h>
#include <asm/pgtable.h>
#include <linux/sched.h>
#include <linux/mm_types.h>

static unsigned long curr_pud;
static unsigned long curr_pmd;
static unsigned long curr_pte;

SYSCALL_DEFINE2(get_pagetable_layout, struct pagetable_layout_info __user *,
        pgtbl_info, int, size)
{
        struct pagetable_layout_info info;

        if (pgtbl_info == NULL)
                return -EINVAL;
        if (size != sizeof(struct pagetable_layout_info))
                return -EINVAL;

        info.pgdir_shift = PGDIR_SHIFT;
        info.pud_shift = PUD_SHIFT;
        info.pmd_shift = PMD_SHIFT;
        info.page_shift = PAGE_SHIFT;

        if (copy_to_user(pgtbl_info, &info, size))
                return -EFAULT;
        return 1;
}

int save_pgd(unsigned long fake_pgd, unsigned long fake_puds,
                unsigned long curr_va,
                unsigned long *ptr)
{
        unsigned long *f_pgd_ent;
        unsigned long f_pud_addr;

        f_pgd_ent = (unsigned long *)(fake_pgd
                        + pgd_index(curr_va)
                        * sizeof(unsigned long));

        f_pud_addr = fake_puds +
                        curr_pud * PTRS_PER_PUD
                        * sizeof(unsigned long);
        *ptr = f_pud_addr;
        curr_pud++;

        if (copy_to_user(f_pgd_ent, &f_pud_addr, sizeof(unsigned long)))
                return -EFAULT;

        printk("PGD saved! %lu|%lu\n", pgd_index(curr_va), curr_pud);
        return 0;
}

int save_pud(unsigned long f_pud_addr, unsigned long fake_pmds,
                unsigned long curr_va,
                unsigned long *ptr)
{
        unsigned long *f_pud_ent;
        unsigned long f_pmd_addr;


        f_pud_ent = (unsigned long *)(f_pud_addr +
                        pud_index(curr_va) * sizeof(unsigned long));

        f_pmd_addr = fake_pmds +
                        curr_pmd * PTRS_PER_PMD
                        * sizeof(unsigned long);
        *ptr = f_pmd_addr;
        curr_pmd++;

        if (copy_to_user(f_pud_ent, &f_pmd_addr, sizeof(unsigned long)))
                return -EFAULT;

        printk("PUD saved! %lu|%lu\n", pud_index(curr_va), curr_pmd);
        return 0;

}

int remap_this(unsigned long f_pmd_addr, unsigned long page_table_addr,
        unsigned long curr_va,
        pmd_t *pmd_p, struct task_struct *p)
{
        unsigned long *f_pmd_ent;
        unsigned long k_pte_addr, f_pte_addr;
        struct vm_area_struct *vma;
        int res;

        f_pmd_ent = (unsigned long *)(f_pmd_addr +
                        pmd_index(curr_va) * sizeof(unsigned long));

        f_pte_addr = page_table_addr +
                        curr_pte * PTRS_PER_PTE
                        * sizeof(unsigned long);
        curr_pte++;

        if (copy_to_user(f_pmd_ent, &f_pte_addr, sizeof(unsigned long)))
                return -EFAULT;

        printk("PMD saved! %lu|%lu\n", pmd_index(curr_va), curr_pte);

        k_pte_addr = pmd_pfn(*pmd_p);
        vma = find_vma(current->mm, f_pte_addr);
        if (vma == NULL)
                return -EFAULT;

        if (p != current)
                down_write(&p->mm->mmap_sem);
        down_write(&current->mm->mmap_sem);
        if ((res = remap_pfn_range(vma,
                f_pte_addr,
                k_pte_addr,
                PAGE_SIZE,
                vma->vm_page_prot))) {
                if (p != current)
                        up_write(&p->mm->mmap_sem);
                up_write(&current->mm->mmap_sem);
                printk("errno:[%d]\n", res);
                return -EAGAIN;
        }
        if (p != current)
                up_write(&p->mm->mmap_sem);
        up_write(&current->mm->mmap_sem);
        return 0;
}

SYSCALL_DEFINE2(expose_page_table, pid_t, pid,
        struct expose_pgtbl_args __user *, args)
{
        struct expose_pgtbl_args args_k;
        struct task_struct *p;
        struct mm_struct *mm;
        struct vm_area_struct *vma;
        const unsigned long ubound_va = 0xffffffffffff;
        unsigned long start, end, curr_va, prev_va;
        unsigned long f_pud_addr, f_pmd_addr;
        int last_vm, res;
        pgd_t *pgd_p;
        pud_t *pud_p;
        pmd_t *pmd_p;

        if (args == NULL || pid < -1)
                return -EINVAL;
        if (copy_from_user(&args_k, args,
                        sizeof(struct expose_pgtbl_args)))
                return -EFAULT;

        if (args_k.begin_vaddr > args_k.end_vaddr)
                return -EINVAL;
        if (args_k.end_vaddr > ubound_va)
                args_k.end_vaddr = ubound_va;
         /*
          * Get task_struct from pid
          */
        rcu_read_lock();
        if (pid == -1)
                p = current;
        else
                p = find_task_by_vpid(pid);
        rcu_read_unlock();
        if(!p)
                return -ESRCH;

        mm = get_task_mm(p);

        if (mm == NULL)
                return -EFAULT;

        vma = find_vma(mm, args_k.begin_vaddr);

        if (vma == NULL)
                return -EFAULT;

        last_vm = 0;
        curr_pud = curr_pmd = curr_pte = 0;
        prev_va = ubound_va;

        if (p != current)
                spin_lock(&mm->page_table_lock);
        for (; vma->vm_next != NULL; vma = vma->vm_next) {
                if (likely(vma->vm_start > args_k.begin_vaddr))
                        start = vma->vm_start;
                else
                        start = args_k.begin_vaddr;

                if (likely(vma->vm_end < args_k.end_vaddr))
                        end = vma->vm_end;
                else {
                        end = args_k.end_vaddr;
                        last_vm = 1;
                }
                printk("%lu|%lu|%lu|%lu\n", pgd_index(start),
                                        pud_index(start),
                                        pmd_index(start),
                                        pte_index(start));

                printk("%lu|%lu|%lu|%lu\n", pgd_index(end),
                                        pud_index(end),
                                        pmd_index(end),
                                        pte_index(end));
                for (curr_va = start; curr_va <= end; curr_va++) {
                        /* PGD */
                        pgd_p = pgd_offset(mm, curr_va);
                        if (pgd_none(*pgd_p) || pgd_bad(*pgd_p)) {
                                prev_va = curr_va;
                                continue;
                        }
                        if (pgd_index(curr_va) != pgd_index(prev_va)) {
                                res = save_pgd(args_k.fake_pgd, args_k.fake_puds,
                                        curr_va, &f_pud_addr);
                                if (unlikely(res < 0)) {
                                        if (p != current)
                                                spin_unlock(&mm->page_table_lock);
                                        return res;
                                }
                        }
                        /* PUD */
                        pud_p = pud_offset(pgd_p, curr_va);
                        if (pud_none(*pud_p) || pud_bad(*pud_p)) {
                                prev_va = curr_va;
                                continue;
                        }
                        if (pgd_index(curr_va) != pgd_index(prev_va) ||
                                pud_index(curr_va) != pud_index(prev_va)) {
                                res = save_pud(f_pud_addr, args_k.fake_pmds,
                                        curr_va, &f_pmd_addr);
                                if (unlikely(res < 0)) {
                                        if (p != current)
                                                spin_unlock(&mm->page_table_lock);
                                        return res;
                                }
                        }
                        /* Remap */
                        pmd_p = pmd_offset(pud_p, curr_va);
                        if (pmd_none(*pmd_p) || pmd_bad(*pmd_p)) {
                                prev_va = curr_va;
                                continue;
                        }
                        if (pgd_index(curr_va) != pgd_index(prev_va) ||
                                pud_index(curr_va) != pud_index(prev_va) ||
                                pmd_index(curr_va) != pmd_index(prev_va)) {
                                res = remap_this(f_pmd_addr,
                                        args_k.page_table_addr,
                                        curr_va, pmd_p, p);
                                if (unlikely(res < 0)) {
                                        if (p != current)
                                                spin_unlock(&mm->page_table_lock);
                                        return res;
                                }
                        }

                        prev_va = curr_va;

                }
                if (unlikely(last_vm))
                        break;
        }
        if (p != current)
                spin_unlock(&mm->page_table_lock);
        return 1;
}
