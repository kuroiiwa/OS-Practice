#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <vm_inspector.h>

static inline unsigned long get_phys_addr(unsigned long pte_entry)
{
        return (((1UL << 46) - 1) & pte_entry) >> 12 << 12;
}

static inline int young_bit(unsigned long pte_entry)
{
        return 1UL << 5 & pte_entry ? 1 : 0;
}

static inline int dirty_bit(unsigned long pte_entry)
{
        return 1UL << 6 & pte_entry ? 1 : 0;
}

static inline int write_bit(unsigned long pte_entry)
{
        return 1UL << 1 & pte_entry ? 1 : 0;
}

static inline int user_bit(unsigned long pte_entry)
{
        return 1UL << 2 & pte_entry ? 1 : 0;
}

void print_pgtbl(struct expose_pgtbl_args *args)
{
        unsigned long *pgd_table = (unsigned long *)args->fake_pgd;
        unsigned long *pud_tables = (unsigned long *)args->fake_puds;
        unsigned long *pmd_tables = (unsigned long *)args->fake_pmds;
        unsigned long *pte_tables = (unsigned long *)args->page_table_addr;
        int total_pgd = OTHER_SPACE;

        printf("======PGD=====\n");
        for (int i = 0; i < 512; i++) {
                if (pgd_table[i] == 0)
                        continue;
                printf("%03d|0x%016lx|\n", i, pgd_table[i]);
        }
        printf("======PUD=====\n");
        for (int i = 0; i < total_pgd; i++) {
               for (int j = 0; j < 512; j++) {
                       if (pud_tables[j] == 0)
                               continue;
                       printf("%03d->%03d|0x%016lx|\n", i, j, pud_tables[j]);
               }
               pud_tables = (unsigned long *)((unsigned long)pud_tables
                       + 512 * sizeof(unsigned long));
        }
        printf("======PMD=====\n");
        for (int i = 0; i < total_pgd; i++) {
               for (int j = 0; j < 512; j++) {
                       if (pmd_tables[j] == 0)
                               continue;
                       printf("%03d->%03d|0x%016lx|\n", i, j, pmd_tables[j]);
               }
               pmd_tables = (unsigned long *)((unsigned long)pmd_tables
                       + 512 * sizeof(unsigned long));
        }
        printf("======PTE=====\n");
        for (int i = 0; i < PTES_SPACE; i++) {
               for (int j = 0; j < 512; j++) {
                       if (pte_tables[j] == 0)
                               continue;
                       printf("%03d->%03d|0x%016lx|\n",i ,j , pte_tables[j]);
               }
               pte_tables = (unsigned long *)((unsigned long)pte_tables
                       + 512 * sizeof(unsigned long));
        }
}

int main(int argc, char **argv)
{
	struct pagetable_layout_info info;
	struct expose_pgtbl_args args;
	pid_t pid;
        unsigned long *base_addr;
        int verbose = 0;

	int res = syscall(__NR_get_pagetable_layout, &info,
			sizeof(struct pagetable_layout_info));
        if (res)
                printf("layout: pgd[%d] pud[%d] pmd[%d] pg_shift[%d]\n",
                        info.pgdir_shift, info.pud_shift,
                        info.pmd_shift, info.page_shift);

        //args.begin_vaddr = 0x640000000000; //PGD=200
        args.begin_vaddr = 0x000000000000; //PGD=200
	args.end_vaddr = 0xffffffffffff; //VA_END

        base_addr = (unsigned long *)malloc(sizeof(unsigned long) * PTRS_PER_PGD);
        if (base_addr == NULL)
                return -1;
        args.fake_pgd = (unsigned long)base_addr;


        base_addr = mmap(NULL, sizeof(unsigned long) * OTHER_SPACE * PTRS_PER_PUD,
                        PROT_WRITE | PROT_READ, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
        if (base_addr == NULL)
                return -1;
        args.fake_puds = (unsigned long)base_addr;

        base_addr = mmap(NULL, sizeof(unsigned long) * OTHER_SPACE* PTRS_PER_PMD,
                        PROT_WRITE | PROT_READ, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
        if (base_addr == NULL)
                return -1;
        args.fake_pmds = (unsigned long)base_addr;

        base_addr = mmap(NULL, sizeof(unsigned long) * PTES_SPACE * PTRS_PER_PTE,
                        PROT_WRITE | PROT_READ, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
        if (base_addr == NULL)
                return -1;
        args.page_table_addr = (unsigned long)base_addr;
        printf("%16lx\n", (unsigned long)base_addr);

        pid = -1;
	res = syscall(__NR_expose_page_table, pid, &args);
	if (res < 0){
		printf("Try assign more space for PTEs\n");
                return -1;
        }
        print_pgtbl(&args);
        fflush(stdout);
        sleep(5);
        printf("<=========================>\n");
        print_pgtbl(&args);
        fflush(stdout);
        return 0;
}
