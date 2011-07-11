/**
 * Copyright (c) 2010 Anup Patel.
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
 * @file arm_regs.h
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief  register related macros & defines for ARM test code
 */
#ifndef __ARM_REGS_H__
#define __ARM_REGS_H__

#define CPSR_MODE_MASK			0x0000001f
#define CPSR_MODE_USER			0x00000010
#define CPSR_MODE_FIQ			0x00000011
#define CPSR_MODE_IRQ			0x00000012
#define CPSR_MODE_SUPERVISOR		0x00000013
#define CPSR_MODE_ABORT			0x00000017
#define CPSR_MODE_UNDEFINED		0x0000001b
#define CPSR_MODE_SYSTEM		0x0000001f

#define CPSR_THUMB_ENABLED		(1 << 5)
#define CPSR_FIQ_DISABLED		(1 << 6)
#define CPSR_IRQ_DISABLED		(1 << 7)
#define CPSR_ASYNC_ABORT_DISABLED	(1 << 8)
#define CPSR_BE_ENABLED			(1 << 9)

#define CPSR_COND_OVERFLOW		(1 << 28)
#define CPSR_COND_CARRY			(1 << 29)
#define CPSR_COND_ZERO			(1 << 30)
#define CPSR_COND_NEGATIVE		(1 << 31)

#endif
