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
 * @file intc.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Allwinner Sunxi interrupt controller interface
 */
#ifndef __SUNXI_INTC_H__
#define __SUNXI_INTC_H__

/** Get active irq number from Sunxi interrupt controller */
u32 aw_intc_irq_active(u32 cpu_irq_no);

/** Initialize Sunxi interrupt controller */
int aw_intc_devtree_init(void);

#endif /* __SUNXI_INTC_H__ */
