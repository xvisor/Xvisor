/**
 * Copyright (c) 2012 Himanshu Chauhan.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * @file cpu_mmu.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief Memory management code.
 */

#include <arch_cpu.h>
#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_string.h>
#include <vmm_host_aspace.h>
#include <vmm_types.h>
#include <cpu_mmu.h>

extern u8 _code_end;
extern u8 _code_start;

/* bootstrap page table */
extern u64 __pml4[];
extern u64 __pgdp[];
extern u64 __pgdi[];
extern u64 __pgti[];

/* */
static u64 pml4[512] __attribute__((aligned(PAGE_SIZE)));
static u64 pgdp[512] __attribute__((aligned(PAGE_SIZE)));
static u64 pgdi[512] __attribute__((aligned(PAGE_SIZE)));
static u64 *pgti;

static int __bootstrap_text
create_cpu_boot_pgtable_entry(virtual_addr_t va,
			      physical_addr_t pa,
			      virtual_size_t sz,
			      u32 mem_flags)
{
	u32 offset;
	union page pg;
	int i;

	sz = VMM_ROUNDUP2_PAGE_SIZE(sz);

	for (i = 0; i < sz / PAGE_SIZE; i++) {
		offset = VIRT_TO_PGTI(va) + (VIRT_TO_PGDI(va) * 512);
		vmm_memset((void *)&pg, 0, sizeof(pg));
		pg.bits.paddr = (pa >> PAGE_SHIFT);
		pg.bits.present = 1;
		pg.bits.rw = 1;
		__pgti[offset] = pg._val;

		vmm_memset((void *)&pg, 0, sizeof(pg));
		pg.bits.paddr = VIRT_TO_PHYS((u64)(&__pgti[offset])
					     & PAGE_MASK) >> PAGE_SHIFT;
		pg.bits.present = 1;
		pg.bits.rw = 1;
		offset = VIRT_TO_PGDI(va);
		__pgdi[offset] = pg._val;

		vmm_memset((void *)&pg, 0, sizeof(pg));
		pg.bits.paddr = VIRT_TO_PHYS((u64)(&__pgdi[offset])
					     & PAGE_MASK) >> PAGE_SHIFT;
		pg.bits.present = 1;
		pg.bits.rw = 1;
		offset = VIRT_TO_PGDP(va);
		__pgdp[offset] = pg._val;

		vmm_memset((void *)&pg, 0, sizeof(pg));
		pg.bits.paddr = VIRT_TO_PHYS((u64)(&__pgdp[offset])
					     & PAGE_MASK) >> PAGE_SHIFT;
		pg.bits.present = 1;
		pg.bits.rw = 1;
		offset = VIRT_TO_PML4(va);
		__pml4[offset] = pg._val;

		va += PAGE_SIZE;
		pa += PAGE_SIZE;
	}

	return VMM_OK;
}

static void switch_to_pagetable(physical_addr_t pml4_base)
{
        __asm__ __volatile__ ("movq %0, %%cr3\n\t"
			      "jmp 1f\n\t" /* sync EIP */
			      "1:\n\t"
                              ::"r"(pml4_base));
        barrier();
}

int arch_cpu_aspace_map(virtual_addr_t va,
			virtual_size_t sz,
			physical_addr_t pa,
			u32 mem_flags)
{
	u32 offset;
	union page pg;
	int i;

	sz = VMM_ROUNDUP2_PAGE_SIZE(sz);

	for (i = 0; i < sz / PAGE_SIZE; i++) {
		offset = VIRT_TO_PGTI(va) + (VIRT_TO_PGDI(va) * 512);
		vmm_memset((void *)&pg, 0, sizeof(pg));
		pg.bits.paddr = (pa >> PAGE_SHIFT);
		pg.bits.present = 1;
		pg.bits.rw = 1;
		pgti[offset] = pg._val;

		vmm_memset((void *)&pg, 0, sizeof(pg));
		pg.bits.paddr = VIRT_TO_PHYS((u64)(&pgti[offset])
					     & PAGE_MASK) >> PAGE_SHIFT;
		pg.bits.present = 1;
		pg.bits.rw = 1;
		offset = VIRT_TO_PGDI(va);
		pgdi[offset] = pg._val;

		vmm_memset((void *)&pg, 0, sizeof(pg));
		pg.bits.paddr = VIRT_TO_PHYS((u64)(&pgdi[offset])
					     & PAGE_MASK) >> PAGE_SHIFT;
		pg.bits.present = 1;
		pg.bits.rw = 1;
		offset = VIRT_TO_PGDP(va);
		pgdp[offset] = pg._val;

		vmm_memset((void *)&pg, 0, sizeof(pg));
		pg.bits.paddr = VIRT_TO_PHYS((u64)(&pgdp[offset])
					     & PAGE_MASK) >> PAGE_SHIFT;
		pg.bits.present = 1;
		pg.bits.rw = 1;
		offset = VIRT_TO_PML4(va);
		pml4[offset] = pg._val;

		va += PAGE_SIZE;
		pa += PAGE_SIZE;
	}

	return VMM_OK;
}

int arch_cpu_aspace_unmap(virtual_addr_t va,
			 virtual_size_t sz)
{
	/* FIXME: */
	return VMM_OK;
}

int arch_cpu_aspace_init(physical_addr_t * resv_pa,
			virtual_addr_t * resv_va,
			virtual_size_t * resv_sz)
{
	virtual_addr_t cva, eva = 0;
	u32 pg_tab_sz = 0, tsize2map;
	physical_addr_t pa;

	tsize2map = (CONFIG_VAPOOL_SIZE << 20) + arch_code_size() + (*resv_sz);

	tsize2map = VMM_ROUNDUP2_PAGE_SIZE(tsize2map);

	/*
	 * Each page with pagetable entries map VMM_PAGE_SIZE * 512
	 * bytes. So we calculate the number of pages required to
	 * map the tsize2map.
	 */
	pg_tab_sz = tsize2map / (VMM_PAGE_SIZE * 512);

	if (unlikely(!pg_tab_sz)) pg_tab_sz++;

	/*
	 * One extra page because we will
	 * need to access page tables using virtual address.
	 */
	pg_tab_sz++;
	pg_tab_sz++;

	/*
	 * Boot page tables are only till end of VMM. New
	 * page table start at the end of code. Before accessing
	 * them we need to map them.
	 */
	create_cpu_boot_pgtable_entry((virtual_addr_t)&_code_end,
				      (physical_addr_t)&_code_end,
				      (pg_tab_sz * VMM_PAGE_SIZE), 0);

	pgti = (u64 *)(arch_code_vaddr_start() + arch_code_size());

	cva = arch_code_vaddr_start();

	eva = cva + ((CONFIG_VAPOOL_SIZE << 20)
		     + (arch_code_size()
			+ (*resv_sz) + (pg_tab_sz * PAGE_SIZE)));

	pa = *resv_pa;

	/* Create the page table entries for all the virtual addresses. */
	for (; cva < eva;) {
		if (arch_cpu_aspace_map(cva, VMM_PAGE_SIZE, pa, 0) != VMM_OK)
			return VMM_EFAIL;

		cva += VMM_PAGE_SIZE;
		pa += VMM_PAGE_SIZE;
	}

	*resv_sz += (pg_tab_sz * PAGE_SIZE);
	*resv_va += (pg_tab_sz * PAGE_SIZE);
	*resv_pa += (pg_tab_sz * PAGE_SIZE) + arch_code_size();

        /* Switch over to new page table. */
        switch_to_pagetable(VIRT_TO_PHYS(&pml4[0]));

	return VMM_OK;
}

int arch_cpu_aspace_va2pa(virtual_addr_t va, physical_addr_t * pa)
{
	return VMM_OK;
}

virtual_addr_t arch_code_vaddr_start(void)
{
	return ((virtual_addr_t) CPU_TEXT_LMA);
}

physical_addr_t arch_code_paddr_start(void)
{
	return ((physical_addr_t) CPU_TEXT_LMA);
}

virtual_size_t cpu_code_base_size(void)
{
	return (virtual_size_t)(&_code_end - &_code_start);
}

virtual_size_t arch_code_size(void)
{
	return VMM_ROUNDUP2_PAGE_SIZE(cpu_code_base_size());
}
