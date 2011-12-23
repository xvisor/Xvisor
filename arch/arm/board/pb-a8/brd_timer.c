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
 * @file brd_timer.c
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief board specific progammable timer
 */

#include <vmm_cpu.h>
#include <vmm_board.h>
#include <vmm_error.h>
#include <vmm_timer.h>
#include <vmm_host_aspace.h>
#include <vmm_math.h>
#include <pba8_board.h>
#include <realview/timer.h>

static virtual_addr_t pba8_timer0_base;
static virtual_addr_t pba8_timer1_base;

u64 vmm_cpu_clocksource_cycles(void)
{
	return ~realview_timer_counter_value(pba8_timer1_base);
}

u64 vmm_cpu_clocksource_mask(void)
{
	return 0xFFFFFFFF;
}

u32 vmm_cpu_clocksource_mult(void)
{
	return vmm_timer_clocksource_khz2mult(1000, 20);
}

u32 vmm_cpu_clocksource_shift(void)
{
	return 20;
}

int __init vmm_cpu_clocksource_init(void)
{
	int rc;
	virtual_addr_t sctl_base;

	/* Map control registers */
	sctl_base = vmm_host_iomap(REALVIEW_SCTL_BASE, 0x1000);

	/* Map timer registers */
	pba8_timer1_base = vmm_host_iomap(REALVIEW_PBA8_TIMER0_1_BASE, 0x1000);
	pba8_timer1_base += 0x20;

	/* Initialize timers */
	rc = realview_timer_init(sctl_base, 
				 pba8_timer1_base,
				 REALVIEW_TIMER2_EnSel,
				 IRQ_PBA8_TIMER0_1,
				 NULL);
	if (rc) {
		return rc;
	}

	/* Unmap control register */
	rc = vmm_host_iounmap(sctl_base, 0x1000);
	if (rc) {
		return rc;
	}

	/* Configure timer1 as free running source */
	rc = realview_timer_counter_start(pba8_timer1_base);
	if (rc) {
		return rc;
	}
	realview_timer_enable(pba8_timer1_base);

	return VMM_OK;
}

int vmm_cpu_clockevent_stop(void)
{
	return realview_timer_event_stop(pba8_timer0_base);
}

static int pba8_timer0_handler(u32 irq_no, vmm_user_regs_t * regs, void *dev)
{
	realview_timer_event_clearirq(pba8_timer0_base);

	vmm_timer_clockevent_process(regs);

	return VMM_OK;
}

int vmm_cpu_clockevent_expire(void)
{
	int i, rc;

	rc = realview_timer_event_start(pba8_timer0_base, 0);
	if (rc) {
		return rc;
	}

	while (!realview_timer_event_checkirq(pba8_timer0_base)) {
		/* FIXME: Relax a bit with some soft delay */
		for (i = 0; i < 100; i++);
	}

	return VMM_OK;
}

int vmm_cpu_clockevent_start(u64 tick_nsecs)
{
	return realview_timer_event_start(pba8_timer0_base, tick_nsecs);
}

int __init vmm_cpu_clockevent_init(void)
{
	int rc;
	virtual_addr_t sctl_base;

	/* Map control registers */
	sctl_base = vmm_host_iomap(REALVIEW_SCTL_BASE, 0x1000);

	/* Map timer registers */
	pba8_timer0_base = vmm_host_iomap(REALVIEW_PBA8_TIMER0_1_BASE, 0x1000);

	/* Initialize timers */
	rc = realview_timer_init(sctl_base,
				 pba8_timer0_base,
				 REALVIEW_TIMER1_EnSel,
				 IRQ_PBA8_TIMER0_1,
				 pba8_timer0_handler);
	if (rc) {
		return rc;
	}

	/* Unmap control register */
	rc = vmm_host_iounmap(sctl_base, 0x1000);
	if (rc) {
		return rc;
	}

	return VMM_OK;
}

