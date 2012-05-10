/**
 * Copyright (c) 2012 Sukanto Ghosh.
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
 * @file arm_gic.h
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief ARM Generic Interrupt Controller header
 *
 * Adapted from tests/arm32/pb-a8/basic/arm_gic.h
 *
 */
#ifndef _ARM_GIC_H__
#define _ARM_GIC_H__

#include <arm_types.h>
#include <arm_plat.h>

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

int arm_gic_active_irq(u32 gic_nr);
int arm_gic_ack_irq(u32 gic_nr, u32 irq);
int arm_gic_mask(u32 gic_nr, u32 irq);
int arm_gic_unmask(u32 gic_nr, u32 irq);
int arm_gic_dist_init(u32 gic_nr, virtual_addr_t base, u32 irq_start);
int arm_gic_cpu_init(u32 gic_nr, virtual_addr_t base);

#endif
