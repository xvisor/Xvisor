/**
 * Copyright (c) 2010 Himanshu Chauhan.
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
 * @file arch_regs.h
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief common header file for CPU registers
 */
#ifndef _ARCH_REGS_H__
#define _ARCH_REGS_H__

#include "vmm_types.h"
#include "cpu_regs.h"
#include "cpu_mmu.h"

struct arch_regs {
        u32 regs[CPU_GPR_COUNT];
	u32 cp0_epc; /* EPC store here at time of interrupt or exception */
	u32 cp0_status; /* status as what should be while returning from INT. */
	u32 cp0_entryhi;
};

typedef struct arch_regs arch_regs_t;

struct mips_super_regs {
	/**
	 * XXX: We assume that machine doesn't have
	 * any other coprocessor.
	 */
	u32 cp0_regs[CP0_REG_COUNT];

	/* Keep track of actual hardware TLB entries */
	mips32_tlb_entry_t hw_tlb_entries[CPU_TLB_COUNT];

	/* Keep track of tlb entries as seen by the guest */
	mips32_tlb_entry_t v_tlb_entries[CPU_TLB_COUNT];

	/* Shadow TLB entries. These serve as lookup cache for guest faults */
	mips32_tlb_entry_t shadow_tlb_entries[2 * CPU_TLB_COUNT];

} __attribute ((packed));

typedef struct mips_super_regs mips_super_regs_t;

#define TLB_NOT_IN_HW	((s16)-1)
#define TLB_FREE	((s16)-1)

#define mips_uregs(vcpu)		(&((vcpu)->regs))
#define mips_sregs(vcpu)		((mips_super_regs_t *)((vcpu)->arch_priv))

#endif
