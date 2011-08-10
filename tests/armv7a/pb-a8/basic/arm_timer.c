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
 * @file arm_timer.c
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief ARM Timer source
 */

#include <arm_io.h>
#include <arm_irq.h>
#include <arm_config.h>
#include <arm_plat.h>
#include <arm_timer.h>

static u32 timer_irq_count;

void arm_timer_enable(void)
{
	u32 ctrl;

	ctrl = arm_readl((void *)(REALVIEW_PBA8_TIMER0_1_BASE + TIMER_CTRL));
	ctrl |= TIMER_CTRL_ENABLE;
	arm_writel(ctrl, (void *)(REALVIEW_PBA8_TIMER0_1_BASE + TIMER_CTRL));
}

void arm_timer_disable(void)
{
	u32 ctrl;

	ctrl = arm_readl((void *)(REALVIEW_PBA8_TIMER0_1_BASE + TIMER_CTRL));
	ctrl &= ~TIMER_CTRL_ENABLE;
	arm_writel(ctrl, (void *)(REALVIEW_PBA8_TIMER0_1_BASE + TIMER_CTRL));
}

void arm_timer_clearirq(void)
{
	arm_writel(1, (void *)(REALVIEW_PBA8_TIMER0_1_BASE + TIMER_INTCLR));
}

u32 arm_timer_irqcount(void)
{
	return timer_irq_count;
}

int arm_timer_irqhndl(u32 irq_no, pt_regs_t * regs)
{
	timer_irq_count++;
	arm_timer_clearirq();
	return 0;
}

int arm_timer_init(u32 usecs, u32 ensel)
{
	u32 val;

	timer_irq_count = 0;

	/* 
	 * set clock frequency: 
	 *      REALVIEW_REFCLK is 32KHz
	 *      REALVIEW_TIMCLK is 1MHz
	 */
	val = arm_readl((void *)REALVIEW_SCTL_BASE) | (REALVIEW_TIMCLK << ensel);
	arm_writel(val, (void *)REALVIEW_SCTL_BASE);

	/* Register interrupt handler */
	arm_irq_register(IRQ_PBA8_TIMER0_1, &arm_timer_irqhndl);

	val = arm_readl((void *)(REALVIEW_PBA8_TIMER0_1_BASE + TIMER_CTRL));
	val &= ~TIMER_CTRL_ENABLE;
	val |= (TIMER_CTRL_32BIT | TIMER_CTRL_PERIODIC | TIMER_CTRL_IE);
	arm_writel(val, (void *)(REALVIEW_PBA8_TIMER0_1_BASE + TIMER_CTRL));
	arm_writel(usecs, (void *)(REALVIEW_PBA8_TIMER0_1_BASE + TIMER_LOAD));
	arm_writel(usecs, (void *)(REALVIEW_PBA8_TIMER0_1_BASE + TIMER_VALUE));

	return 0;
}
