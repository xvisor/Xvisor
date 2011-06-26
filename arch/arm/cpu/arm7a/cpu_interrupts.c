/**
 * Copyright (c) 2011 Pranav Sawargaonkar.
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
 * @file cpu_interrupts.c
 * @version 1.0
 * @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @author Anup Patel (anup@brainfault.org)
 * @brief source code for handling cpu interrupts
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_guest.h>
#include <vmm_devtree.h>
#include <vmm_host_irq.h>
#include <vmm_vcpu_irq.h>
#include <vmm_scheduler.h>
#include <cpu_inline_asm.h>
#include <cpu_vcpu_emulate_arm.h>
#include <cpu_vcpu_emulate_thumb.h>
#include <cpu_vcpu_cp15.h>
#include <cpu_defines.h>

void do_undefined_instruction(vmm_user_regs_t * uregs)
{
	int rc = VMM_OK;
	vmm_vcpu_t * vcpu;

	if ((uregs->cpsr & CPSR_MODE_MASK) != CPSR_MODE_USER) {
		vmm_panic("%s: unexpected exception\n", __func__);
	}

	vcpu = vmm_scheduler_current_vcpu();

	/* If vcpu priviledge is user then generate exception 
	 * and return without emulating instruction 
	 */
	if ((vcpu->sregs.cpsr & CPSR_MODE_MASK) == CPSR_MODE_USER) {
		vmm_vcpu_irq_assert(vcpu, CPU_UNDEF_INST_IRQ, 0x0);
	} else {
		if (uregs->cpsr & CPSR_THUMB_ENABLED) {
			rc = cpu_vcpu_emulate_thumb_inst(vcpu, uregs, FALSE);
		} else {
			rc = cpu_vcpu_emulate_arm_inst(vcpu, uregs, FALSE);
		}
	}

	if (rc) {
		vmm_printf("%s: error %d\n", __func__, rc);
	}

	vmm_vcpu_irq_process(uregs);
}

void do_software_interrupt(vmm_user_regs_t * uregs)
{
	int rc = VMM_OK;
	vmm_vcpu_t * vcpu;

	if ((uregs->cpsr & CPSR_MODE_MASK) != CPSR_MODE_USER) {
		vmm_panic("%s: unexpected exception\n", __func__);
	}

	vcpu = vmm_scheduler_current_vcpu();

	/* If vcpu priviledge is user then generate exception 
	 * and return without emulating instruction 
	 */
	if ((vcpu->sregs.cpsr & CPSR_MODE_MASK) == CPSR_MODE_USER) {
		vmm_vcpu_irq_assert(vcpu, CPU_SOFT_IRQ, 0x0);
	} else {
		if (uregs->cpsr & CPSR_THUMB_ENABLED) {
			rc = cpu_vcpu_emulate_thumb_inst(vcpu, uregs, TRUE);
		} else {
			rc = cpu_vcpu_emulate_arm_inst(vcpu, uregs, TRUE);
		}
	}

	if (rc) {
		vmm_printf("%s: error %d\n", __func__, rc);
	}

	vmm_vcpu_irq_process(uregs);
}

void do_prefetch_abort(vmm_user_regs_t * uregs)
{
	int rc = VMM_EFAIL;
	u32 ifsr, ifar, fs;
	vmm_vcpu_t * vcpu;

	if ((uregs->cpsr & CPSR_MODE_MASK) != CPSR_MODE_USER) {
		vmm_panic("%s: unexpected exception\n", __func__);
	}

	ifsr = read_ifsr();
	ifar = read_ifar();
	vcpu = vmm_scheduler_current_vcpu();

	fs = (ifsr & IFSR_FS4_MASK) >> IFSR_FS4_SHIFT;
	fs = (fs << 4) | (ifsr & IFSR_FS_MASK);

	switch(fs) {
	case IFSR_FS_TTBL_WALK_SYNC_EXT_ABORT_1:
	case IFSR_FS_TTBL_WALK_SYNC_EXT_ABORT_2:
		break;
	case IFSR_FS_TTBL_WALK_SYNC_PARITY_ERROR_1:
	case IFSR_FS_TTBL_WALK_SYNC_PARITY_ERROR_2:
		break;
	case IFSR_FS_TRANS_FAULT_SECTION:
		rc = cpu_vcpu_cp15_trans_fault(vcpu, uregs, ifar, 0, 0, 0);
		break;
	case IFSR_FS_TRANS_FAULT_PAGE:
		rc = cpu_vcpu_cp15_trans_fault(vcpu, uregs, ifar, 0, 1, 0);
		break;
	case IFSR_FS_ACCESS_FAULT_SECTION:
		rc = cpu_vcpu_cp15_access_fault(vcpu, uregs, ifar, 0, 0, 0);
		break;
	case IFSR_FS_ACCESS_FAULT_PAGE:
		rc = cpu_vcpu_cp15_access_fault(vcpu, uregs, ifar, 0, 1, 0);
		break;
	case IFSR_FS_DOMAIN_FAULT_SECTION:
		rc = cpu_vcpu_cp15_domain_fault(vcpu, uregs, ifar, 0, 0, 0);
		break;
	case IFSR_FS_DOMAIN_FAULT_PAGE:
		rc = cpu_vcpu_cp15_domain_fault(vcpu, uregs, ifar, 0, 1, 0);
		break;
	case IFSR_FS_PERM_FAULT_SECTION:
		rc = cpu_vcpu_cp15_perm_fault(vcpu, uregs, ifar, 0, 0, 0);
		break;
	case IFSR_FS_PERM_FAULT_PAGE:
		rc = cpu_vcpu_cp15_perm_fault(vcpu, uregs, ifar, 0, 1, 0);
		break;
	case IFSR_FS_DEBUG_EVENT:
	case IFSR_FS_SYNC_EXT_ABORT:
	case IFSR_FS_IMP_VALID_LOCKDOWN:
	case IFSR_FS_IMP_VALID_COPROC_ABORT:
	case IFSR_FS_MEM_ACCESS_SYNC_PARITY_ERROR:
		break;
	default:
		break; 
	};

	if (rc) {
		vmm_printf("%s: error %d\n", __func__, rc);
	}

	vmm_vcpu_irq_process(uregs);
}

void do_data_abort(vmm_user_regs_t * uregs)
{
	int rc = VMM_EFAIL;
	u32 dfsr, dfar, fs, wnr;
	vmm_vcpu_t * vcpu;

	if ((uregs->cpsr & CPSR_MODE_MASK) != CPSR_MODE_USER) {
		vmm_panic("%s: unexpected exception\n", __func__);
	}

	dfsr = read_dfsr();
	dfar = read_dfar();
	vcpu = vmm_scheduler_current_vcpu();

	fs = (dfsr & DFSR_FS4_MASK) >> DFSR_FS4_SHIFT;
	fs = (fs << 4) | (dfsr & DFSR_FS_MASK);
	wnr = (dfsr & DFSR_WNR_MASK) >> DFSR_WNR_SHIFT;

	switch(fs) {
	case DFSR_FS_ALIGN_FAULT:
		break;
	case DFSR_FS_ICACHE_MAINT_FAULT:
		break;
	case DFSR_FS_TTBL_WALK_SYNC_EXT_ABORT_1:
	case DFSR_FS_TTBL_WALK_SYNC_EXT_ABORT_2:
		break;
	case DFSR_FS_TTBL_WALK_SYNC_PARITY_ERROR_1:
	case DFSR_FS_TTBL_WALK_SYNC_PARITY_ERROR_2:
		break;
	case DFSR_FS_TRANS_FAULT_SECTION:
		rc = cpu_vcpu_cp15_trans_fault(vcpu, uregs, dfar, wnr, 0, 1);
		break;
	case DFSR_FS_TRANS_FAULT_PAGE:
		rc = cpu_vcpu_cp15_trans_fault(vcpu, uregs, dfar, wnr, 1, 1);
		break;
	case DFSR_FS_ACCESS_FAULT_SECTION:
		rc = cpu_vcpu_cp15_access_fault(vcpu, uregs, dfar, wnr, 0, 1);
		break;
	case DFSR_FS_ACCESS_FAULT_PAGE:
		rc = cpu_vcpu_cp15_access_fault(vcpu, uregs, dfar, wnr, 1, 1);
		break;
	case DFSR_FS_DOMAIN_FAULT_SECTION:
		rc = cpu_vcpu_cp15_domain_fault(vcpu, uregs, dfar, wnr, 0, 1);
		break;
	case DFSR_FS_DOMAIN_FAULT_PAGE:
		rc = cpu_vcpu_cp15_domain_fault(vcpu, uregs, dfar, wnr, 1, 1);
		break;
	case DFSR_FS_PERM_FAULT_SECTION:
		rc = cpu_vcpu_cp15_perm_fault(vcpu, uregs, dfar, wnr, 0, 1);
		break;
	case DFSR_FS_PERM_FAULT_PAGE:
		rc = cpu_vcpu_cp15_perm_fault(vcpu, uregs, dfar, wnr, 1, 1);
		break;
	case DFSR_FS_DEBUG_EVENT:
	case DFSR_FS_SYNC_EXT_ABORT:
	case DFSR_FS_IMP_VALID_LOCKDOWN:
	case DFSR_FS_IMP_VALID_COPROC_ABORT:
	case DFSR_FS_MEM_ACCESS_SYNC_PARITY_ERROR:
	case DFSR_FS_ASYNC_EXT_ABORT:
	case DFSR_FS_MEM_ACCESS_ASYNC_PARITY_ERROR:
		break;
	default:
		break;
	};

	if (rc) {
		vmm_printf("%s: error %d\n", __func__, rc);
	}

	vmm_vcpu_irq_process(uregs);
}

void do_not_used(vmm_user_regs_t * uregs)
{
	vmm_panic("%s: unexpected exception\n", __func__);
}

void do_irq(vmm_user_regs_t * uregs)
{
	vmm_host_irq_exec(CPU_EXTERNAL_IRQ, uregs);

	vmm_vcpu_irq_process(uregs);
}

void do_fiq(vmm_user_regs_t * uregs)
{
	vmm_host_irq_exec(CPU_EXTERNAL_FIQ, uregs);

	vmm_vcpu_irq_process(uregs);
}

int vmm_cpu_irq_setup(void)
{
	extern u32 _start_vect[];
	u32 *vectors, *vectors_data;
	u32 highvec_enable, vec;
	vmm_devtree_node_t *node;
	const char *attrval;

	/* Get the vmm information node */
	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPRATOR_STRING
				   VMM_DEVTREE_VMMINFO_NODE_NAME);
	if (!node) {
		return VMM_EFAIL;
	}

	/* Determine value of highvec_enable attribute */
	attrval = vmm_devtree_attrval(node, CPU_HIGHVEC_ENABLE_ATTR_NAME);
	if (!attrval) {
		return VMM_EFAIL;
	}
	highvec_enable = *((u32 *) attrval);

	if (highvec_enable) {
		/* Enable high vectors in SCTLR */
		write_sctlr(read_sctlr() | SCTLR_V_MASK);
		vectors = (u32 *) CPU_IRQ_HIGHVEC_BASE;
	} else {
		vectors = (u32 *) CPU_IRQ_LOWVEC_BASE;
	}
	vectors_data = vectors + CPU_IRQ_NR;

	/* If vectors are tat correct location then do nothing */
	if ((u32) _start_vect == (u32) vectors) {
		return VMM_OK;
	}

	/*
	 * Loop through the vectors we're taking over, and copy the
	 * vector's insn and data word.
	 */
	for (vec = 0; vec < CPU_IRQ_NR; vec++) {
		vectors[vec] = _start_vect[vec];
		vectors_data[vec] = _start_vect[vec + CPU_IRQ_NR];
	}

	return VMM_OK;
}

void vmm_cpu_irq_enable(void)
{
	__asm("cpsie i");
}

void vmm_cpu_irq_disable(void)
{
	__asm("cpsid i");
}

irq_flags_t vmm_cpu_irq_save(void)
{
	unsigned long retval;

	asm volatile (" mrs     %0, cpsr\n\t" " cpsid   i"	/* Syntax CPSID <iflags> {, #<p_mode>}
								 * Note: This instruction is supported 
								 * from ARM6 and above
								 */
		      :"=r" (retval)::"memory", "cc");

	return retval;
}

void vmm_cpu_irq_restore(irq_flags_t flags)
{
	asm volatile (" msr     cpsr_c, %0"::"r" (flags)
		      :"memory", "cc");
}

