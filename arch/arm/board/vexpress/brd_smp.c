/**
 * Copyright (c) 2012 Anup Patel.
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
 * @file brd_smp.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief board specific smp functions
 */

#include <vmm_error.h>
#include <vmm_types.h>
#include <vmm_stdio.h>
#include <vmm_host_io.h>
#include <vmm_compiler.h>
#include <libs/libfdt.h>
#include <motherboard.h>
#include <smp_scu.h>
#include <gic.h>

int __init arch_smp_prepare_cpus(void)
{
	int rc = VMM_OK;
#ifdef CONFIG_ARM_SCU
	struct vmm_devtree_node *node;
	virtual_addr_t ca9_scu_base;
	u32 ncores;
	int i;

	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
				   VMM_DEVTREE_HOSTINFO_NODE_NAME
				   VMM_DEVTREE_PATH_SEPARATOR_STRING "scu");
	if (!node) {
		return VMM_EFAIL;
	}
	rc = vmm_devtree_regmap(node, &ca9_scu_base, 0);
	if (rc) {
		return rc;
	}

	ncores = scu_get_core_count((void *)ca9_scu_base);

	/* Find out the number of SMP-enabled cpu cores */
	for (i = 0; i < CONFIG_CPU_COUNT; i++) {
		if ((i >= ncores)
		    || !scu_cpu_core_is_smp((void *)ca9_scu_base, i)) {
			/* Update the cpu_possible bitmap */
			vmm_set_cpu_possible(i, 0);
		}
	}

	/* Enable snooping through SCU */
	scu_enable((void *)ca9_scu_base);

	rc = vmm_devtree_regunmap(node, ca9_scu_base, 0);
#endif

	return rc;
}

extern unsigned long _load_start;

int __init arch_smp_start_cpu(u32 cpu)
{
#ifdef CONFIG_CPU_CORTEX_A9
	const struct vmm_cpumask *mask = get_cpu_mask(cpu);

	if (cpu == 0) {
		return VMM_OK;
	}

	/* Write the entry address for the secondary cpus */
	v2m_flags_set((u32) _load_start);

	/* Wakeup target cpu from wfe/wfi by sending an IPI */
	gic_raise_softirq(mask, 0);
#endif

	return VMM_OK;
}
