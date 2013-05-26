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
 * @file vmm_host_irq.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source code for host interrupts
 */

#include <vmm_error.h>
#include <vmm_spinlocks.h>
#include <vmm_smp.h>
#include <vmm_heap.h>
#include <vmm_host_irq.h>
#include <arch_cpu_irq.h>
#include <arch_host_irq.h>
#include <libs/stringlib.h>

struct vmm_host_irqs_ctrl {
	vmm_spinlock_t lock;
	struct vmm_host_irq *irq;
};

static struct vmm_host_irqs_ctrl hirqctrl;

void vmm_handle_fast_eoi(u32 hirq_no, struct vmm_host_irq *irq)
{
	struct dlist *l;
	struct vmm_host_irq_action *act;
	list_for_each(l, &irq->action_list) {
		act = list_entry(l, struct vmm_host_irq_action, head);
		if (act->func(hirq_no, act->dev) == VMM_IRQ_HANDLED) {
			break;
		}
	}
	if (irq->chip && irq->chip->irq_eoi) {
		irq->chip->irq_eoi(irq);
	}
}

void vmm_handle_level_irq(u32 hirq_no, struct vmm_host_irq *irq)
{
	struct dlist *l;
	struct vmm_host_irq_action *act;
	if (irq->chip && irq->chip->irq_mask_and_ack) {
			irq->chip->irq_mask_and_ack(irq);
	} else {
		if (irq->chip && irq->chip->irq_mask) {
			irq->chip->irq_mask(irq);
		}
		if (irq->chip && irq->chip->irq_ack) {
			irq->chip->irq_ack(irq);
		}
	}
	list_for_each(l, &irq->action_list) {
		act = list_entry(l, struct vmm_host_irq_action, head);
		if (act->func(hirq_no, act->dev) == VMM_IRQ_HANDLED) {
			break;
		}
	}
	if (irq->chip && irq->chip->irq_unmask) {
		irq->chip->irq_unmask(irq);
	}
}

int vmm_host_generic_irq_exec(u32 hirq_no)
{
	struct vmm_host_irq *irq;
	if (hirq_no < ARCH_HOST_IRQ_COUNT) {
		irq = &hirqctrl.irq[hirq_no];
		irq->count[vmm_smp_processor_id()]++;
		if (!(irq->state & VMM_IRQ_STATE_PER_CPU)) {
			irq->state |= VMM_IRQ_STATE_INPROGRESS;
		}
		if (irq->handler) {
			irq->handler(hirq_no, irq);
		}
		if (!(irq->state & VMM_IRQ_STATE_PER_CPU)) {
			irq->state &= ~VMM_IRQ_STATE_INPROGRESS;
		}
		return VMM_OK;
	}
	return VMM_ENOTAVAIL;
}

int vmm_host_irq_exec(u32 cpu_irq_no)
{
	return vmm_host_generic_irq_exec(arch_host_irq_active(cpu_irq_no));
}

u32 vmm_host_irq_count(void)
{
	return ARCH_HOST_IRQ_COUNT;
}

struct vmm_host_irq *vmm_host_irq_get(u32 hirq_num)
{
	if (hirq_num < ARCH_HOST_IRQ_COUNT) {
		return &hirqctrl.irq[hirq_num];
	}
	return NULL;
}

int vmm_host_irq_set_chip(u32 hirq_num, struct vmm_host_irq_chip *chip)
{
	if (hirq_num < ARCH_HOST_IRQ_COUNT) {
		hirqctrl.irq[hirq_num].chip = chip;
		return VMM_OK;
	}
	return VMM_EFAIL;
}

int vmm_host_irq_set_chip_data(u32 hirq_num, void *chip_data)
{
	if (hirq_num < ARCH_HOST_IRQ_COUNT) {
		hirqctrl.irq[hirq_num].chip_data = chip_data;
		return VMM_OK;
	}
	return VMM_EFAIL;
}

int vmm_host_irq_set_handler(u32 hirq_num, 
			     void (*handler)(u32, struct vmm_host_irq *))
{
	if (hirq_num < ARCH_HOST_IRQ_COUNT) {
		hirqctrl.irq[hirq_num].handler = handler;
		return VMM_OK;
	}
	return VMM_EFAIL;
}

int vmm_host_irq_set_affinity(u32 hirq_num, 
			      const struct vmm_cpumask *dest, 
			      bool force)
{
	struct vmm_host_irq *irq;
	if (hirq_num < ARCH_HOST_IRQ_COUNT) {
		irq = &hirqctrl.irq[hirq_num];
		if (irq->chip && irq->chip->irq_set_affinity) {
			irq->state |= VMM_IRQ_STATE_AFFINITY_SET;
			return irq->chip->irq_set_affinity(irq, dest, force);
		}
	}
	return VMM_EFAIL;
}

int vmm_host_irq_set_type(u32 hirq_num, u32 type)
{
	int rc = VMM_EFAIL;
	struct vmm_host_irq *irq;
	if (hirq_num < ARCH_HOST_IRQ_COUNT) {
		irq = &hirqctrl.irq[hirq_num];
		type &= VMM_IRQ_TYPE_SENSE_MASK;
		if (type == VMM_IRQ_TYPE_NONE) {
			return VMM_OK;
		}
		if (irq->chip && irq->chip->irq_set_type) {
			rc = irq->chip->irq_set_type(irq, type);
		} else {
			return VMM_OK;
		}
		if (rc == VMM_OK) {
			irq->state &= ~VMM_IRQ_STATE_TRIGGER_MASK;
			irq->state |= type;
			if (type & VMM_IRQ_TYPE_LEVEL_MASK) {
				irq->state |= VMM_IRQ_STATE_LEVEL;
			} else {
				irq->state &= ~VMM_IRQ_STATE_LEVEL;
			}
		}
	}
	return rc;
}

int vmm_host_irq_mark_per_cpu(u32 hirq_num)
{
	struct vmm_host_irq *irq;
	if (hirq_num < ARCH_HOST_IRQ_COUNT) {
		irq = &hirqctrl.irq[hirq_num];
		irq->state |= VMM_IRQ_STATE_PER_CPU;
		return VMM_OK;
	}
	return VMM_EFAIL;
}

int vmm_host_irq_unmark_per_cpu(u32 hirq_num)
{
	struct vmm_host_irq *irq;
	if (hirq_num < ARCH_HOST_IRQ_COUNT) {
		irq = &hirqctrl.irq[hirq_num];
		irq->state &= ~VMM_IRQ_STATE_PER_CPU;
		return VMM_OK;
	}
	return VMM_EFAIL;
}

int vmm_host_irq_enable(u32 hirq_num)
{
	struct vmm_host_irq *irq;
	if (hirq_num < ARCH_HOST_IRQ_COUNT) {
		irq = &hirqctrl.irq[hirq_num];
		irq->state &= ~VMM_IRQ_STATE_DISABLED;
		if (irq->chip) {
			if (irq->chip->irq_enable) {
				irq->chip->irq_enable(irq);
			} else {
				irq->chip->irq_unmask(irq);
			}
		}
		irq->state &= ~VMM_IRQ_STATE_MASKED;
		return VMM_OK;
	}
	return VMM_EFAIL;
}

int vmm_host_irq_disable(u32 hirq_num)
{
	struct vmm_host_irq *irq;
	if (hirq_num < ARCH_HOST_IRQ_COUNT) {
		irq = &hirqctrl.irq[hirq_num];
		irq->state |= VMM_IRQ_STATE_DISABLED;
		if (irq->chip) {
			if (irq->chip->irq_disable) {
				irq->chip->irq_disable(irq);
			} else {
				irq->chip->irq_mask(irq);
			}
		}
		irq->state |= VMM_IRQ_STATE_MASKED;
		return VMM_OK;
	}
	return VMM_EFAIL;
}

int vmm_host_irq_unmask(u32 hirq_num)
{
	struct vmm_host_irq *irq;
	if (hirq_num < ARCH_HOST_IRQ_COUNT) {
		irq = &hirqctrl.irq[hirq_num];
		if (irq->chip && irq->chip->irq_unmask) {
			irq->chip->irq_mask(irq);
			irq->state &= ~VMM_IRQ_STATE_MASKED;
		}
		return VMM_OK;
	}
	return VMM_EFAIL;
}

int vmm_host_irq_mask(u32 hirq_num)
{
	struct vmm_host_irq *irq;
	if (hirq_num < ARCH_HOST_IRQ_COUNT) {
		irq = &hirqctrl.irq[hirq_num];
		if (irq->chip && irq->chip->irq_mask) {
			irq->chip->irq_mask(irq);
			irq->state |= VMM_IRQ_STATE_MASKED;
		}
		return VMM_OK;
	}
	return VMM_EFAIL;
}

int vmm_host_irq_register(u32 hirq_num, 
			  const char *name,
			  vmm_host_irq_function_t func,
			  void *dev)
{
	bool found;
	irq_flags_t flags;
	struct dlist *l;
	struct vmm_host_irq *irq;
	struct vmm_host_irq_action *act;
	if (hirq_num < ARCH_HOST_IRQ_COUNT) {
		vmm_spin_lock_irqsave(&hirqctrl.lock, flags);
		irq = &hirqctrl.irq[hirq_num];
		found = FALSE;
		list_for_each(l, &irq->action_list) {
			act = list_entry(l, struct vmm_host_irq_action, head);
			if (act->dev == dev) {
				found = TRUE;
				break;
			}
		}
		if (found) {
			vmm_spin_unlock_irqrestore(&hirqctrl.lock, flags);
			return VMM_EFAIL;
		}
		irq->name = name;
		act = vmm_malloc(sizeof(struct vmm_host_irq_action));
		if (!act) {
			vmm_spin_unlock_irqrestore(&hirqctrl.lock, flags);
			return VMM_EFAIL;
		}
		INIT_LIST_HEAD(&act->head);
		act->func = func;
		act->dev = dev;
		list_add_tail(&act->head, &irq->action_list);
		vmm_spin_unlock_irqrestore(&hirqctrl.lock, flags);
		return vmm_host_irq_enable(hirq_num);
	}
	return VMM_EFAIL;
}

int vmm_host_irq_unregister(u32 hirq_num, void *dev)
{
	bool found;
	irq_flags_t flags;
	struct dlist *l;
	struct vmm_host_irq *irq;
	struct vmm_host_irq_action *act;
	if (hirq_num < ARCH_HOST_IRQ_COUNT) {
		vmm_spin_lock_irqsave(&hirqctrl.lock, flags);
		irq = &hirqctrl.irq[hirq_num];
		found = FALSE;
		list_for_each(l, &irq->action_list) {
			act = list_entry(l, struct vmm_host_irq_action, head);
			if (act->dev == dev) {
				found = TRUE;
				break;
			}
		}
		if (!found) {
			vmm_spin_unlock_irqrestore(&hirqctrl.lock, flags);
			return VMM_EFAIL;
		}
		list_del(&act->head);
		vmm_free(act);
		vmm_spin_unlock_irqrestore(&hirqctrl.lock, flags);
		if (list_empty(&irq->action_list)) {
			return vmm_host_irq_disable(hirq_num);
		}
		return VMM_OK;
	}
	return VMM_EFAIL;
}

int __cpuinit vmm_host_irq_init(void)
{
	int ret;
	u32 ite, cpu = vmm_smp_processor_id();

	if (!cpu) {
		/* Clear the memory of control structure */
		memset(&hirqctrl, 0, sizeof(hirqctrl));

		/* Initialize spin lock */
		INIT_SPIN_LOCK(&hirqctrl.lock);

		/* Allocate memory for irq array */
		hirqctrl.irq = vmm_malloc(sizeof(struct vmm_host_irq) * 
				  ARCH_HOST_IRQ_COUNT);

		/* Reset the handler array */
		for (ite = 0; ite < ARCH_HOST_IRQ_COUNT; ite++) {
			hirqctrl.irq[ite].num = ite;
			hirqctrl.irq[ite].name = NULL;
			hirqctrl.irq[ite].state = (VMM_IRQ_TYPE_NONE | 
						   VMM_IRQ_STATE_DISABLED | 
						   VMM_IRQ_STATE_MASKED);
			for (cpu = 0; cpu < CONFIG_CPU_COUNT; cpu++) {
				hirqctrl.irq[ite].count[cpu] = 0;
			}
			hirqctrl.irq[ite].chip = NULL;
			hirqctrl.irq[ite].chip_data = NULL;
			hirqctrl.irq[ite].handler = NULL;
			INIT_LIST_HEAD(&hirqctrl.irq[ite].action_list);
		}
	}

	/* Initialize board specific PIC */
	if ((ret = arch_host_irq_init())) {
		return ret;
	}

	/* Setup interrupts in CPU */
	if ((ret = arch_cpu_irq_setup())) {
		return ret;
	}

	/* Enable interrupts in CPU */
	arch_cpu_irq_enable();

	return VMM_OK;
}
