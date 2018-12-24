cd#ifndef _VM_INSPECTOR_H
#define _VM_INSPECTOR_H

#define __NR_get_pagetable_layout 326
#define __NR_expose_page_table 327
#define PTES_SPACE              50
#define OTHER_SPACE             10

#define PTRS_PER_PTE		512
#define PTRS_PER_PMD		512
#define PTRS_PER_PUD		512
#define PTRS_PER_PGD		512

struct pagetable_layout_info {
        uint32_t pgdir_shift;
        uint32_t pud_shift;
        uint32_t pmd_shift;
        uint32_t page_shift;
 };

struct expose_pgtbl_args {
        unsigned long fake_pgd;
        unsigned long fake_puds;
        unsigned long fake_pmds;
        unsigned long page_table_addr;
        unsigned long begin_vaddr;
        unsigned long end_vaddr;
};


#endif
