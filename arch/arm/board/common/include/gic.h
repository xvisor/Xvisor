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
 * @file gic.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Generic Interrupt Controller Interface
 */
#ifndef __GIC_H__
#define __GIC_H__

#include <vmm_types.h>
#include <vmm_cpumask.h>
#include <vmm_devtree.h>

/* Include GIC configuration defines/macros 
 * provided by board specific code 
 */
#include <gic_config.h>

#define GIC_CPU_CTRL			0x00
#define GIC_CPU_PRIMASK			0x04
#define GIC_CPU_BINPOINT		0x08
#define GIC_CPU_INTACK			0x0c
#define GIC_CPU_EOI			0x10
#define GIC_CPU_RUNNINGPRI		0x14
#define GIC_CPU_HIGHPRI			0x18

#define GIC_DIST_CTRL			0x000
#define GIC_DIST_CTR			0x004
#define GIC_DIST_ENABLE_SET		0x100
#define GIC_DIST_ENABLE_CLEAR		0x180
#define GIC_DIST_PENDING_SET		0x200
#define GIC_DIST_PENDING_CLEAR		0x280
#define GIC_DIST_ACTIVE_BIT		0x300
#define GIC_DIST_PRI			0x400
#define GIC_DIST_TARGET			0x800
#define GIC_DIST_CONFIG			0xc00
#define GIC_DIST_SOFTINT		0xf00

u32 gic_active_irq(u32 gic_nr);

void gic_enable_ppi(u32 irq);

#if defined(CONFIG_SMP)
void gic_raise_softirq(const struct vmm_cpumask *mask, u32 irq);
#endif

void gic_cascade_irq(u32 gic_nr, u32 irq);

int gic_init_bases(u32 gic_nr, u32 irq_start, 
		   virtual_addr_t cpu_base, 
		   virtual_addr_t dist_base);

void gic_secondary_init(u32 gic_nr);

int gic_devtree_init(struct vmm_devtree_node *node, 
		     struct vmm_devtree_node *parent);

#endif /* __GIC_H__ */
