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
 * @file brd_smp.c
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief board specific smp functions
 */

#include <vmm_error.h>
#include <vmm_types.h>
#include <vmm_smp.h>
#include <vmm_stdio.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_host_aspace.h>
#include <vmm_compiler.h>
#include <libs/libfdt.h>
#include <smp_scu.h>
#include <gic.h>

#include <exynos/mach/map.h>

static virtual_addr_t scu_base;
static virtual_addr_t pmu_base;

int __init arch_smp_init_cpus(void)
{
	u32 ncores;
	int i, rc = VMM_OK;
	struct vmm_devtree_node *node;

	/* Get the PMU node in the dev tree */
	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING "pmu");
	if (!node) {
		return VMM_EFAIL;
	}

	/* Map the PMU physical address to virtual address */
	rc = vmm_devtree_regmap(node, &pmu_base, 0);
	if (rc) {
		return rc;
	}

	/* Get the SCU node in the dev tree */
	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING "scu");
	if (!node) {
		return VMM_EFAIL;
	}

	/* Map the SCU physical address to virtual address */
	rc = vmm_devtree_regmap(node, &scu_base, 0);
	if (rc) {
		return rc;
	}

	/* How many ARM core do we have */
	ncores = scu_get_core_count((void *)scu_base);

	/* Update the cpu_possible bitmap based on SCU */
	for (i = 0; i < CONFIG_CPU_COUNT; i++) {
		if ((i < ncores) &&
		    scu_cpu_core_is_smp((void *)scu_base, i)) {
			vmm_set_cpu_possible(i, TRUE);
		}
	}

	return VMM_OK;
}

extern u8 _start_secondary;

int __init arch_smp_prepare_cpus(unsigned int max_cpus)
{
	int i, rc;
	physical_addr_t _start_secondary_pa;

	/* Get physical address secondary startup code */
	rc = vmm_host_va2pa((virtual_addr_t)&_start_secondary, 
			    &_start_secondary_pa);
	if (rc) {
		return rc;
	}

	/* Update the cpu_present bitmap */
	for (i = 0; i < max_cpus; i++) {
		vmm_set_cpu_present(i, TRUE);
	}

	if (scu_base) {
		/* Enable snooping through SCU */
		scu_enable((void *)scu_base);
	}

	if (pmu_base) {
		/* Write the entry address for the secondary cpus */
		vmm_writel((u32)_start_secondary_pa, (void *)pmu_base + 0x814);
	}

	return VMM_OK;
}

int __init arch_smp_start_cpu(u32 cpu)
{
	const struct vmm_cpumask *mask = get_cpu_mask(cpu);

	/* Wakeup target cpu from wfe/wfi by sending an IPI */
	gic_raise_softirq(mask, 0);

	return VMM_OK;
}

static vmm_irq_return_t smp_ipi_handler(int irq_no, void *dev)
{
	/* Call core code to handle IPI1 */
	vmm_smp_ipi_exec();	

	return VMM_IRQ_HANDLED;
}

void arch_smp_ipi_trigger(const struct vmm_cpumask *dest)
{
	/* Send IPI1 to other cores */
	gic_raise_softirq(dest, 1);
}

int __cpuinit arch_smp_ipi_init(void)
{
	int rc;
	u32 cpu = vmm_smp_processor_id();

	if (!cpu) {
		/* Register IPI1 interrupt handler */
		rc = vmm_host_irq_register(1, "IPI1", &smp_ipi_handler, NULL);
		if (rc) {
			return rc;
		}

		/* Mark IPI1 interrupt as per-cpu */
		rc = vmm_host_irq_mark_per_cpu(1);
		if (rc) {
			return rc;
		}
	}

	/* Explicitly enable IPI1 interrupt */
	gic_enable_ppi(1);

	return VMM_OK;
}
