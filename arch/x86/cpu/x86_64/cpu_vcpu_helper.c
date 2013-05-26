/**
 * Copyright (c) 2011 Himanshu Chauhan
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
 * @file cpu_vcpu_helper.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief source of VCPU helper functions
 */

#include <arch_cpu.h>
#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_manager.h>
#include <vmm_guest_aspace.h>
#include <cpu_mmu.h>
#include <libs/stringlib.h>

extern char _stack_start;

#define VMM_REGION_TYPE_ROM	0
#define VMM_REGION_TYPE_RAM	1

int arch_guest_init(struct vmm_guest * guest)
{
	return VMM_OK;
}

int arch_guest_deinit(struct vmm_guest * guest)
{
	return VMM_OK;
}

int arch_vcpu_init(struct vmm_vcpu *vcpu)
{
	u64 stack_start;

	if (!vcpu->is_normal) {
		/* For orphan vcpu */
		stack_start = vcpu->stack_va + vcpu->stack_sz - sizeof(u64);
		vcpu->regs.rip = vcpu->start_pc;
		vcpu->regs.rip = vcpu->start_pc;
		vcpu->regs.rsp = stack_start;
		vcpu->regs.cs = VMM_CODE_SEG_SEL;
		vcpu->regs.ss = VMM_DATA_SEG_SEL;
		vcpu->regs.rflags = (X86_EFLAGS_IF | X86_EFLAGS_PF | X86_EFLAGS_CF);
	} else {
		vmm_panic("Non orphan VCPU intialization not supported yet.\n");
	}

	return VMM_OK;
}

int arch_vcpu_deinit(struct vmm_vcpu * vcpu)
{
	return VMM_OK;
}

void dump_vcpu_regs(arch_regs_t *regs)
{
	vmm_printf("rax: %lx rbx: %lx rcx: %lx rdx: %lx\n", regs->rax, regs->rbx, regs->rcx, regs->rdx);
	vmm_printf("rdi: %lx rsi: %lx rbp: %lx r8 : %lx\n", regs->rdi, regs->rsi, regs->rbp, regs->r8);
	vmm_printf("r9 : %lx r10: %lx r11: %lx r12: %lx\n", regs->r9, regs->r10, regs->r11, regs->r12);
	vmm_printf("r13: %lx r14: %lx r15: %lx\n", regs->r13, regs->r14, regs->r15);
	vmm_printf("rip: %lx rsp: %lx rflags: %lx hwec: %lx\n", regs->rip, regs->rsp, regs->rflags, regs->hw_err_code);
	vmm_printf("ss: %lx cs: %lx\n",regs->ss, regs->cs);
}

void arch_vcpu_switch(struct vmm_vcpu *tvcpu, 
		      struct vmm_vcpu *vcpu,
		      arch_regs_t *regs)
{
	if (!tvcpu) {
		/* first time rescheduling */
		memcpy(regs, &vcpu->regs, sizeof(arch_regs_t));
	} else {
		memcpy(&tvcpu->regs, regs, sizeof(arch_regs_t));
		memcpy(regs, &vcpu->regs, sizeof(arch_regs_t));
	}
}

void arch_vcpu_preempt_orphan(void)
{
	/* Trigger system call from hypervisor. This will
	 * cause do_generic_int_handler() function to call 
	 * vmm_scheduler_preempt_orphan()
	 */
	asm volatile ("int $0x80\t\n");
}

void arch_vcpu_regs_dump(struct vmm_vcpu *vcpu) 
{
	dump_vcpu_regs(&vcpu->regs);
}

void arch_vcpu_stat_dump(struct vmm_vcpu *vcpu) 
{
}
