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
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief Realview GIC (Generic Interrupt Controller) emulator.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_string.h>
#include <vmm_modules.h>
#include <vmm_scheduler.h>
#include <vmm_vcpu_irq.h>
#include <vmm_devemu.h>

#define MODULE_VARID			gic_emulator_module
#define MODULE_NAME			"Realview GIC Emulator"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			gic_emulator_init
#define	MODULE_EXIT			gic_emulator_exit

#define GIC_MAX_NCPU			4
#define GIC_MAX_NIRQ			96

struct gic_irq_state {
	u32 enabled:1;
	u32 pending:GIC_MAX_NCPU;
	u32 active:GIC_MAX_NCPU;
	u32 level:GIC_MAX_NCPU;
	u32 model:1; /* 0 = N:N, 1 = 1:N */
	u32 trigger:1; /* nonzero = edge triggered.  */
};

struct gic_state {
	vmm_guest_t *guest;
	vmm_emupic_t *pic;
	vmm_spinlock_t lock;

	/* Coniguration */
	u8 id[8];
	u32 num_cpu;
	u32 num_irq;
	u32 num_base_irq;
	bool is_child_pic;
	u32 parent_irq[GIC_MAX_NCPU];

	/* Chip Info */
	int enabled;

	/* CPU Interface */
	int cpu_enabled[GIC_MAX_NCPU];
	int priority_mask[GIC_MAX_NCPU];
	int running_irq[GIC_MAX_NCPU];
	int running_priority[GIC_MAX_NCPU];
	int current_pending[GIC_MAX_NCPU];

	/* Distribution Control */
	struct gic_irq_state irq_state[GIC_MAX_NIRQ];
	int irq_target[GIC_MAX_NIRQ];
	int priority1[32][GIC_MAX_NCPU];
	int priority2[GIC_MAX_NIRQ - 32];
	int last_active[GIC_MAX_NIRQ][GIC_MAX_NCPU];
};

#define GIC_ALL_CPU_MASK(s) ((1 << (s)->num_cpu) - 1)
#define GIC_NUM_CPU(s) ((s)->num_cpu)
#define GIC_NUM_IRQ(s) ((s)->num_irq)
#define GIC_SET_ENABLED(s, irq) (s)->irq_state[irq].enabled = 1
#define GIC_CLEAR_ENABLED(s, irq) (s)->irq_state[irq].enabled = 0
#define GIC_TEST_ENABLED(s, irq) (s)->irq_state[irq].enabled
#define GIC_SET_PENDING(s, irq, cm) (s)->irq_state[irq].pending |= (cm)
#define GIC_CLEAR_PENDING(s, irq, cm) (s)->irq_state[irq].pending &= ~(cm)
#define GIC_TEST_PENDING(s, irq, cm) (((s)->irq_state[irq].pending & (cm)) != 0)
#define GIC_SET_ACTIVE(s, irq, cm) (s)->irq_state[irq].active |= (cm)
#define GIC_CLEAR_ACTIVE(s, irq, cm) (s)->irq_state[irq].active &= ~(cm)
#define GIC_TEST_ACTIVE(s, irq, cm) (((s)->irq_state[irq].active & (cm)) != 0)
#define GIC_SET_MODEL(s, irq) (s)->irq_state[irq].model = 1
#define GIC_CLEAR_MODEL(s, irq) (s)->irq_state[irq].model = 0
#define GIC_TEST_MODEL(s, irq) (s)->irq_state[irq].model
#define GIC_SET_LEVEL(s, irq, cm) (s)->irq_state[irq].level = (cm)
#define GIC_CLEAR_LEVEL(s, irq, cm) (s)->irq_state[irq].level &= ~(cm)
#define GIC_TEST_LEVEL(s, irq, cm) (((s)->irq_state[irq].level & (cm)) != 0)
#define GIC_SET_TRIGGER(s, irq) (s)->irq_state[irq].trigger = 1
#define GIC_CLEAR_TRIGGER(s, irq) (s)->irq_state[irq].trigger = 0
#define GIC_TEST_TRIGGER(s, irq) (s)->irq_state[irq].trigger
#define GIC_GET_PRIORITY(s, irq, cpu) \
  (((irq) < 32) ? (s)->priority1[irq][cpu] : (s)->priority2[(irq) - 32])
#define GIC_TARGET(s, irq) (s)->irq_target[irq]

/* Update interrupt status after enabled or pending bits have been changed. */
static void gic_update(struct gic_state *s)
{
	int best_irq, best_prio;
	int irq, level, cpu, cm;
	vmm_vcpu_t *vcpu;
	for (cpu = 0; cpu < GIC_NUM_CPU(s); cpu++) {
		cm = 1 << cpu;
		s->current_pending[cpu] = 1023;
		if (!s->enabled || !s->cpu_enabled[cpu]) {
			if (s->is_child_pic) {
				vmm_devemu_emulate_irq(s->guest, 
						       s->parent_irq[cpu], 
						       0);
			}
			return;
		}
		best_prio = 0x100;
		best_irq = 1023;
		for (irq = 0; irq < GIC_NUM_IRQ(s); irq++) {
			if (GIC_TEST_ENABLED(s, irq) && 
			    GIC_TEST_PENDING(s, irq, cm)) {
				if (GIC_GET_PRIORITY(s, irq, cpu) < best_prio) {
					best_prio = 
						GIC_GET_PRIORITY(s, irq, cpu);
					best_irq = irq;
				}
			}
		}
		level = 0;
		if (best_prio <= s->priority_mask[cpu]) {
			s->current_pending[cpu] = best_irq;
			if (best_prio < s->running_priority[cpu]) {
				level = 1;
			}
		}
		if (s->is_child_pic) {
			/* Assert irq to Parent PIC */
			vmm_devemu_emulate_irq(s->guest, 
					       s->parent_irq[cpu], level);
		} else if (level) {
			/* Assert irq to VCPU */
			vcpu = vmm_scheduler_guest_vcpu(s->guest, cpu);
			if (vcpu) {
				vmm_vcpu_irq_assert(vcpu, 
						    s->parent_irq[cpu], 0x0);
			}
		}
	}
}

static void gic_set_running_irq(struct gic_state *s, int cpu, int irq)
{
	s->running_irq[cpu] = irq;
	if (irq == 1023) {
		s->running_priority[cpu] = 0x100;
	} else {
		s->running_priority[cpu] = GIC_GET_PRIORITY(s, irq, cpu);
	}
	gic_update(s);
}

static u32 gic_acknowledge_irq(struct gic_state *s, int cpu)
{
	int new_irq;
	int cm = 1 << cpu;

	new_irq = s->current_pending[cpu];
	if ((new_irq == 1023) ||
	    GIC_GET_PRIORITY(s, new_irq, cpu) >= s->running_priority[cpu]) {
		return 1023;
	}

	s->last_active[new_irq][cpu] = s->running_irq[cpu];
	/* Clear pending flags for both level and edge triggered interrupts.
	 * Level triggered IRQs will be reasserted once they become inactive. */
	GIC_CLEAR_PENDING(s, new_irq, 
			  GIC_TEST_MODEL(s, new_irq) ? GIC_ALL_CPU_MASK(s) : cm);
	gic_set_running_irq(s, cpu, new_irq);

	return new_irq;
}

static void gic_complete_irq(struct gic_state * s, int cpu, int irq)
{
	int update = 0;
	int cm = 1 << cpu;

	if (s->running_irq[cpu] == 1023)
		return; /* No active IRQ.  */

	if (irq != 1023) {
		/* Mark level triggered interrupts as pending if 
		 * they are still raised. */
		if (!GIC_TEST_TRIGGER(s, irq) && 
		    GIC_TEST_ENABLED(s, irq) &&
		    GIC_TEST_LEVEL(s, irq, cm) && 
		    (GIC_TARGET(s, irq) & cm) != 0) {
			GIC_SET_PENDING(s, irq, cm);
			update = 1;
		}
	}
	if (irq != s->running_irq[cpu]) {
		/* Complete an IRQ that is not currently running.  */
		int tmp = s->running_irq[cpu];
		while (s->last_active[tmp][cpu] != 1023) {
			if (s->last_active[tmp][cpu] == irq) {
				s->last_active[tmp][cpu] = 
						s->last_active[irq][cpu];
				break;
			}
			tmp = s->last_active[tmp][cpu];
		}
		if (update) {
			gic_update(s);
		}
	} else {
		/* Complete the current running IRQ.  */
		gic_set_running_irq(s, cpu, 
				s->last_active[s->running_irq[cpu]][cpu]);
	}
}

static int gic_dist_readb(struct gic_state * s, int cpu, u32 offset, u8 *dst)
{
	u32 done = 0, i, irq, mask;

	if (!s || !dst) {
		return VMM_EFAIL;
	}

	vmm_spin_lock(&s->lock);

	done = 1;
	switch (offset - (offset & 0x3)) {
	case 0x000: /* Distributor control */
		if (offset == 0x000) {
			*dst = s->enabled;
		} else {
			*dst = 0x0;
		}
		break;
	case 0x004: /* Controller type */
		if (offset == 0x004) {
			*dst = (GIC_NUM_CPU(s) - 1) << 5;
			*dst |= (GIC_NUM_IRQ(s) / 32) - 1;
		} else {
			*dst = 0x0;
		}
		break;
	case 0x100: /* Set-enable0 */
	case 0x104: /* Set-enable1 */
	case 0x108: /* Set-enable2 */
	case 0x180: /* Clear-enable0 */
	case 0x184: /* Clear-enable1 */
	case 0x188: /* Clear-enable2 */
		if (offset < 0x180) {
			irq = (offset - 0x100) * 8;
		} else {
			irq = (offset - 0x180) * 8;
		}
		*dst = 0;
		for (i = 0; i < 8; i++) {
			*dst |= GIC_TEST_ENABLED(s, irq + i) ? 
				(1 << i) : 0x0;
		}
		break;
	case 0x200: /* Set-pending0 */
	case 0x204: /* Set-pending1 */
	case 0x208: /* Set-pending2 */
	case 0x280: /* Clear-pending0 */
	case 0x284: /* Clear-pending1 */
	case 0x288: /* Clear-pending2 */
		if (offset < 0x280) {
			irq = (offset - 0x200) * 8;
		} else {
			irq = (offset - 0x280) * 8;
		}
		mask = (irq < 32) ? (1 << cpu) : GIC_ALL_CPU_MASK(s);
		*dst = 0;
		for (i = 0; i < 8; i++) {
			*dst |= GIC_TEST_PENDING(s, irq + i, mask) ? 
				(1 << i) : 0x0;
		}
		break;
	case 0x300: /* Active0 */
	case 0x304: /* Active1 */
	case 0x308: /* Active2 */
		irq = (offset - 0x300) * 8;
		mask = (irq < 32) ? (1 << cpu) : GIC_ALL_CPU_MASK(s);
		*dst = 0;
		for (i = 0; i < 8; i++) {
			*dst |= GIC_TEST_ACTIVE(s, irq + i, mask) ? 
				(1 << i) : 0x0;
		}
		break;
	default:
		done = 0;
		break;
	};

	if (!done) {
		done = 1;
		switch (offset >> 8) {
		case 0x4: /* Priority */
			irq = offset - 0x400;
			if (GIC_NUM_IRQ(s) <= irq) {
				done = 0;
				break;
			}
			*dst = GIC_GET_PRIORITY(s, irq, cpu) << 4;
			break;
		case 0x8: /* CPU targets */
			irq = offset - 0x800;
			if (GIC_NUM_IRQ(s) <= irq) {
				done = 0;
				break;
			}
			if (29 <= irq && irq < 32) {
				*dst = 1 << cpu;
			} else {
				*dst = GIC_TARGET(s, irq);
			}
			break;
		case 0xC: /* Configuration */
			irq = (offset - 0xC00) * 4;
			if (GIC_NUM_IRQ(s) <= irq) {
				done = 0;
				break;
			}
			*dst = 0;
			for (i = 0; i < 4; i++) {
				if (GIC_TEST_MODEL(s, irq + i)) {
					*dst |= (1 << (i * 2));
				}
				if (GIC_TEST_TRIGGER(s, irq + i)) {
					*dst |= (2 << (i * 2));
				}
			}
			break;
		case 0xF:
			if (0xFE0 <= offset) {
				if (offset & 0x3) {
					*dst = 0;
				} else {
					*dst = s->id[(offset - 0xFE0) >> 2];
				}
			} else {
				done = 0;
			}
			break;
		default:
			done = 0;
			break;
		};
	}

	vmm_spin_unlock(&s->lock);

	if (!done) {
		return VMM_EFAIL;
	}

	return VMM_OK;
}

static int gic_dist_writeb(struct gic_state * s, int cpu, u32 offset, u8 src)
{
	u32 done = 0, i, irq, mask;

	if (!s) {
		return VMM_EFAIL;
	}

	vmm_spin_lock(&s->lock);

	done = 1;
	switch (offset - (offset & 0x3)) {
	case 0x000: /* Distributor control */
		if (offset == 0x000) {
			s->enabled = src & 0x1;
		}
		break;
	case 0x004: /* Controller type */
		/* Ignored. */
		break;
	case 0x100: /* Set-enable0 */
	case 0x104: /* Set-enable1 */
	case 0x108: /* Set-enable2 */
		irq = (offset - 0x100) * 8;
		if (GIC_NUM_IRQ(s) <= irq) {
			done = 0;
			break;
		}
		if (irq < 16) {
			src = 0xFF;
		}
		for (i = 0; i < 8; i++) {
			if (src & (1 << i)) {
				mask = ((irq + i) < 32) ? 
					(1 << cpu) : GIC_TARGET(s, (irq + i));
				GIC_SET_ENABLED(s, irq + i);
				/* If a raised level triggered IRQ enabled 
				 * then mark is as pending.  */
				if (GIC_TEST_LEVEL(s, (irq + i), mask) &&
				    !GIC_TEST_TRIGGER(s, (irq + i))) {
					GIC_SET_PENDING(s, (irq + i), mask);
				}
			}
		}
		break;
	case 0x180: /* Clear-enable0 */
	case 0x184: /* Clear-enable1 */
	case 0x188: /* Clear-enable2 */
		irq = (offset - 0x180) * 8;
		if (GIC_NUM_IRQ(s) <= irq) {
			done = 0;
			break;
		}
		if (irq < 16) {
			src = 0x00;
		}
		for (i = 0; i < 8; i++) {
			if (src & (1 << i)) {
				GIC_CLEAR_ENABLED(s, irq + i);
			}
		}
		break;
	case 0x200: /* Set-pending0 */
	case 0x204: /* Set-pending1 */
	case 0x208: /* Set-pending2 */
		irq = (offset - 0x200) * 8;
		if (GIC_NUM_IRQ(s) <= irq) {
			done = 0;
			break;
		}
		if (irq < 16) {
			src = 0x00;
		}
		for (i = 0; i < 8; i++) {
			if (src & (1 << i)) {
				mask = GIC_TARGET(s, irq + i);
				GIC_SET_PENDING(s, irq + i, mask);
			}
		}
		break;
	case 0x280: /* Clear-pending0 */
	case 0x284: /* Clear-pending1 */
	case 0x288: /* Clear-pending2 */
		irq = (offset - 0x280) * 8;
		if (GIC_NUM_IRQ(s) <= irq) {
			done = 0;
			break;
		}
		/* ??? This currently clears the pending bit for all CPUs, even
 		 * for per-CPU interrupts.  It's unclear whether this is the
		 * corect behavior.  */
		mask = GIC_ALL_CPU_MASK(s);
		for (i = 0; i < 8; i++) {
			if (src & (1 << i)) {
				GIC_CLEAR_PENDING(s, irq + i, mask);
			}
		}
		break;
	default:
		done = 0;
		break;
	};

	if (!done) {
		done = 1;
		switch (offset >> 8) {
		case 0x4: /* Priority */
			irq = offset - 0x400;
			if (GIC_NUM_IRQ(s) <= irq) {
				done = 0;
				break;
			}
			if (irq < 32) {
				s->priority1[irq][cpu] = src >> 4;
			} else {
				s->priority2[irq - 32] = src >> 4;
			}
			break;
		case 0x8: /* CPU targets */
			irq = offset - 0x800;
			if (GIC_NUM_IRQ(s) <= irq) {
				done = 0;
				break;
			}
			if (irq < 29) {
				src = 0x0;
			} else if (irq < 32) {
				src = GIC_ALL_CPU_MASK(s);
			}
			s->irq_target[irq] = src & GIC_ALL_CPU_MASK(s);
			break;
		case 0xC: /* Configuration */
			irq = (offset - 0xC00) * 4;
			if (GIC_NUM_IRQ(s) <= irq) {
				done = 0;
				break;
			}
			if (irq < 32) {
				src |= 0xAA;
			}
			for (i = 0; i < 4; i++) {
				if (src & (1 << (i * 2))) {
					GIC_SET_MODEL(s, irq + i);
				} else {
					GIC_CLEAR_MODEL(s, irq + i);
				}
				if (src & (2 << (i * 2))) {
					GIC_SET_TRIGGER(s, irq + i);
				} else {
					GIC_CLEAR_TRIGGER(s, irq + i);
				}
			}
			break;
		default:
			done = 0;
			break;
		};
	}

	if (done) {
		gic_update(s);
	}

	vmm_spin_unlock(&s->lock);

	if (!done) {
		return VMM_EFAIL;
	}

	return VMM_OK;
}

static int gic_dist_read(struct gic_state * s, int cpu, u32 offset, u32 *dst)
{
	int rc = VMM_OK, i;
	u8 val;

	if (!s || !dst) {
		return VMM_EFAIL;
	}

	*dst = 0;
	for (i = 0; i < 4; i++) {
		if ((rc = gic_dist_readb(s, cpu, 
					 offset + i, &val))) {
				break;
		}
		*dst |= val << (i * 8);
	}

	return VMM_OK;
}

static int gic_dist_write(struct gic_state * s, int cpu, u32 offset, 
			  u32 src_mask, u32 src)
{
	int rc = VMM_OK, irq, mask, i;

	if (!s) {
		return VMM_EFAIL;
	}

	if (offset == 0xF00) {
		/* Software Interrupt */
		vmm_spin_lock(&s->lock);
		irq = src & 0x3ff;
		switch ((src >> 24) & 3) {
		case 0:
			mask = (src >> 16) & GIC_ALL_CPU_MASK(s);
			break;
		case 1:
			mask = GIC_ALL_CPU_MASK(s) ^ (1 << cpu);
			break;
		case 2:
			mask = 1 << cpu;
			break;
		default:
			mask = GIC_ALL_CPU_MASK(s);
			break;
		};
		GIC_SET_PENDING(s, irq, mask);
		gic_update(s);
		vmm_spin_unlock(&s->lock);
	} else {
		src_mask = ~src_mask;
		for (i = 0; i < 4; i++) {
			if (src_mask & 0xFF) {
				if ((rc = gic_dist_writeb(s, cpu, 
						offset + i, src & 0xFF))) {
					break;
				}
			}
			src_mask = src_mask >> 8;
			src = src >> 8;
		}
	}

	return rc;
}

static int gic_cpu_read(struct gic_state * s, u32 cpu, u32 offset, u32 *dst)
{
	if (!s || !dst) {
		return VMM_EFAIL;
	}
	if (s->num_cpu <= cpu) {
		return VMM_EFAIL;
	}

	vmm_spin_lock(&s->lock);

	switch (offset) {
	case 0x00: /* Control */
		*dst = s->cpu_enabled[cpu];
		break;
	case 0x04: /* Priority mask */
		*dst = s->priority_mask[cpu];
		break;
	case 0x08: /* Binary Point */
		/* ??? Not implemented.  */
		break;
	case 0x0c: /* Acknowledge */
		*dst = gic_acknowledge_irq(s, cpu);
		break;
	case 0x14: /* Runing Priority */
		*dst = s->running_priority[cpu];
		break;
	case 0x18: /* Highest Pending Interrupt */
		*dst = s->current_pending[cpu];
		break;
	default:
		return VMM_EFAIL;
		break;
	}

	vmm_spin_unlock(&s->lock);

	return VMM_OK;
}

static int gic_cpu_write(struct gic_state * s, u32 cpu, u32 offset, 
			 u32 src_mask, u32 src)
{
	if (!s) {
		return VMM_EFAIL;
	}
	if (s->num_cpu <= cpu) {
		return VMM_EFAIL;
	}

	src = src & ~src_mask;

	vmm_spin_lock(&s->lock);

	switch (offset) {
	case 0x00: /* Control */
		s->cpu_enabled[cpu] = src & 0x1;
		break;
	case 0x04: /* Priority mask */
		s->priority_mask[cpu] = src & 0xFF;
		break;
	case 0x08: /* Binary Point */
		/* ??? Not implemented.  */
		break;
	case 0x10: /* End Of Interrupt */
		gic_complete_irq(s, cpu, src & 0x3ff);
		break;
	default:
		return VMM_EFAIL;
		break;
	};

	gic_update(s);

	vmm_spin_unlock(&s->lock);

	return VMM_OK;
}

/* Process a change in an external IRQ input.  */
static void gic_irq_hndl(vmm_emupic_t *epic, u32 irq, int level)
{
	struct gic_state * s = (struct gic_state *)epic->priv;

	/* Ensure irq is in range (base_irq, base_irq + num_irq)
	 * Also irq should not be in (base_irq, base_irq + 32)
	 * The first 32 irq in GIC are for inter processor communication */
	if ((irq < (s->num_base_irq + 32)) || 
	    ((s->num_base_irq + s->num_irq) <= irq)) {
		return;
	}

	if (level == GIC_TEST_LEVEL(s, irq, GIC_ALL_CPU_MASK(s)))
		return;

	vmm_spin_lock(&s->lock);

	if (level) {
		GIC_SET_LEVEL(s, irq, GIC_ALL_CPU_MASK(s));
		if (GIC_TEST_TRIGGER(s, irq) || GIC_TEST_ENABLED(s, irq)) {
			GIC_SET_PENDING(s, irq, GIC_TARGET(s, irq));
		}
	} else {
		GIC_CLEAR_LEVEL(s, irq, GIC_ALL_CPU_MASK(s));
	}

	gic_update(s);

	vmm_spin_unlock(&s->lock);
}

static int gic_emulator_read(vmm_emudev_t *edev,
			     physical_addr_t offset, 
			     void *dst, u32 dst_len)
{
	vmm_vcpu_t * vcpu = NULL;
	int rc = VMM_OK;
	u32 regval = 0x0;
	struct gic_state * s = edev->priv;

	vcpu = vmm_scheduler_current_vcpu();
	if (!vcpu || !vcpu->guest) {
		return VMM_EFAIL;
	}
	if (s->guest->num != vcpu->guest->num) {
		return VMM_EFAIL;
	}

	if (offset < 0x1000) {
		/* Read CPU Interface */
		rc = gic_cpu_read(s, vcpu->index, offset & ~0x3, &regval);
	} else {
		/* Read Distribution Control */
		rc = gic_dist_read(s, vcpu->index, 
				  (offset & ~0x3) - 0x1000, &regval);
	}

	if (!rc) {
		regval = (regval >> ((offset & 0x3) * 8));
		switch (dst_len) {
		case 1:
			((u8 *)dst)[0] = regval & 0xFF;
			break;
		case 2:
			((u8 *)dst)[0] = regval & 0xFF;
			((u8 *)dst)[1] = (regval >> 8) & 0xFF;
			break;
		case 4:
			((u8 *)dst)[0] = regval & 0xFF;
			((u8 *)dst)[1] = (regval >> 8) & 0xFF;
			((u8 *)dst)[2] = (regval >> 16) & 0xFF;
			((u8 *)dst)[3] = (regval >> 24) & 0xFF;
			break;
		default:
			rc = VMM_EFAIL;
			break;
		};
	}

	return rc;
}

static int gic_emulator_write(vmm_emudev_t *edev,
			      physical_addr_t offset, 
			      void *src, u32 src_len)
{
	vmm_vcpu_t * vcpu = NULL;
	int rc = VMM_OK, i;
	u32 regmask = 0x0, regval = 0x0;
	struct gic_state * s = edev->priv;

	switch (src_len) {
	case 1:
		regmask = 0xFFFFFF00;
		regval = ((u8 *)src)[0];
		break;
	case 2:
		regmask = 0xFFFF0000;
		regval = ((u8 *)src)[0];
		regval |= (((u8 *)src)[1] << 8);
		break;
	case 4:
		regmask = 0x00000000;
		regval = ((u8 *)src)[0];
		regval |= (((u8 *)src)[1] << 8);
		regval |= (((u8 *)src)[2] << 16);
		regval |= (((u8 *)src)[3] << 24);
		break;
	default:
		return VMM_EFAIL;
		break;
	};

	for (i = 0; i < (offset & 0x3); i++) {
		regmask = (regmask << 8) | ((regmask >> 24) & 0xFF);
	}
	regval = (regval << ((offset & 0x3) * 8));

	vcpu = vmm_scheduler_current_vcpu();
	if (!vcpu || !vcpu->guest) {
		return VMM_EFAIL;
	}
	if (s->guest->num != vcpu->guest->num) {
		return VMM_EFAIL;
	}

	if (offset < 0x1000) {
		/* Write CPU Interface */
		rc = gic_cpu_write(s, vcpu->index, 
				   offset & ~0x3, regmask, regval);
	} else {
		/* Write Distribution Control */
		rc = gic_dist_write(s, vcpu->index, 
				    (offset & ~0x3) - 0x1000, regmask, regval);
	}

	return rc;
}

static int gic_emulator_reset(vmm_emudev_t *edev)
{
	u32 i;
	struct gic_state * s = edev->priv;

	vmm_spin_lock(&s->lock);

	vmm_memset(s->irq_state, 0, s->num_irq * sizeof(struct gic_irq_state));
	for (i = 0; i < GIC_NUM_CPU(s); i++) {
		s->priority_mask[i] = 0xf0;
		s->current_pending[i] = 1023;
		s->running_irq[i] = 1023;
		s->running_priority[i] = 0x100;
		s->cpu_enabled[i] = 0;
	}
	for (i = 0; i < 16; i++) {
		GIC_SET_ENABLED(s, i);
		GIC_SET_TRIGGER(s, i);
	}
	s->enabled = 0;

	vmm_spin_unlock(&s->lock);

	return VMM_OK;
}

static int gic_emulator_probe(vmm_guest_t *guest,
			      vmm_emudev_t *edev,
			      const vmm_emuid_t *eid)
{
	int rc = VMM_OK, i;
	u32 attrlen;
	const char *attr;
	struct gic_state * s;

	s = vmm_malloc(sizeof(struct gic_state));
	if (!s) {
		rc = VMM_EFAIL;
		goto gic_emulator_probe_done;
	}
	vmm_memset(s, 0x0, sizeof(struct gic_state));

	s->pic = vmm_malloc(sizeof(vmm_emupic_t));
	if (!s->pic) {
		rc = VMM_EFAIL;
		goto gic_emulator_probe_freestate_fail;
	}
	vmm_strcpy(s->pic->name, "gic-pic");
	s->pic->hndl = &gic_irq_hndl;
	s->pic->priv = s;
	if ((rc = vmm_devemu_register_pic(guest, s->pic))) {
		rc = VMM_EFAIL;
		goto gic_emulator_probe_freepic_fail;
	}

	if (eid->data) {
		s->num_cpu = ((u32 *)eid->data)[0];
		s->num_irq = ((u32 *)eid->data)[1];
		s->num_base_irq = ((u32 *)eid->data)[2];
		s->id[0] = ((u32 *)eid->data)[3];
		s->id[1] = ((u32 *)eid->data)[4];
		s->id[2] = ((u32 *)eid->data)[5];
		s->id[3] = ((u32 *)eid->data)[6];
		s->id[4] = ((u32 *)eid->data)[7];
		s->id[5] = ((u32 *)eid->data)[8];
		s->id[6] = ((u32 *)eid->data)[9];
		s->id[7] = ((u32 *)eid->data)[10];
	}

	attr = vmm_devtree_attrval(edev->node, "child_pic");
	if (attr) {
		s->is_child_pic = TRUE;
	} else {
		s->is_child_pic = FALSE;
	}

	attr = vmm_devtree_attrval(edev->node, "parent_irq");
	attrlen = vmm_devtree_attrlen(edev->node, "parent_irq");
	if (attr && (attrlen == (s->num_cpu * sizeof(u32)))) {
		for (i = 0; i < s->num_cpu; i++) {
			s->parent_irq[i] = ((u32 *)attr)[i];
		}
	} else {
		rc = VMM_EFAIL;
		goto gic_emulator_probe_freepic_fail;
	}

	edev->priv = s;

	s->guest = guest;
	INIT_SPIN_LOCK(&s->lock);

	goto gic_emulator_probe_done;

gic_emulator_probe_freepic_fail:
	vmm_free(s->pic);
gic_emulator_probe_freestate_fail:
	vmm_free(s);
gic_emulator_probe_done:
	return rc;
}

static int gic_emulator_remove(vmm_emudev_t *edev)
{
	struct gic_state * s = edev->priv;

	vmm_free(s);

	return VMM_OK;
}

static u32 gic_configs[] = {
	/* === realview === */
	/* num_cpu */ 1, 
	/* num_irq */ 96,
	/* num_base_irq */ 0,
	/* id0 */ 0x90, 
	/* id1 */ 0x13, 
	/* id2 */ 0x04, 
	/* id3 */ 0x00, 
	/* id4 */ 0x0d, 
	/* id5 */ 0xf0, 
	/* id6 */ 0x05, 
	/* id7 */ 0xb1,
	/* reserved */ 0,
	/* reserved */ 0,
	/* reserved */ 0,
	/* reserved */ 0,
	/* reserved */ 0,
};

static vmm_emuid_t gic_emuid_table[] = {
	{ .type = "pic", 
	  .compatible = "realview,gic", 
	  .data = &gic_configs[0],
	},
	{ /* end of list */ },
};

static vmm_emulator_t gic_emulator = {
	.name = "gic",
	.match_table = gic_emuid_table,
	.probe = gic_emulator_probe,
	.read = gic_emulator_read,
	.write = gic_emulator_write,
	.reset = gic_emulator_reset,
	.remove = gic_emulator_remove,
};

static int gic_emulator_init(void)
{
	return vmm_devemu_register_emulator(&gic_emulator);
}

static void gic_emulator_exit(void)
{
	vmm_devemu_unregister_emulator(&gic_emulator);
}

VMM_DECLARE_MODULE(MODULE_VARID, 
			MODULE_NAME, 
			MODULE_AUTHOR, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
