/**
 * Copyright (c) 2011 Anup Patel.
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
 * @file cpu_init.c
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief intialization functions for CPU
 */

#include <vmm_cpu.h>
#include <vmm_main.h>
#include <vmm_error.h>
#include <cpu_mmu.h>

int vmm_cpu_early_init(void)
{
	/*
	 * Host virtual memory, device tree, heap is up.
	 * Do necessary early stuff like iomapping devices
	 * memory or boot time memory reservation here.
	 */
	return VMM_OK;
}

int vmm_cpu_final_init(void)
{
	/** FIXME: Final initialzation code for CPU */
	/* All VMM API's are available here */
	/* We can register a CPU specific resources here */
	return VMM_OK;
}

void cpu_init(void)
{
	/** Initialize VMM (APIs only available after this) */
	vmm_init();

	/** Start VMM */
	vmm_start();
}
