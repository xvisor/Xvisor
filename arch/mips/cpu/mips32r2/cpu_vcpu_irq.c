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
 * @file cpu_vcpu_irq.c
 * @version 0.1
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief source code for handling vcpu interrupts
 */

#include <vmm_error.h>
#include <vmm_cpu.h>
#include <vmm_vcpu_irq.h>

u32 vmm_vcpu_irq_count(vmm_vcpu_t * vcpu)
{
	return 7;
}

u32 vmm_vcpu_irq_priority(vmm_vcpu_t * vcpu, u32 irq_no)
{
	/* all at same priority */
	return 1;
}

int vmm_vcpu_irq_execute(vmm_vcpu_t * vcpu,
			 vmm_user_regs_t * regs, 
			 u32 irq_no, u32 reason)
{
	return VMM_OK;
}
