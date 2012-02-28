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
 * @file brd_pic.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief board specific progammable interrupt contoller
 */

#include <arch_board.h>
#include <vmm_error.h>

u32 arch_pic_irq_count(void)
{
	return 8;
}

int arch_pic_cpu_to_host_map(u32 cpu_irq_no)
{
	return -1;
}

int arch_pic_pre_condition(u32 host_irq_no)
{
	return VMM_OK;
}

int arch_pic_post_condition(u32 host_irq_no)
{
	return VMM_OK;
}

int arch_pic_irq_enable(u32 host_irq_no)
{
	return VMM_OK;
}

int arch_pic_irq_disable(u32 host_irq_no)
{
	return VMM_OK;
}

int arch_pic_init(void)
{
	return VMM_OK;
}

