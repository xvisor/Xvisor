/**
 * Copyright (c) 2012 Jean-Christophe Dubois.
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
 * @file cpu_mmu_v5.c
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief source file for ARMV5 memory management unit
 */

#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_host_aspace.h>
#include <vmm_main.h>
#include <vmm_stdio.h>
#include <vmm_types.h>
#include <arch_cpu.h>
#include <cpu_defines.h>
#include <cpu_inline_asm.h>
#include <cpu_mmu.h>

#define TTBL_MAX_L1TBL_COUNT	(CONFIG_MAX_VCPU_COUNT + 1)

#define TTBL_MAX_L2TBL_COUNT	(CPU_VCPU_VTLB_ENTRY_COUNT * \
				 (CONFIG_MAX_VCPU_COUNT + 1))

u8 __attribute__ ((aligned(TTBL_L1TBL_SIZE))) defl1_mem[TTBL_L1TBL_SIZE];

struct cpu_mmu_ctrl {
	struct cpu_l1tbl defl1;
	virtual_addr_t l1_base_va;
	physical_addr_t l1_base_pa;
	struct cpu_l1tbl *l1_array;
	u8 *l1_bmap;
	u32 l1_alloc_count;
	virtual_addr_t l2_base_va;
	physical_addr_t l2_base_pa;
	struct cpu_l2tbl *l2_array;
	u8 *l2_bmap;
	u32 l2_alloc_count;
	struct dlist l1tbl_list;
	struct dlist l2tbl_list;
};

static struct cpu_mmu_ctrl mmuctrl;

/** Find L2 page table at given physical address from L1 page table */
static struct cpu_l2tbl *cpu_mmu_l2tbl_find_tbl_pa(physical_addr_t tbl_pa)
{
	u32 tmp = (tbl_pa - mmuctrl.l2_base_pa) >> TTBL_L2TBL_SIZE_SHIFT;

	if (tmp < TTBL_MAX_L2TBL_COUNT) {
		return &mmuctrl.l2_array[tmp];
	}

	/* Not found */
	return NULL;
}

/** Check whether a L2 page table is attached or not */
static int cpu_mmu_l2tbl_is_attached(struct cpu_l2tbl *l2)
{
	return (l2 && (l2->l1)) ? 1 : 0;
}

/** Detach a L2 page table */
static int cpu_mmu_l2tbl_detach(struct cpu_l2tbl *l2)
{
	u32 *l1_tte;
	u32 l1_tte_type;

	/* Sanity check */
	if (!l2) {
		return VMM_EFAIL;
	}

	/* if it is not attached already, nothing to do */
	if (!cpu_mmu_l2tbl_is_attached(l2)) {
		return VMM_OK;
	}

	l1_tte = (u32 *) (l2->l1->tbl_va +
			  ((l2->map_va >> TTBL_L1TBL_TTE_OFFSET_SHIFT) << 2));
	l1_tte_type = *l1_tte & TTBL_L1TBL_TTE_TYPE_MASK;
	if (l1_tte_type == TTBL_L1TBL_TTE_TYPE_FAULT) {
		return VMM_EFAIL;
	}

	*l1_tte = 0x0;
	l2->l1->tte_cnt--;
	l2->l1->l2tbl_cnt--;
	l2->l1 = NULL;
	l2->tte_cnt = 0;

	vmm_memset((void *)l2->tbl_va, 0, TTBL_L2TBL_SIZE);

	/* remove the L2 page table from this list it is attached */
	list_del(&l2->head);
	list_add(&mmuctrl.l2tbl_list, &l2->head);

	return VMM_OK;
}

/** Attach a L2 page table to a particular L1 page table */
static int cpu_mmu_l2tbl_attach(struct cpu_l1tbl *l1, struct cpu_l2tbl *l2,
				u32 new_imp, u32 new_domain,
				virtual_addr_t new_map_va, bool force)
{
	int rc;
	u32 *l1_tte, l1_tte_new;
	u32 l1_tte_type;

	/* Sanity check */
	if (!l2 || !l1) {
		return VMM_EFAIL;
	}

	/* If the L2 page is already attached */
	if (cpu_mmu_l2tbl_is_attached(l2)) {
		/* We try to detach it first */
		rc = cpu_mmu_l2tbl_detach(l2);
		if (rc) {
			return rc;
		}
	}

	l1_tte = (u32 *) (l1->tbl_va +
			  ((new_map_va >> TTBL_L1TBL_TTE_OFFSET_SHIFT) << 2));
	l1_tte_type = *l1_tte & TTBL_L1TBL_TTE_TYPE_MASK;
	if ((l1_tte_type != TTBL_L1TBL_TTE_TYPE_FAULT) && !force) {
		return VMM_EFAIL;
	}

	l2->l1 = l1;
	l2->domain =
	    new_domain & (TTBL_L1TBL_TTE_DOM_MASK >> TTBL_L1TBL_TTE_DOM_SHIFT);
	l2->map_va = new_map_va & TTBL_L1TBL_TTE_OFFSET_MASK;

	l1_tte_new = 0x0;
	l1_tte_new |= (l2->domain) << TTBL_L1TBL_TTE_DOM_SHIFT;
	l1_tte_new |= (l2->tbl_pa & TTBL_L1TBL_TTE_BASE10_MASK);
	l1_tte_new |= TTBL_L1TBL_TTE_REQ_MASK;
	l1_tte_new |= TTBL_L1TBL_TTE_TYPE_L2TBL;

	*l1_tte = l1_tte_new;

	if (l1_tte_type == TTBL_L1TBL_TTE_TYPE_FAULT) {
		l1->tte_cnt++;
	}

	l1->l2tbl_cnt++;

	list_del(&l2->head);
	list_add(&l1->l2tbl_list, &l2->head);

	return VMM_OK;
}

/** Allocate a L2 page table of given type */
static struct cpu_l2tbl *cpu_mmu_l2tbl_alloc(void)
{
	u32 i;
	struct cpu_l2tbl *l2;

	if (!list_empty(&mmuctrl.l2tbl_list)) {
		return list_entry(mmuctrl.l2tbl_list.next, struct cpu_l2tbl,
				  head);
	}

	/* if all page are already allocated, then return NULL */
	if (mmuctrl.l2_alloc_count == TTBL_MAX_L2TBL_COUNT) {
		return NULL;
	}

	for (i = 0; i < TTBL_MAX_L2TBL_COUNT; i++) {
		if (!mmuctrl.l2_bmap[i]) {
			break;
		}
	}

	/* if not found, then return NULL */
	if (i == TTBL_MAX_L2TBL_COUNT) {
		return NULL;
	}

	mmuctrl.l2_bmap[i] = 1;
	mmuctrl.l2_alloc_count++;

	l2 = &mmuctrl.l2_array[i];

	l2->l1 = NULL;
	l2->domain = 0;
	l2->map_va = 0;
	l2->tte_cnt = 0;

	vmm_memset((void *)l2->tbl_va, 0, TTBL_L2TBL_SIZE);

	list_add(&mmuctrl.l2tbl_list, &l2->head);

	return l2;
}

/** Free a L2 page table */
static int cpu_mmu_l2tbl_free(struct cpu_l2tbl *l2)
{
	int rc;

	/* Sanity check */
	if (!l2) {
		return VMM_EFAIL;
	}

	/* If the L2 page is attached */
	if (cpu_mmu_l2tbl_is_attached(l2)) {
		/* We detach it */
		rc = cpu_mmu_l2tbl_detach(l2);
		if (rc) {
			return rc;
		}
	}

	/* Remove the page from the mmuctrl.l2tbl_list */
	list_del(&l2->head);

	/* Reset it */
	mmuctrl.l2_bmap[l2->num] = 0;
	mmuctrl.l2_alloc_count--;

	return VMM_OK;
}

/** Find L1 page table at given physical address */
static struct cpu_l1tbl *cpu_mmu_l1tbl_find_tbl_pa(physical_addr_t tbl_pa)
{
	u32 tmp;

	if (tbl_pa == mmuctrl.defl1.tbl_pa) {
		return &mmuctrl.defl1;
	}

	tmp = (tbl_pa - mmuctrl.l1_base_pa) >> TTBL_L1TBL_SIZE_SHIFT;

	if (tmp < TTBL_MAX_L1TBL_COUNT) {
		return &mmuctrl.l1_array[tmp];
	}

	return NULL;
}

u32 cpu_mmu_best_page_size(virtual_addr_t va, physical_addr_t pa, u32 availsz)
{
	if (!(va & (TTBL_L1TBL_SECTION_PAGE_SIZE - 1)) &&
	    !(pa & (TTBL_L1TBL_SECTION_PAGE_SIZE - 1)) &&
	    (TTBL_L1TBL_SECTION_PAGE_SIZE <= availsz)) {
		return TTBL_L1TBL_SECTION_PAGE_SIZE;
	}

	if (!(va & (TTBL_L2TBL_LARGE_PAGE_SIZE - 1)) &&
	    !(pa & (TTBL_L2TBL_LARGE_PAGE_SIZE - 1)) &&
	    (TTBL_L2TBL_LARGE_PAGE_SIZE <= availsz)) {
		return TTBL_L2TBL_LARGE_PAGE_SIZE;
	}

	return TTBL_L2TBL_SMALL_PAGE_SIZE;
}

int cpu_mmu_get_page(struct cpu_l1tbl *l1, virtual_addr_t va,
		     struct cpu_page *pg)
{
	int ret = VMM_EFAIL;
	u32 *l1_tte, *l2_tte;
	u32 l1_tte_type, l1_sec_type;
	physical_addr_t l2base;
	struct cpu_l2tbl *l2;
	struct cpu_page r;

	/* Sanity check */
	if (!l1) {
		return ret;
	}

	if (!pg) {
		pg = &r;
	}

	l1_tte = (u32 *) (l1->tbl_va +
			  ((va >> TTBL_L1TBL_TTE_OFFSET_SHIFT) << 2));
	l1_tte_type = *l1_tte & TTBL_L1TBL_TTE_TYPE_MASK;

	switch (l1_tte_type) {
	case TTBL_L1TBL_TTE_TYPE_FAULT:
		vmm_memset(pg, 0, sizeof(struct cpu_page));
		break;
	case TTBL_L1TBL_TTE_TYPE_SECTION:
		pg->va = va & TTBL_L1TBL_TTE_OFFSET_MASK;
		pg->ap |= (*l1_tte & TTBL_L1TBL_TTE_AP_MASK) >>
		    TTBL_L1TBL_TTE_AP_SHIFT;
		pg->c = (*l1_tte & TTBL_L1TBL_TTE_C_MASK) >>
		    TTBL_L1TBL_TTE_C_SHIFT;
		pg->b = (*l1_tte & TTBL_L1TBL_TTE_B_MASK) >>
		    TTBL_L1TBL_TTE_B_SHIFT;
		l1_sec_type = (*l1_tte & TTBL_L1TBL_TTE_SECTYPE_MASK) >>
		    TTBL_L1TBL_TTE_SECTYPE_SHIFT;
		if (l1_sec_type) {
			pg->pa = *l1_tte & TTBL_L1TBL_TTE_BASE24_MASK;
			pg->sz = TTBL_L1TBL_SUPSECTION_PAGE_SIZE;
			pg->dom = 0;
		} else {
			pg->pa = *l1_tte & TTBL_L1TBL_TTE_BASE20_MASK;
			pg->sz = TTBL_L1TBL_SECTION_PAGE_SIZE;
			pg->dom = (*l1_tte & TTBL_L1TBL_TTE_DOM_MASK) >>
			    TTBL_L1TBL_TTE_DOM_SHIFT;
		}
		ret = VMM_OK;
		break;
	case TTBL_L1TBL_TTE_TYPE_L2TBL:
		l2base = *l1_tte & TTBL_L1TBL_TTE_BASE10_MASK;
		l2_tte = (u32 *) ((va & ~TTBL_L1TBL_TTE_OFFSET_MASK) >>
				  TTBL_L2TBL_TTE_OFFSET_SHIFT);
		l2 = cpu_mmu_l2tbl_find_tbl_pa(l2base);
		if (l2) {
			l2_tte = (u32 *) (l2->tbl_va + ((u32) l2_tte << 2));
			pg->va = va & TTBL_L2TBL_TTE_BASE12_MASK;
			pg->dom = (*l1_tte & TTBL_L1TBL_TTE_DOM_MASK) >>
			    TTBL_L1TBL_TTE_DOM_SHIFT;
			pg->ap |= (*l2_tte & TTBL_L2TBL_TTE_V5_AP0_MASK) >>
			    TTBL_L2TBL_TTE_V5_AP0_SHIFT;
			pg->c = (*l2_tte & TTBL_L2TBL_TTE_C_MASK) >>
			    TTBL_L2TBL_TTE_C_SHIFT;
			pg->b = (*l2_tte & TTBL_L2TBL_TTE_B_MASK) >>
			    TTBL_L2TBL_TTE_B_SHIFT;
			switch (*l2_tte & TTBL_L2TBL_TTE_TYPE_MASK) {
			case TTBL_L2TBL_TTE_TYPE_LARGE:
				pg->pa = *l2_tte & TTBL_L2TBL_TTE_BASE16_MASK;
				pg->sz = TTBL_L2TBL_LARGE_PAGE_SIZE;
				ret = VMM_OK;
				break;
			case TTBL_L2TBL_TTE_TYPE_SMALL_X:
			case TTBL_L2TBL_TTE_TYPE_SMALL_XN:
				pg->pa = *l2_tte & TTBL_L2TBL_TTE_BASE12_MASK;
				pg->sz = TTBL_L2TBL_SMALL_PAGE_SIZE;
				ret = VMM_OK;
				break;
			default:
				ret = VMM_ENOTAVAIL;
				break;
			}
		}
		break;
	default:
		vmm_memset(pg, 0, sizeof(struct cpu_page));
		ret = VMM_ENOTAVAIL;
		break;
	};

	return ret;
}

int cpu_mmu_unmap_page(struct cpu_l1tbl *l1, struct cpu_page *pg)
{
	int ret = VMM_EFAIL;
	u32 ite, *l1_tte, *l2_tte;
	u32 l1_tte_type, l1_sec_type, found = 0;
	physical_addr_t l2base, pgpa, chkpa;
	virtual_size_t chksz;
	struct cpu_l2tbl *l2 = NULL;

	if (!l1 || !pg) {
		return ret;
	}

	l1_tte = (u32 *) (l1->tbl_va +
			  ((pg->va >> TTBL_L1TBL_TTE_OFFSET_SHIFT) << 2));
	l1_tte_type = *l1_tte & TTBL_L1TBL_TTE_TYPE_MASK;

	found = 0;
	switch (l1_tte_type) {
	case TTBL_L1TBL_TTE_TYPE_FAULT:
		break;
	case TTBL_L1TBL_TTE_TYPE_SECTION:
		l1_sec_type = (*l1_tte & TTBL_L1TBL_TTE_SECTYPE_MASK) >>
		    TTBL_L1TBL_TTE_SECTYPE_SHIFT;
		if (l1_sec_type) {
			l1_tte = l1_tte - ((u32) l1_tte % 64) / 4;
			pgpa = pg->pa & TTBL_L1TBL_TTE_BASE24_MASK;
			chkpa = *l1_tte & TTBL_L1TBL_TTE_BASE24_MASK;
			chksz = TTBL_L1TBL_SUPSECTION_PAGE_SIZE;
			found = 1;
		} else {
			pgpa = pg->pa & TTBL_L1TBL_TTE_BASE20_MASK;
			chkpa = *l1_tte & TTBL_L1TBL_TTE_BASE20_MASK;
			chksz = TTBL_L1TBL_SECTION_PAGE_SIZE;
			found = 2;
		}
		break;
	case TTBL_L1TBL_TTE_TYPE_L2TBL:
		l2base = *l1_tte & TTBL_L1TBL_TTE_BASE10_MASK;
		l2_tte = (u32 *) ((pg->va & ~TTBL_L1TBL_TTE_OFFSET_MASK) >>
				  TTBL_L2TBL_TTE_OFFSET_SHIFT);
		l2 = cpu_mmu_l2tbl_find_tbl_pa(l2base);
		if (l2) {
			l2_tte = (u32 *) (l2->tbl_va + ((u32) l2_tte << 2));
			switch (*l2_tte & TTBL_L2TBL_TTE_TYPE_MASK) {
			case TTBL_L2TBL_TTE_TYPE_LARGE:
				l2_tte = l2_tte - ((u32) l2_tte % 64) / 4;
				pgpa = pg->pa & TTBL_L2TBL_TTE_BASE16_MASK;
				chkpa = *l2_tte & TTBL_L2TBL_TTE_BASE16_MASK;
				chksz = TTBL_L2TBL_LARGE_PAGE_SIZE;
				found = 3;
				break;
			case TTBL_L2TBL_TTE_TYPE_SMALL_X:
			case TTBL_L2TBL_TTE_TYPE_SMALL_XN:
				pgpa = pg->pa & TTBL_L2TBL_TTE_BASE12_MASK;
				chkpa = *l2_tte & TTBL_L2TBL_TTE_BASE12_MASK;
				chksz = TTBL_L2TBL_SMALL_PAGE_SIZE;
				found = 4;
				break;
			default:
				break;
			}
		}
		break;
	default:
		break;
	};

	switch (found) {
	case 1:		/* Super Section */
		if ((pgpa == chkpa) && (pg->sz == chksz)) {
			for (ite = 0; ite < 16; ite++) {
				l1_tte[ite] = 0x0;
				l1->tte_cnt--;
			}
			ret = VMM_OK;
		}
		break;
	case 2:		/* Section */
		if ((pgpa == chkpa) && (pg->sz == chksz)) {
			*l1_tte = 0x0;
			l1->tte_cnt--;
			ret = VMM_OK;
		}
		break;
	case 3:		/* Large Page */
		if ((pgpa == chkpa) && (pg->sz == chksz)) {
			for (ite = 0; ite < 16; ite++) {
				l2_tte[ite] = 0x0;
				l2->tte_cnt--;
			}
			if (!l2->tte_cnt) {
				cpu_mmu_l2tbl_detach(l2);
			}
			ret = VMM_OK;
		}
		break;
	case 4:		/* Small Page */
		if ((pgpa == chkpa) && (pg->sz == chksz)) {
			*l2_tte = 0x0;
			l2->tte_cnt--;
			if (!l2->tte_cnt) {
				cpu_mmu_l2tbl_detach(l2);
			}
			ret = VMM_OK;
		}
		break;
	default:
		break;
	}

	if (!ret) {
		/* If given L1 page table is current then 
		 * invalidate tlb line 
		 */
		if (read_ttbr0() == l1->tbl_pa) {
			invalid_tlb_line(pg->va);
		}
	}

	return ret;
}

int cpu_mmu_map_page(struct cpu_l1tbl *l1, struct cpu_page *pg)
{
	int rc = VMM_OK;
	u32 *l1_tte, *l2_tte;
	u32 ite, l1_tte_type;
	virtual_addr_t pgva;
	virtual_size_t pgsz, minpgsz;
	physical_addr_t l2base;
	struct cpu_page upg;
	struct cpu_l2tbl *l2;

	if (!l1 || !pg) {
		rc = VMM_EFAIL;
		goto mmu_map_return;
	}

	/* get the l1 TBL location */
	l1_tte = (u32 *) (l1->tbl_va +
			  ((pg->va >> TTBL_L1TBL_TTE_OFFSET_SHIFT) << 2));

	/* Get l1 TLB value */
	l1_tte_type = *l1_tte & TTBL_L1TBL_TTE_TYPE_MASK;

	/* if the l1 TBL is already set */
	if (l1_tte_type != TTBL_L1TBL_TTE_TYPE_FAULT) {
		/*
		 * we need to check that the requested area is not already
		 * mapped
		 */
		if (l1_tte_type == TTBL_L1TBL_TTE_TYPE_L2TBL) {
			minpgsz = TTBL_L2TBL_SMALL_PAGE_SIZE;
		} else {
			minpgsz = TTBL_L1TBL_SECTION_PAGE_SIZE;
			rc = VMM_EFAIL;
			goto mmu_map_return;
		}
		pgva = pg->va & ~(pg->sz - 1);
		pgva = pgva & ~(minpgsz - 1);
		pgsz = pg->sz;
		while (pgsz) {
			if (!cpu_mmu_get_page(l1, pgva, &upg)) {
				rc = VMM_EFAIL;
				goto mmu_map_return;
			}
			pgva += minpgsz;
			pgsz = (pgsz < minpgsz) ? 0 : (pgsz - minpgsz);
		}
	} else {
		/* the L1 is not already set */
		switch (pg->sz) {
		case TTBL_L2TBL_LARGE_PAGE_SIZE:	/* 64K Large Page */
		case TTBL_L2TBL_SMALL_PAGE_SIZE:	/* 4K Small Page */
			/*
			 * if small pages requested, then alloc a level 2 
			 * TBL
			 */
			if (!(l2 = cpu_mmu_l2tbl_alloc())) {
				rc = VMM_EFAIL;
				goto mmu_map_return;
			}

			/* And attach it to the L1 TBL */
			if ((rc =
			     cpu_mmu_l2tbl_attach(l1, l2, 0, pg->dom, pg->va,
						  FALSE))) {
				goto mmu_map_return;
			}

			break;
		default:
			break;
		};
	}

	/* Now set up the mapping based on requested page size */
	switch (pg->sz) {
	case TTBL_L1TBL_SECTION_PAGE_SIZE:	/* 1M Section Page */
		*l1_tte = (pg->pa & TTBL_L1TBL_TTE_BASE20_MASK);
		*l1_tte |=
		    (pg->dom << TTBL_L1TBL_TTE_DOM_SHIFT) &
		    TTBL_L1TBL_TTE_DOM_MASK;
		*l1_tte |=
		    (pg->ap << TTBL_L1TBL_TTE_AP_SHIFT) &
		    TTBL_L1TBL_TTE_AP_MASK;
		*l1_tte |=
		    (pg->c << TTBL_L1TBL_TTE_C_SHIFT) & TTBL_L1TBL_TTE_C_MASK;
		*l1_tte |=
		    (pg->b << TTBL_L1TBL_TTE_B_SHIFT) & TTBL_L1TBL_TTE_B_MASK;
		*l1_tte |= TTBL_L1TBL_TTE_REQ_MASK;
		*l1_tte |= TTBL_L1TBL_TTE_TYPE_SECTION;

		l1->tte_cnt++;

		break;
	case TTBL_L2TBL_LARGE_PAGE_SIZE:	/* 64K Large Page */
	case TTBL_L2TBL_SMALL_PAGE_SIZE:	/* 4K Small Page */
		l2base = *l1_tte & TTBL_L1TBL_TTE_BASE10_MASK;

		if (!(l2 = cpu_mmu_l2tbl_find_tbl_pa(l2base))) {
			rc = VMM_EFAIL;
			goto mmu_map_return;
		}

		l2_tte =
		    (u32 *) ((pg->va & ~TTBL_L1TBL_TTE_OFFSET_MASK) >>
			     TTBL_L2TBL_TTE_OFFSET_SHIFT);
		l2_tte = (u32 *) (l2->tbl_va + ((u32) l2_tte << 2));
		if (pg->sz == TTBL_L2TBL_LARGE_PAGE_SIZE) {
			l2_tte -= ((u32) l2_tte % 64) >> 2;
			*l2_tte = (pg->pa & TTBL_L2TBL_TTE_BASE16_MASK);
			*l2_tte |= TTBL_L2TBL_TTE_TYPE_LARGE;
		} else {
			*l2_tte = (pg->pa & TTBL_L2TBL_TTE_BASE12_MASK);
			*l2_tte |= TTBL_L2TBL_TTE_TYPE_SMALL_X;
		}

		*l2_tte |=
		    (pg->ap << TTBL_L2TBL_TTE_V5_AP0_SHIFT) &
		    TTBL_L2TBL_TTE_V5_AP0_MASK;
		*l2_tte |=
		    (pg->ap << TTBL_L2TBL_TTE_V5_AP1_SHIFT) &
		    TTBL_L2TBL_TTE_V5_AP1_MASK;
		*l2_tte |=
		    (pg->ap << TTBL_L2TBL_TTE_V5_AP2_SHIFT) &
		    TTBL_L2TBL_TTE_V5_AP2_MASK;
		*l2_tte |=
		    (pg->ap << TTBL_L2TBL_TTE_V5_AP3_SHIFT) &
		    TTBL_L2TBL_TTE_V5_AP3_MASK;
		*l2_tte |=
		    (pg->c << TTBL_L2TBL_TTE_C_SHIFT) & TTBL_L2TBL_TTE_C_MASK;
		*l2_tte |=
		    (pg->b << TTBL_L2TBL_TTE_B_SHIFT) & TTBL_L2TBL_TTE_B_MASK;

		l2->tte_cnt++;

		if (pg->sz == TTBL_L2TBL_LARGE_PAGE_SIZE) {
			for (ite = 1; ite < 16; ite++) {
				l2_tte[ite] = l2_tte[0];
				l2->tte_cnt++;
			}
		}

		break;
	default:
		break;
	};

 mmu_map_return:
	return rc;
}

static int cpu_mmu_split_reserved_page(struct cpu_page *pg,
				       virtual_size_t rsize)
{
	int rc = VMM_EFAIL;
	int i, count;
	u32 *l2_tte;
	struct cpu_l1tbl *l1;
	struct cpu_l2tbl *l2;
	virtual_addr_t va;
	physical_addr_t pa;

	if (pg == NULL) {
		goto error;
	}

	l1 = &mmuctrl.defl1;

	/* XXX Currently, this function handles only
	 *     Section -> Pages splitting case.
	 */
	/* TODO Add other cases:
	 *        Supersection -> Sections
	 *        Supersection -> Large Pages
	 *        Supersection -> Pages
	 *        Section      -> Large Pages
	 *        Large Page   -> Pages
	 */
	switch (pg->sz) {
	case TTBL_L1TBL_SECTION_PAGE_SIZE:
		switch (rsize) {
		case TTBL_L2TBL_SMALL_PAGE_SIZE:
			count = TTBL_L1TBL_SECTION_PAGE_SIZE /
			    TTBL_L2TBL_SMALL_PAGE_SIZE;

			if (!(l2 = cpu_mmu_l2tbl_alloc())) {
				rc = VMM_EFAIL;
				goto error;
			}

			va = pg->va;
			pa = pg->pa;

			for (i = 0; i < count; i++) {
				l2_tte = (u32 *)
				    ((va & ~TTBL_L1TBL_TTE_OFFSET_MASK) >>
				     TTBL_L2TBL_TTE_OFFSET_SHIFT);
				l2_tte =
				    (u32 *) (l2->tbl_va + ((u32) l2_tte << 2));

				*l2_tte = pa & TTBL_L2TBL_TTE_BASE12_MASK;
				*l2_tte |= TTBL_L2TBL_TTE_TYPE_SMALL_X;
				*l2_tte |=
				    (pg->ap << TTBL_L2TBL_TTE_V5_AP0_SHIFT) &
				    TTBL_L2TBL_TTE_V5_AP0_MASK;
				*l2_tte |=
				    (pg->ap << TTBL_L2TBL_TTE_V5_AP1_SHIFT) &
				    TTBL_L2TBL_TTE_V5_AP1_MASK;
				*l2_tte |=
				    (pg->ap << TTBL_L2TBL_TTE_V5_AP2_SHIFT) &
				    TTBL_L2TBL_TTE_V5_AP2_MASK;
				*l2_tte |=
				    (pg->ap << TTBL_L2TBL_TTE_V5_AP3_SHIFT) &
				    TTBL_L2TBL_TTE_V5_AP3_MASK;
				*l2_tte |=
				    (pg->c << TTBL_L2TBL_TTE_C_SHIFT) &
				    TTBL_L2TBL_TTE_C_MASK;
				*l2_tte |=
				    (pg->b << TTBL_L2TBL_TTE_B_SHIFT) &
				    TTBL_L2TBL_TTE_B_MASK;

				l2->tte_cnt++;

				va += TTBL_L2TBL_SMALL_PAGE_SIZE;
				pa += TTBL_L2TBL_SMALL_PAGE_SIZE;
			}

			cpu_mmu_l2tbl_attach(l1, l2, 0, pg->dom, pg->va, TRUE);

			invalid_tlb();

			break;
		default:
			BUG_ON("%s: Unimplemented (target size 0x%x)\n",
			       __func__, rsize);

			break;
		}
		break;
	default:
		BUG_ON("%s: Unimplemented (source size 0x%x)\n", __func__,
		       pg->sz);
		break;
	}

	rc = VMM_OK;
 error:
	return rc;
}

int cpu_mmu_get_reserved_page(virtual_addr_t va, struct cpu_page *pg)
{
	return cpu_mmu_get_page(&mmuctrl.defl1, va, pg);
}

int cpu_mmu_unmap_reserved_page(struct cpu_page *pg)
{
	int rc = VMM_EFAIL;

	if (pg) {
		rc = cpu_mmu_unmap_page(&mmuctrl.defl1, pg);
	}

	return rc;
}

int cpu_mmu_map_reserved_page(struct cpu_page *pg)
{
	int rc = VMM_EFAIL;

	if (pg) {
		rc = cpu_mmu_map_page(&mmuctrl.defl1, pg);
	}

	return rc;
}

struct cpu_l1tbl *cpu_mmu_l1tbl_alloc(void)
{
	u32 i, *nl1_tte;
	struct cpu_l1tbl *nl1 = NULL;
	struct dlist *le;
	struct cpu_l2tbl *l2, *nl2;

	if (mmuctrl.l1_alloc_count < TTBL_MAX_L1TBL_COUNT) {
		for (i = 0; i < TTBL_MAX_L1TBL_COUNT; i++) {
			if (!mmuctrl.l1_bmap[i]) {
				break;
			}
		}
		if (i == TTBL_MAX_L1TBL_COUNT) {
			return NULL;
		}
		mmuctrl.l1_bmap[i] = 1;
		mmuctrl.l1_alloc_count++;
		nl1 = &mmuctrl.l1_array[i];
	} else {
		return NULL;
	}

	INIT_LIST_HEAD(&nl1->l2tbl_list);
	nl1->tte_cnt = 0;
	nl1->l2tbl_cnt = 0;

	vmm_memcpy((void *)nl1->tbl_va,
		   (void *)mmuctrl.defl1.tbl_va, TTBL_L1TBL_SIZE);
	nl1->tte_cnt = mmuctrl.defl1.tte_cnt;

	list_for_each(le, &mmuctrl.defl1.l2tbl_list) {
		l2 = list_entry(le, struct cpu_l2tbl, head);
		nl1_tte = (u32 *) (nl1->tbl_va +
				   ((l2->map_va >> TTBL_L1TBL_TTE_OFFSET_SHIFT)
				    << 2));
		*nl1_tte = 0x0;
		nl1->tte_cnt--;
		nl2 = cpu_mmu_l2tbl_alloc();
		if (!nl2) {
			goto l1tbl_alloc_fail;
		}
		vmm_memcpy((void *)nl2->tbl_va, (void *)l2->tbl_va,
			   TTBL_L2TBL_SIZE);
		nl2->tte_cnt = l2->tte_cnt;
		if (cpu_mmu_l2tbl_attach
		    (nl1, nl2, 0, l2->domain, l2->map_va, FALSE)) {
			goto l1tbl_alloc_fail;
		}
	}
	nl1->l2tbl_cnt = mmuctrl.defl1.l2tbl_cnt;

	list_add(&mmuctrl.l1tbl_list, &nl1->head);

	return nl1;

 l1tbl_alloc_fail:
	if (nl1) {
		while (!list_empty(&nl1->l2tbl_list)) {
			le = list_pop(&nl1->l2tbl_list);
			nl2 = list_entry(le, struct cpu_l2tbl, head);
			cpu_mmu_l2tbl_free(nl2);
		}
		mmuctrl.l1_bmap[nl1->num] = 0;
		mmuctrl.l1_alloc_count--;
	}

	return NULL;
}

int cpu_mmu_l1tbl_free(struct cpu_l1tbl *l1)
{
	struct dlist *le;
	struct cpu_l2tbl *l2;

	/* Sanity check */
	if (!l1) {
		return VMM_EFAIL;
	}

	if (l1->tbl_pa == mmuctrl.defl1.tbl_pa) {
		return VMM_EFAIL;
	}

	while (!list_empty(&l1->l2tbl_list)) {
		le = list_pop(&l1->l2tbl_list);
		l2 = list_entry(le, struct cpu_l2tbl, head);
		cpu_mmu_l2tbl_free(l2);
	}

	list_del(&l1->head);

	mmuctrl.l1_bmap[l1->num] = 0;
	mmuctrl.l1_alloc_count--;

	return VMM_OK;
}

struct cpu_l1tbl *cpu_mmu_l1tbl_default(void)
{
	return &mmuctrl.defl1;
}

struct cpu_l1tbl *cpu_mmu_l1tbl_current(void)
{
	u32 ttbr0;

	ttbr0 = read_ttbr0();

	return cpu_mmu_l1tbl_find_tbl_pa(ttbr0);
}

u32 cpu_mmu_physical_read32(physical_addr_t pa)
{
	u32 ret = 0x0, ite, found;
	u32 *l1_tte = NULL;
	struct cpu_l1tbl *l1 = NULL;
	virtual_addr_t va = 0x0;
	irq_flags_t flags;

	flags = arch_cpu_irq_save();

	l1 = cpu_mmu_l1tbl_current();
	if (l1) {
		found = 0;
		l1_tte = (u32 *) (l1->tbl_va);
		for (ite = 0; ite < (TTBL_L1TBL_SIZE / 4); ite++) {
			if ((l1_tte[ite] & TTBL_L2TBL_TTE_TYPE_MASK) ==
			    TTBL_L2TBL_TTE_TYPE_FAULT) {
				found = 1;
				break;
			}
		}
		if (found) {
			l1_tte[ite] = 0x0;
			l1_tte[ite] |= (pa & TTBL_L1TBL_TTE_BASE20_MASK);
			l1_tte[ite] |= (TTBL_L1TBL_TTE_DOM_RESERVED <<
					TTBL_L1TBL_TTE_DOM_SHIFT) &
			    TTBL_L1TBL_TTE_DOM_MASK;
			l1_tte[ite] |= (TTBL_AP_SRW_U <<
					TTBL_L1TBL_TTE_AP_SHIFT) &
			    TTBL_L1TBL_TTE_AP_MASK;
			l1_tte[ite] |= (0x0 << TTBL_L1TBL_TTE_C_SHIFT) &
			    TTBL_L1TBL_TTE_C_MASK;
			l1_tte[ite] |= (0x0 << TTBL_L1TBL_TTE_B_SHIFT) &
			    TTBL_L1TBL_TTE_B_MASK;
			l1_tte[ite] |= TTBL_L1TBL_TTE_REQ_MASK;
			l1_tte[ite] |= TTBL_L1TBL_TTE_TYPE_SECTION;
			va = (ite << TTBL_L1TBL_TTE_BASE20_SHIFT) +
			    (pa & ~TTBL_L1TBL_TTE_BASE20_MASK);
			va &= ~0x3;
			ret = *(u32 *) (va);
			l1_tte[ite] = 0x0;
			invalid_tlb_line(va);
		}
	}

	arch_cpu_irq_restore(flags);

	return ret;
}

void cpu_mmu_physical_write32(physical_addr_t pa, u32 val)
{
	u32 ite, found;
	u32 *l1_tte = NULL;
	struct cpu_l1tbl *l1 = NULL;
	virtual_addr_t va = 0x0;
	irq_flags_t flags;

	flags = arch_cpu_irq_save();

	l1 = cpu_mmu_l1tbl_current();
	if (l1) {
		found = 0;
		l1_tte = (u32 *) (l1->tbl_va);
		for (ite = 0; ite < (TTBL_L1TBL_SIZE / 4); ite++) {
			if ((l1_tte[ite] & TTBL_L2TBL_TTE_TYPE_MASK) ==
			    TTBL_L2TBL_TTE_TYPE_FAULT) {
				found = 1;
				break;
			}
		}
		if (found) {
			l1_tte[ite] = 0x0;
			l1_tte[ite] |= (pa & TTBL_L1TBL_TTE_BASE20_MASK);
			l1_tte[ite] |= (TTBL_L1TBL_TTE_DOM_RESERVED <<
					TTBL_L1TBL_TTE_DOM_SHIFT) &
			    TTBL_L1TBL_TTE_DOM_MASK;
			l1_tte[ite] |= (TTBL_AP_SRW_U <<
					TTBL_L1TBL_TTE_AP_SHIFT) &
			    TTBL_L1TBL_TTE_AP_MASK;
			l1_tte[ite] |= (0x0 << TTBL_L1TBL_TTE_C_SHIFT) &
			    TTBL_L1TBL_TTE_C_MASK;
			l1_tte[ite] |= (0x0 << TTBL_L1TBL_TTE_B_SHIFT) &
			    TTBL_L1TBL_TTE_B_MASK;
			l1_tte[ite] |= TTBL_L1TBL_TTE_REQ_MASK;
			l1_tte[ite] |= TTBL_L1TBL_TTE_TYPE_SECTION;
			va = (ite << TTBL_L1TBL_TTE_BASE20_SHIFT) +
			    (pa & ~TTBL_L1TBL_TTE_BASE20_MASK);
			va &= ~0x3;
			*(u32 *) (va) = val;
			l1_tte[ite] = 0x0;
			invalid_tlb_line(va);
		}
	}

	arch_cpu_irq_restore(flags);

	return;
}

int cpu_mmu_chdacr(u32 new_dacr)
{
	u32 old_dacr;

	old_dacr = read_dacr();

	new_dacr &= ~0x3;
	new_dacr |= old_dacr & 0x3;

	if (new_dacr != old_dacr) {
		write_dacr(new_dacr);
	}

	return VMM_OK;
}

int cpu_mmu_chttbr(struct cpu_l1tbl *l1)
{
	u32 sctlr;
	struct cpu_l1tbl *curr_l1;

	/* Sanity check */
	if (!l1) {
		return VMM_EFAIL;
	}

	/* Do nothing if new l1 table is already current l1 table */
	curr_l1 = cpu_mmu_l1tbl_current();
	if (curr_l1 == l1) {
		return VMM_OK;
	}

	sctlr = read_sctlr();

	/* FIXME: Clean & Flush I-Cache if I-Cache enabled */
	if (sctlr & SCTLR_I_MASK) {
	}

	/* FIXME: Clean & Flush D-Cache if D-Cache enabled */
	if (sctlr & SCTLR_C_MASK) {
	}

	/* Invalidate all TLB enteries */
	invalid_tlb();

	/* Update TTBR0 to point to new L1 table */
	write_ttbr0(l1->tbl_pa);

	return VMM_OK;
}

int arch_cpu_aspace_map(virtual_addr_t va,
			virtual_size_t sz, physical_addr_t pa, u32 mem_flags)
{
	static const struct cpu_page zero_filled_cpu_page = { 0 };
	/* Get a 0 filled page struct */
	struct cpu_page p = zero_filled_cpu_page;

	/* initialize the page struct */
	p.pa = pa;
	p.va = va;
	p.sz = sz;
	p.dom = TTBL_L1TBL_TTE_DOM_RESERVED;

	/* For ARMV5 we cannot prevent writing to priviledge mode */
	if (mem_flags & (VMM_MEMORY_READABLE | VMM_MEMORY_WRITEABLE)) {
		p.ap = TTBL_AP_SRW_U;
	} else {
		p.ap = TTBL_AP_S_U;
	}

	if (mem_flags & VMM_MEMORY_EXECUTABLE) {
		p.c = 1;
	}

	return cpu_mmu_map_reserved_page(&p);
}

int arch_cpu_aspace_unmap(virtual_addr_t va, virtual_size_t sz)
{
	int rc;
	struct cpu_page p;

	if ((rc = cpu_mmu_get_reserved_page(va, &p))) {
		return rc;
	}

	if (p.sz > sz) {
		if ((rc = cpu_mmu_split_reserved_page(&p, sz))) {
			return rc;
		}

		if ((rc = cpu_mmu_get_reserved_page(va, &p))) {
			return rc;
		}
	}

	return cpu_mmu_unmap_reserved_page(&p);
}

int arch_cpu_aspace_va2pa(virtual_addr_t va, physical_addr_t * pa)
{
	int rc;
	struct cpu_page p;

	if ((rc = cpu_mmu_get_reserved_page(va, &p)) == VMM_OK) {
		*pa = p.pa + (va & (p.sz - 1));
	}

	return rc;
}

int __init arch_cpu_aspace_init(physical_addr_t * core_resv_pa,
				virtual_addr_t * core_resv_va,
				virtual_size_t * core_resv_sz,
				physical_addr_t * arch_resv_pa,
				virtual_addr_t * arch_resv_va,
				virtual_size_t * arch_resv_sz)
{
	int rc = VMM_OK;
	u32 i, val;
	virtual_addr_t va, resv_va = *core_resv_va;
	virtual_size_t sz, resv_sz = *core_resv_sz;
	physical_addr_t pa, resv_pa = *core_resv_pa;
	struct cpu_page respg;

	/* Reset the memory of MMU control structure */
	vmm_memset(&mmuctrl, 0, sizeof(mmuctrl));

	pa = arch_code_paddr_start();
	va = arch_code_vaddr_start();
	sz = arch_code_size();

	/* Initialize list heads */
	INIT_LIST_HEAD(&mmuctrl.l1tbl_list);
	INIT_LIST_HEAD(&mmuctrl.l2tbl_list);

	/* Handcraft default translation table */
	INIT_LIST_HEAD(&mmuctrl.defl1.l2tbl_list);

	mmuctrl.defl1.tbl_va = (virtual_addr_t) defl1_mem;
	mmuctrl.defl1.tbl_pa = pa + (mmuctrl.defl1.tbl_va - va);

	/* if this is not a virtual = physical config then we invalidate
	 * the initial virtual = physical mapping we set in the early 
	 * boot
	 */
	if (pa != va) {
		val = pa >> TTBL_L1TBL_TTE_OFFSET_SHIFT;
		val = val << 2;
		*((u32 *) (mmuctrl.defl1.tbl_va + val)) = 0x0;
		invalid_tlb();
	}

	/* Now count the number of level 1 mapping not yet set. */
	mmuctrl.defl1.tte_cnt = 0;

	for (i = 0; i < TTBL_L1TBL_SIZE; i += 4) {
		val = *((u32 *) (mmuctrl.defl1.tbl_va + i));
		if ((val & TTBL_L1TBL_TTE_TYPE_MASK) !=
		    TTBL_L1TBL_TTE_TYPE_FAULT) {
			mmuctrl.defl1.tte_cnt++;
		}
	}

	/* for now, no level 2 mapping */
	mmuctrl.defl1.l2tbl_cnt = 0;

	/* Compute additional reserved space required */
	if ((va <= resv_va) && (resv_va < (va + sz))) {
		/* if requested virtual address is within code range, we put it
		 * after the code.
		 */
		resv_va = va + sz;
	} else if ((va <= (resv_va + resv_sz)) &&
		   ((resv_va + resv_sz) < (va + sz))) {
		/* if requested space overlap with code range, we put it after
		 * the code.
		 */
		resv_va = va + sz;
	}

	if ((pa <= resv_pa) && (resv_pa < (pa + sz))) {
		/* if requested physical address is within code range, we put it
		 * after the code.
		 */
		resv_pa = pa + sz;
	} else if ((pa <= (resv_pa + resv_sz)) &&
		   ((resv_pa + resv_sz) < (pa + sz))) {
		/* if requested space overlap with code range, we put it after 
		 * the code.
		 */
		resv_pa = pa + sz;
	}

	/* align resv_va (on 1MB) if needed */
	if (resv_va & TTBL_L1TBL_SECTION_PAGE_MASK) {
		resv_va &= ~TTBL_L1TBL_SECTION_PAGE_MASK;
		resv_va += TTBL_L1TBL_SECTION_PAGE_SIZE;
	}

	/* align resv_pa (on 1MB) if needed */
	if (resv_pa & TTBL_L1TBL_SECTION_PAGE_MASK) {
		resv_pa &= ~TTBL_L1TBL_SECTION_PAGE_MASK;
		resv_pa += TTBL_L1TBL_SECTION_PAGE_SIZE;
	}

	*core_resv_pa = resv_pa;
	*core_resv_va = resv_va;
	*core_resv_sz = resv_sz;

	*arch_resv_pa = resv_pa + resv_sz;
	*arch_resv_va = resv_va + resv_sz;
	*arch_resv_sz = resv_sz;

	/* align the requested size on 4 bytes */
	resv_sz = (resv_sz + 3) & ~0x3;
	/* set the l1_bmap */
	mmuctrl.l1_bmap = (u8 *) (resv_va + resv_sz);
	resv_sz += TTBL_MAX_L1TBL_COUNT;
	/* align the requested size on 4 bytes */
	resv_sz = (resv_sz + 3) & ~0x3;
	mmuctrl.l1_array = (struct cpu_l1tbl *)(resv_va + resv_sz);
	resv_sz += sizeof(struct cpu_l1tbl) * TTBL_MAX_L1TBL_COUNT;
	/* align the requested size on 4 bytes */
	resv_sz = (resv_sz + 3) & ~0x3;
	mmuctrl.l2_bmap = (u8 *) (resv_va + resv_sz);
	resv_sz += TTBL_MAX_L2TBL_COUNT;
	/* align the requested size on 4 bytes */
	resv_sz = (resv_sz + 3) & ~0x3;
	mmuctrl.l2_array = (struct cpu_l2tbl *)(resv_va + resv_sz);
	resv_sz += sizeof(struct cpu_l2tbl) * TTBL_MAX_L2TBL_COUNT;
	/* align the requested size on 4 bytes */
	resv_sz = (resv_sz + 3) & ~0x3;

	/* align the requested size on 4KB */
	if (resv_sz & TTBL_L1TBL_MASK) {
		resv_sz += TTBL_L1TBL_SIZE;
		resv_sz &= ~TTBL_L1TBL_MASK;
	}

	/* set the l1_base address  and size */
	mmuctrl.l1_base_va = resv_va + resv_sz;
	mmuctrl.l1_base_pa = resv_pa + resv_sz;
	resv_sz += TTBL_L1TBL_SIZE * TTBL_MAX_L1TBL_COUNT;

	/* set the l2_base address  and size */
	mmuctrl.l2_base_va = resv_va + resv_sz;
	mmuctrl.l2_base_pa = resv_pa + resv_sz;
	resv_sz += TTBL_L2TBL_SIZE * TTBL_MAX_L2TBL_COUNT;

	/* align the final size on 1 MB if needed */
	if (resv_sz & TTBL_L1TBL_SECTION_PAGE_MASK) {
		resv_sz += TTBL_L1TBL_SECTION_PAGE_SIZE;
		resv_sz &= ~TTBL_L1TBL_SECTION_PAGE_MASK;
	}

	*arch_resv_sz = resv_sz - *arch_resv_sz;

	/* Map space for reserved area */
	/* Everything is mapped as 1 MB sections */
	pa = resv_pa;
	va = resv_va;
	sz = resv_sz;
	while (sz) {
		vmm_memset(&respg, 0, sizeof(respg));
		respg.pa = pa;
		respg.va = va;
		respg.sz = TTBL_L1TBL_SECTION_PAGE_SIZE;
		respg.dom = TTBL_L1TBL_TTE_DOM_RESERVED;
		respg.ap = TTBL_AP_SRW_U;
		respg.c = 1;
		if ((rc = cpu_mmu_map_reserved_page(&respg))) {
			goto mmu_init_error;
		}
		sz -= TTBL_L1TBL_SECTION_PAGE_SIZE;
		pa += TTBL_L1TBL_SECTION_PAGE_SIZE;
		va += TTBL_L1TBL_SECTION_PAGE_SIZE;
	}

	/* Setup up l1 array */
	vmm_memset(mmuctrl.l1_bmap, 0x0, TTBL_MAX_L1TBL_COUNT);
	vmm_memset(mmuctrl.l1_array, 0x0,
		   sizeof(struct cpu_l1tbl) * TTBL_MAX_L1TBL_COUNT);

	for (i = 0; i < TTBL_MAX_L1TBL_COUNT; i++) {
		INIT_LIST_HEAD(&mmuctrl.l1_array[i].head);
		mmuctrl.l1_array[i].num = i;
		mmuctrl.l1_array[i].tbl_pa = mmuctrl.l1_base_pa +
		    i * TTBL_L1TBL_SIZE;
		mmuctrl.l1_array[i].tbl_va = mmuctrl.l1_base_va +
		    i * TTBL_L1TBL_SIZE;
	}

	/* Setup up l2 array */
	vmm_memset(mmuctrl.l2_bmap, 0x0, TTBL_MAX_L2TBL_COUNT);
	vmm_memset(mmuctrl.l2_array, 0x0,
		   sizeof(struct cpu_l2tbl) * TTBL_MAX_L2TBL_COUNT);

	for (i = 0; i < TTBL_MAX_L2TBL_COUNT; i++) {
		INIT_LIST_HEAD(&mmuctrl.l2_array[i].head);
		mmuctrl.l2_array[i].num = i;
		mmuctrl.l2_array[i].tbl_pa = mmuctrl.l2_base_pa +
		    i * TTBL_L2TBL_SIZE;
		mmuctrl.l2_array[i].tbl_va = mmuctrl.l2_base_va +
		    i * TTBL_L2TBL_SIZE;
	}

 mmu_init_error:
	return rc;
}
