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
 * @file gic.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Generic Interrupt Controller Implementation
 */

#include <vmm_error.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <gic.h>

#define max(a,b)	((a) < (b) ? (b) : (a))

struct gic_chip_data {
	u32 irq_offset;
	virtual_addr_t dist_base;
	virtual_addr_t cpu_base;
};

#ifndef GIC_MAX_NR
#define GIC_MAX_NR	1
#endif

static virtual_addr_t gic_cpu_base_addr;
static struct gic_chip_data gic_data[GIC_MAX_NR];

#define gic_write(val, addr)	vmm_writel((val), (void *)(addr))
#define gic_read(addr)		vmm_readl((void *)(addr))

static inline virtual_addr_t gic_dist_base(struct vmm_host_irq *irq)
{
	struct gic_chip_data *gic_data = vmm_host_irq_get_chip_data(irq);
	return gic_data->dist_base;
}

static inline virtual_addr_t gic_cpu_base(struct vmm_host_irq *irq)
{
	struct gic_chip_data *gic_data = vmm_host_irq_get_chip_data(irq);
	return gic_data->cpu_base;
}

static inline u32 gic_irq(struct vmm_host_irq *irq)
{
	struct gic_chip_data *gic_data = vmm_host_irq_get_chip_data(irq);
	return irq->num - gic_data->irq_offset;
}

u32 gic_active_irq(u32 gic_nr)
{
	u32 ret;

	if (GIC_MAX_NR <= gic_nr) {
		return 0xFFFFFFFF;
	}

	ret = gic_read(gic_data[gic_nr].cpu_base + GIC_CPU_INTACK) & 0x3FF;
	ret += gic_data[gic_nr].irq_offset;

	return ret;
}

void gic_eoi_irq(struct vmm_host_irq *irq)
{
	gic_write(gic_irq(irq), gic_cpu_base(irq) + GIC_CPU_EOI);
}

void gic_mask_irq(struct vmm_host_irq *irq)
{
	gic_write(1 << (irq->num % 32), gic_dist_base(irq) +
		  GIC_DIST_ENABLE_CLEAR + (gic_irq(irq) / 32) * 4);
}

void gic_unmask_irq(struct vmm_host_irq *irq)
{
	gic_write(1 << (irq->num % 32), gic_dist_base(irq) +
		  GIC_DIST_ENABLE_SET + (gic_irq(irq) / 32) * 4);
}

static struct vmm_host_irq_chip gic_chip = {
	.name			= "GIC",
	.irq_mask		= gic_mask_irq,
	.irq_unmask		= gic_unmask_irq,
	.irq_eoi		= gic_eoi_irq,
};

void __init gic_dist_init(struct gic_chip_data *gic, u32 irq_start)
{
	unsigned int max_irq, irq_limit, i;
	u32 cpumask = 1 << 0;	/* FIXME: smp_processor_id(); */
	virtual_addr_t base = gic->dist_base;

	cpumask |= cpumask << 8;
	cpumask |= cpumask << 16;

	/* Disable IRQ distribution */
	gic_write(0, base + GIC_DIST_CTRL);

	/*
	 * Find out how many interrupts are supported.
	 */
	max_irq = gic_read(base + GIC_DIST_CTR) & 0x1f;
	max_irq = (max_irq + 1) * 32;

	/*
	 * The GIC only supports up to 1020 interrupt sources.
	 * Limit this to either the architected maximum, or the
	 * platform maximum.
	 */
	if (max_irq > 1020) {
		max_irq = 1020;
	}

	/*
	 * Set all global interrupts to be level triggered, active low.
	 */
	for (i = 32; i < max_irq; i += 16) {
		gic_write(0, base + GIC_DIST_CONFIG + i * 4 / 16);
	}

	/*
	 * Set all global interrupts to this CPU only.
	 */
	for (i = 32; i < max_irq; i += 4) {
		gic_write(cpumask, base + GIC_DIST_TARGET + i * 4 / 4);
	}

	/*
	 * Set priority on all interrupts.
	 */
	for (i = 0; i < max_irq; i += 4) {
		gic_write(0xa0a0a0a0, base + GIC_DIST_PRI + i * 4 / 4);
	}

	/*
	 * Disable all interrupts.
	 */
	for (i = 0; i < max_irq; i += 32) {
		gic_write(0xffffffff,
			  base + GIC_DIST_ENABLE_CLEAR + i * 4 / 32);
	}

	/*
	 * Limit number of interrupts registered to the platform maximum
	 */
	irq_limit = gic->irq_offset + max_irq;
	if (irq_limit > GIC_NR_IRQS) {
		irq_limit = GIC_NR_IRQS;
	}

	/*
	 * Setup the Host IRQ subsystem.
	 */
	for (i = irq_start; i < irq_limit; i++) {
		vmm_host_irq_set_chip(i, &gic_chip);
		vmm_host_irq_set_chip_data(i, gic);
	}

	/* Enable IRQ distribution */
	gic_write(1, base + GIC_DIST_CTRL);
}

void __init gic_cpu_init(struct gic_chip_data *gic)
{
	int i;

	/*
	 * Deal with the banked PPI and SGI interrupts - disable all
	 * PPI interrupts, ensure all SGI interrupts are enabled.
	 */
	gic_write(0xffff0000, gic->dist_base + GIC_DIST_ENABLE_CLEAR);
	gic_write(0x0000ffff, gic->dist_base + GIC_DIST_ENABLE_SET);

	/*
	 * Set priority on PPI and SGI interrupts
	 */
	for (i = 0; i < 32; i += 4) {
		gic_write(0xa0a0a0a0, 
			  gic->dist_base + GIC_DIST_PRI + i * 4 / 4);
	}

	gic_write(0xf0, gic->cpu_base + GIC_CPU_PRIMASK);
	gic_write(1, gic->cpu_base + GIC_CPU_CTRL);
}

int __init gic_init(u32 gic_nr, u32 irq_start, 
		    virtual_addr_t cpu_base, virtual_addr_t dist_base)
{
	struct gic_chip_data *gic;

	if (gic_nr >= GIC_MAX_NR) {
		return VMM_EFAIL;
	}

	gic = &gic_data[gic_nr];
	gic->dist_base = dist_base;
	gic->cpu_base = cpu_base;
	gic->irq_offset = (irq_start - 1) & ~31;

	if (gic_nr == 0) {
		gic_cpu_base_addr = cpu_base;
	}

	gic_dist_init(gic, irq_start);
	gic_cpu_init(gic);

	return VMM_OK;
}

