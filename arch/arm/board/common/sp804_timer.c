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
 * @file timer.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief SP804 Dual-Mode Timer Implementation
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_clocksource.h>
#include <vmm_clockchip.h>
#include <sp804_timer.h>

struct sp804_clocksource {
	virtual_addr_t base;
	struct vmm_clocksource clksrc;
};

u64 sp804_clocksource_read(struct vmm_clocksource *cs)
{
	u32 count;
	struct sp804_clocksource *tcs = cs->priv;

	count = vmm_readl((void *)(tcs->base + TIMER_VALUE));

	return ~count;
}

int __init sp804_clocksource_init(virtual_addr_t base, 
				  const char *name, 
				  u32 freq_hz)
{
	u32 ctrl;
	struct sp804_clocksource *cs;

	cs = vmm_zalloc(sizeof(struct sp804_clocksource));
	if (!cs) {
		return VMM_EFAIL;
	}

	cs->base = base;
	cs->clksrc.name = name;
	cs->clksrc.rating = 300;
	cs->clksrc.read = &sp804_clocksource_read;
	cs->clksrc.mask = VMM_CLOCKSOURCE_MASK(32);
	vmm_clocks_calc_mult_shift(&cs->clksrc.mult, &cs->clksrc.shift, 
				   freq_hz, VMM_NSEC_PER_SEC, 10);
	cs->clksrc.priv = cs;

	vmm_writel(0x0, (void *)(base + TIMER_CTRL));
	vmm_writel(0xFFFFFFFF, (void *)(base + TIMER_LOAD));
	ctrl = (TIMER_CTRL_ENABLE | TIMER_CTRL_32BIT | TIMER_CTRL_PERIODIC);
	vmm_writel(ctrl, (void *)(base + TIMER_CTRL));

	return vmm_clocksource_register(&cs->clksrc);
}

struct sp804_clockchip {
	virtual_addr_t base;
	struct vmm_clockchip clkchip;
};

static vmm_irq_return_t sp804_clockchip_irq_handler(int irq_no, void *dev)
{
	struct sp804_clockchip *tcc = dev;

	/* clear the interrupt */
	vmm_writel(1, (void *)(tcc->base + TIMER_INTCLR));

	tcc->clkchip.event_handler(&tcc->clkchip);

	return VMM_IRQ_HANDLED;
}

static void sp804_clockchip_set_mode(enum vmm_clockchip_mode mode,
				     struct vmm_clockchip *cc)
{
	struct sp804_clockchip *tcc = cc->priv;
	unsigned long ctrl = TIMER_CTRL_32BIT | TIMER_CTRL_IE;

	vmm_writel(ctrl, (void *)(tcc->base + TIMER_CTRL));

	switch (mode) {
	case VMM_CLOCKCHIP_MODE_PERIODIC:
		/* FIXME: */
		vmm_writel(10000, (void *)(tcc->base + TIMER_LOAD));
		ctrl |= TIMER_CTRL_PERIODIC | TIMER_CTRL_ENABLE;
		break;
	case VMM_CLOCKCHIP_MODE_ONESHOT:
		ctrl |= TIMER_CTRL_ONESHOT;
		break;
	case VMM_CLOCKCHIP_MODE_UNUSED:
	case VMM_CLOCKCHIP_MODE_SHUTDOWN:
	default:
		break;
	}

	vmm_writel(ctrl, (void *)(tcc->base + TIMER_CTRL));
}

static int sp804_clockchip_set_next_event(unsigned long next,
					  struct vmm_clockchip *cc)
{
	struct sp804_clockchip *tcc = cc->priv;
	unsigned long ctrl = vmm_readl((void *)(tcc->base + TIMER_CTRL));

	vmm_writel(next, (void *)(tcc->base + TIMER_LOAD));
	vmm_writel(ctrl | TIMER_CTRL_ENABLE, (void *)(tcc->base + TIMER_CTRL));

	return 0;
}

int __cpuinit sp804_clockchip_init(virtual_addr_t base, 
				   const char *name, 
				   u32 hirq,
				   u32 freq_hz,
				   u32 target_cpu)
{
	int rc;
	struct sp804_clockchip *cc;

	cc = vmm_zalloc(sizeof(struct sp804_clockchip));
	if (!cc) {
		return VMM_EFAIL;
	}

	cc->base = base;
	cc->clkchip.name = name;
	cc->clkchip.hirq = hirq;
	cc->clkchip.rating = 300;
#ifdef CONFIG_SMP
	cc->clkchip.cpumask = vmm_cpumask_of(target_cpu);
#else
	cc->clkchip.cpumask = cpu_all_mask;
#endif
	cc->clkchip.features = 
		VMM_CLOCKCHIP_FEAT_PERIODIC | VMM_CLOCKCHIP_FEAT_ONESHOT;
	vmm_clocks_calc_mult_shift(&cc->clkchip.mult, &cc->clkchip.shift, 
				   VMM_NSEC_PER_SEC, freq_hz, 10);
	cc->clkchip.min_delta_ns = vmm_clockchip_delta2ns(0xF, &cc->clkchip);
	cc->clkchip.max_delta_ns = 
			vmm_clockchip_delta2ns(0xFFFFFFFF, &cc->clkchip);
	cc->clkchip.set_mode = &sp804_clockchip_set_mode;
	cc->clkchip.set_next_event = &sp804_clockchip_set_next_event;
	cc->clkchip.priv = cc;

	/* Register interrupt handler */
	if ((rc = vmm_host_irq_register(hirq, name,
					&sp804_clockchip_irq_handler, cc))) {
		return rc;
	}

#ifdef CONFIG_SMP
	/* Set host irq affinity to target cpu */
	if ((rc = vmm_host_irq_set_affinity(hirq,
					    vmm_cpumask_of(target_cpu), 
					    TRUE))) {
		return rc;
	}
#endif

	return vmm_clockchip_register(&cc->clkchip);
}

