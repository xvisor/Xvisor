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
 * @file vmm_scheduler.c
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief source file for vmm scheduler
 */

#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_heap.h>
#include <vmm_cpu.h>
#include <vmm_mterm.h>
#include <vmm_devtree.h>
#include <vmm_guest_aspace.h>
#include <vmm_vcpu_irq.h>
#include <vmm_scheduler.h>

/** Scheduler control structure */
vmm_scheduler_ctrl_t sched;

void vmm_scheduler_tick(vmm_user_regs_t * regs)
{
	if (!sched.scheduler_count) {
		vmm_scheduler_next(regs);
	} else {
		sched.scheduler_count--;
		if (-1 < sched.vcpu_current &&
		    sched.vcpu_array[sched.vcpu_current].tick_func) {
			sched.vcpu_array[sched.vcpu_current].
					tick_func(regs, sched.scheduler_count);
		}
	}
}

u32 vmm_scheduler_vcpu_count(void)
{
	return sched.vcpu_count;
}

u32 vmm_scheduler_guest_count(void)
{
	return sched.guest_count;
}

vmm_vcpu_t * vmm_scheduler_vcpu(s32 vcpu_no)
{
	if (-1 < vcpu_no && vcpu_no < sched.vcpu_count)
		return &sched.vcpu_array[vcpu_no];
	return NULL;
}

vmm_guest_t * vmm_scheduler_guest(s32 guest_no)
{
	if (-1 < guest_no && guest_no < sched.guest_count)
		return &sched.guest_array[guest_no];
	return NULL;
}

u32 vmm_scheduler_guest_vcpu_count(vmm_guest_t *guest)
{
	u32 ret = 0;
	struct dlist *l;

	if (!guest) {
		return 0;
	}

	list_for_each(l, &guest->vcpu_list) {
		ret++;
	}
	
	return ret;
}

vmm_vcpu_t * vmm_scheduler_guest_vcpu(vmm_guest_t *guest, int index)
{
	vmm_vcpu_t *vcpu = NULL;
	struct dlist *l;

	if (!guest || (index < 0)) {
		return NULL;
	}

	list_for_each(l, &guest->vcpu_list) {
		if (!index) {
			vcpu = list_entry(l, vmm_vcpu_t, head);
			break;
		}
		index--;
	}

	return vcpu;
}

int vmm_scheduler_guest_vcpu_index(vmm_guest_t *guest, vmm_vcpu_t *vcpu)
{
	int ret = -1, index = 0;
	vmm_vcpu_t *tvcpu = NULL;
	struct dlist *l;

	if (!guest || !vcpu) {
		return -1;
	}

	list_for_each(l, &guest->vcpu_list) {
		tvcpu = list_entry(l, vmm_vcpu_t, head);
		if (tvcpu->num == vcpu->num) {
			ret = index;
			break;
		}
		index++;
	}

	return ret;
}

vmm_vcpu_t * vmm_scheduler_current_vcpu(void)
{
	if (sched.vcpu_current != -1)
		return &sched.vcpu_array[sched.vcpu_current];
	return NULL;
}

vmm_guest_t * vmm_scheduler_current_guest(void)
{
	if (sched.vcpu_current != -1)
		return sched.vcpu_array[sched.vcpu_current].guest;
	return NULL;
}

void vmm_scheduler_next(vmm_user_regs_t * regs)
{
	u32 vcpu_next;

	/* Save the state of current running vcpu */
	if (-1 < sched.vcpu_current) {
		sched.vcpu_array[sched.vcpu_current].state =
		    VMM_VCPU_STATE_READY;
	}

	/* Determine the next ready vcpu to schedule */
	vcpu_next = (sched.vcpu_current + 1) % (sched.vcpu_count);
	while (sched.vcpu_array[vcpu_next].state != VMM_VCPU_STATE_READY) {
		vcpu_next = (vcpu_next + 1) % (sched.vcpu_count);
	}

	/* Restore the state of next vcpu */
	if (sched.vcpu_current != vcpu_next) {
		if (-1 < sched.vcpu_current) {
			vmm_vcpu_regs_switch(&sched.
					     vcpu_array[sched.vcpu_current],
					     &sched.vcpu_array[vcpu_next],
					     regs);
		} else {
			vmm_vcpu_regs_switch(NULL,
					     &sched.vcpu_array[vcpu_next],
					     regs);
		}
		sched.vcpu_array[vcpu_next].state = VMM_VCPU_STATE_RUNNING;
		sched.vcpu_current = vcpu_next;
	}

	/* Reload tick count from current vcpu */
	sched.scheduler_count = sched.vcpu_array[sched.vcpu_current].tick_count;
}

int vmm_scheduler_vcpu_kick(vmm_vcpu_t * vcpu)
{
	irq_flags_t flags;
	if (vcpu) {
		if (vcpu->state == VMM_VCPU_STATE_RESET) {
			/* Acquire lock */
			flags = vmm_spin_lock_irqsave(&sched.lock);
			vcpu->state = VMM_VCPU_STATE_READY;
			/* Release lock */
			vmm_spin_unlock_irqrestore(&sched.lock, flags);
			return VMM_OK;
		}
	}
	return VMM_EFAIL;
}

int vmm_scheduler_vcpu_pause(vmm_vcpu_t * vcpu)
{
	irq_flags_t flags;
	if (vcpu) {
		if (vcpu->state == VMM_VCPU_STATE_READY) {
			/* Acquire lock */
			flags = vmm_spin_lock_irqsave(&sched.lock);
			vcpu->state = VMM_VCPU_STATE_PAUSED;
			/* Release lock */
			vmm_spin_unlock_irqrestore(&sched.lock, flags);
			return VMM_OK;
		}
	}
	return VMM_EFAIL;
}

int vmm_scheduler_vcpu_resume(vmm_vcpu_t * vcpu)
{
	irq_flags_t flags;
	if (vcpu) {
		if (vcpu->state == VMM_VCPU_STATE_PAUSED) {
			/* Acquire lock */
			flags = vmm_spin_lock_irqsave(&sched.lock);
			vcpu->state = VMM_VCPU_STATE_READY;
			/* Release lock */
			vmm_spin_unlock_irqrestore(&sched.lock, flags);
			return VMM_OK;
		}
	}
	return VMM_EFAIL;
}

int vmm_scheduler_vcpu_halt(vmm_vcpu_t * vcpu)
{
	irq_flags_t flags;
	if (vcpu) {
		if (vcpu->state != VMM_VCPU_STATE_RUNNING &&
		    vcpu->state != VMM_VCPU_STATE_HALTED) {
			/* Acquire lock */
			flags = vmm_spin_lock_irqsave(&sched.lock);
			vcpu->state = VMM_VCPU_STATE_HALTED;
			/* Release lock */
			vmm_spin_unlock_irqrestore(&sched.lock, flags);
			return VMM_OK;
		}
	}
	return VMM_EFAIL;
}

int vmm_scheduler_vcpu_dumpreg(vmm_vcpu_t * vcpu)
{
	if (vcpu) {
		if (vcpu->state != VMM_VCPU_STATE_RUNNING) {
			vmm_vcpu_regs_dump(vcpu);
			return VMM_OK;
		}
	}
	return VMM_EFAIL;
}

vmm_vcpu_t * vmm_scheduler_vcpu_orphan_create(const char *name,
					      virtual_addr_t start_pc,
					      u32 tick_count,
					      vmm_vcpu_tick_t tick_func)
{
	vmm_vcpu_t *vcpu;
	irq_flags_t flags;

	/* Sanity checks */
	if (name == NULL || start_pc == 0 || tick_count == 0) {
		return NULL;
	}

	/* Acquire lock */
	flags = vmm_spin_lock_irqsave(&sched.lock);

	/* Find the next available vcpu */
	vcpu = &sched.vcpu_array[sched.vcpu_count];

	/* Add it to orphan list */
	list_add_tail(&sched.orphan_vcpu_list, &vcpu->head);

	/* Update vcpu attributes */
	vmm_strcpy(vcpu->name, name);
	vcpu->node = NULL;
	vcpu->tick_count = tick_count;
	vcpu->tick_func = tick_func;
	vcpu->state = VMM_VCPU_STATE_READY;
	vcpu->start_pc = start_pc;
	vcpu->bootpg_addr = 0;
	vcpu->bootpg_size = 0;
	vcpu->guest = NULL;
	vmm_vcpu_regs_init(vcpu);

	/* Increment vcpu count */
	sched.vcpu_count++;

	/* Release lock */
	vmm_spin_unlock_irqrestore(&sched.lock, flags);

	return vcpu;
}

int vmm_scheduler_vcpu_orphan_destroy(vmm_vcpu_t * vcpu)
{
	/* FIXME: TBD */
	return VMM_OK;
}

void vmm_scheduler_start(void)
{
	/** Setup timer */
	vmm_cpu_timer_setup(sched.tick_usecs);

	/** Enable timer */
	vmm_cpu_timer_enable();
}

void vmm_scheduler_stop(void)
{
	/** Disable timer */
	vmm_cpu_timer_disable();
}

u32 vmm_scheduler_tick_usecs(void)
{
	return sched.tick_usecs;
}

int vmm_scheduler_init(void)
{
	u32 vnum, gnum;
	const char *attrval;
	struct dlist *l, *l1;
	vmm_devtree_node_t *gsnode;
	vmm_devtree_node_t *gnode;
	vmm_devtree_node_t *vsnode;
	vmm_devtree_node_t *vnode;
	vmm_guest_t *guest;
	vmm_vcpu_t *vcpu;

	/* Reset the scheduler control structure */
	vmm_memset(&sched, 0, sizeof(sched));

	/* Initialize scheduling parameters */
	sched.vcpu_current = -1;
	sched.scheduler_count = 0;

	/* Intialize guest & vcpu managment parameters */
	INIT_SPIN_LOCK(&sched.lock);
	sched.max_vcpu_count = 0;
	sched.max_guest_count = 0;
	sched.vcpu_count = 0;
	sched.guest_count = 0;
	sched.vcpu_array = NULL;
	sched.guest_array = NULL;
	INIT_LIST_HEAD(&sched.orphan_vcpu_list);
	INIT_LIST_HEAD(&sched.guest_list);

	/* Get VMM information node */
	vnode = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPRATOR_STRING
				    VMM_DEVTREE_VMMINFO_NODE_NAME);
	if (!vnode) {
		return VMM_EFAIL;
	}

	/* Get max vcpu count */
	attrval = vmm_devtree_attrval(vnode,
				      VMM_DEVTREE_MAX_VCPU_COUNT_ATTR_NAME);
	if (!attrval) {
		return VMM_EFAIL;
	}
	sched.max_vcpu_count = *((u32 *) attrval);

	/* Get max guest count */
	attrval = vmm_devtree_attrval(vnode,
				      VMM_DEVTREE_MAX_GUEST_COUNT_ATTR_NAME);
	if (!attrval) {
		return VMM_EFAIL;
	}
	sched.max_guest_count = *((u32 *) attrval);

	/* Allocate memory for guest instances */
	sched.guest_array =
	    vmm_malloc(sizeof(vmm_guest_t) * sched.max_guest_count);

	/* Initialze memory for guest instances */
	for (gnum = 0; gnum < sched.max_guest_count; gnum++) {
		vmm_memset(&sched.guest_array[gnum], 0, sizeof(vmm_guest_t));
		INIT_LIST_HEAD(&sched.guest_array[gnum].head);
		sched.guest_array[gnum].num = gnum;
		sched.guest_array[gnum].node = NULL;
		INIT_LIST_HEAD(&sched.guest_array[gnum].vcpu_list);
	}

	/* Allocate memory for vcpu instances */
	sched.vcpu_array =
	    vmm_malloc(sizeof(vmm_vcpu_t) * sched.max_vcpu_count);

	/* Initialze memory for vcpu instances */
	for (vnum = 0; vnum < sched.max_vcpu_count; vnum++) {
		vmm_memset(&sched.vcpu_array[vnum], 0, sizeof(vmm_vcpu_t));
		INIT_LIST_HEAD(&sched.vcpu_array[vnum].head);
		sched.vcpu_array[vnum].num = vnum;
		vmm_strcpy(sched.vcpu_array[vnum].name, "");
		sched.vcpu_array[vnum].node = NULL;
		sched.vcpu_array[vnum].state = VMM_VCPU_STATE_UNKNOWN;
	}

	/* Retrive the guest information node */
	gsnode = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPRATOR_STRING
				     VMM_DEVTREE_GUESTINFO_NODE_NAME);
	if (!gsnode) {
		return VMM_EFAIL;
	}

	/* Count vcpu and guest instances from device tree */
	sched.guest_count = 0;
	sched.vcpu_count = 0;
	list_for_each(l, &gsnode->child_list) {
		gnode = list_entry(l, vmm_devtree_node_t, head);

		/* Sanity checks */
		attrval = vmm_devtree_attrval(gnode,
					      VMM_DEVTREE_DEVICE_TYPE_ATTR_NAME);
		if (!attrval) {
			continue;
		}
		if (vmm_strcmp(attrval, VMM_DEVTREE_DEVICE_TYPE_VAL_GUEST) != 0) {
			continue;
		}

		/* Increment guest count */
		sched.guest_count++;

		vsnode = vmm_devtree_getchildnode(gnode,
						  VMM_DEVTREE_VCPUS_NODE_NAME);
		if (!vsnode) {
			continue;
		}
		list_for_each(l1, &vsnode->child_list) {
			vnode = list_entry(l1, vmm_devtree_node_t, head);

			/* Sanity checks */
			attrval = vmm_devtree_attrval(vnode,
						      VMM_DEVTREE_DEVICE_TYPE_ATTR_NAME);
			if (!attrval) {
				continue;
			}
			if (vmm_strcmp(attrval,
				       VMM_DEVTREE_DEVICE_TYPE_VAL_VCPU) != 0) {
				continue;
			}

			/* Increment vcpu count */
			sched.vcpu_count++;
		}
	}
	if (sched.vcpu_count > sched.max_vcpu_count) {
		return VMM_EFAIL;
	}
	if (sched.guest_count > sched.max_guest_count) {
		return VMM_EFAIL;
	}

	/* Populate vcpu and guest instances from device tree */
	gnum = 0;
	vnum = 0;
	list_for_each(l, &gsnode->child_list) {
		gnode = list_entry(l, vmm_devtree_node_t, head);

		/* Sanity checks */
		attrval = vmm_devtree_attrval(gnode,
					      VMM_DEVTREE_DEVICE_TYPE_ATTR_NAME);
		if (!attrval) {
			continue;
		}
		if (vmm_strcmp(attrval, VMM_DEVTREE_DEVICE_TYPE_VAL_GUEST) != 0) {
			continue;
		}

		/* Initialize guest instance */
		guest = &sched.guest_array[gnum];
		list_add_tail(&sched.guest_list, &guest->head);
		guest->node = gnode;
		INIT_LIST_HEAD(&guest->vcpu_list);
		vmm_guest_aspace_initguest(guest);

		/* Increment guest number */
		gnum++;

		vsnode = vmm_devtree_getchildnode(gnode,
						  VMM_DEVTREE_VCPUS_NODE_NAME);
		if (!vsnode) {
			continue;
		}
		list_for_each(l1, &vsnode->child_list) {
			vnode = list_entry(l1, vmm_devtree_node_t, head);

			/* Sanity checks */
			attrval = vmm_devtree_attrval(vnode,
						      VMM_DEVTREE_DEVICE_TYPE_ATTR_NAME);
			if (!attrval) {
				continue;
			}
			if (vmm_strcmp(attrval,
				       VMM_DEVTREE_DEVICE_TYPE_VAL_VCPU) != 0) {
				continue;
			}

			/* Initialize vcpu instance */
			vcpu = &sched.vcpu_array[vnum];
			list_add_tail(&guest->vcpu_list, &vcpu->head);
			vmm_strcpy(vcpu->name, gnode->name);
			vmm_strcat(vcpu->name,
				   VMM_DEVTREE_PATH_SEPRATOR_STRING);
			vmm_strcat(vcpu->name, vnode->name);
			vcpu->node = vnode;
			vcpu->state = VMM_VCPU_STATE_RESET;
			attrval = vmm_devtree_attrval(vnode,
						      VMM_DEVTREE_TICK_COUNT_ATTR_NAME);
			if (attrval) {
				vcpu->tick_count =
				    *((virtual_addr_t *) attrval);
			}
			attrval = vmm_devtree_attrval(vnode,
						      VMM_DEVTREE_START_PC_ATTR_NAME);
			if (attrval) {
				vcpu->start_pc = *((virtual_addr_t *) attrval);
			}
			attrval = vmm_devtree_attrval(vnode,
						      VMM_DEVTREE_BOOTPG_ADDR_ATTR_NAME);
			if (attrval) {
				vcpu->bootpg_addr =
				    *((physical_addr_t *) attrval);
			}
			attrval = vmm_devtree_attrval(vnode,
						      VMM_DEVTREE_BOOTPG_SIZE_ATTR_NAME);
			if (attrval) {
				vcpu->bootpg_size =
				    *((physical_addr_t *) attrval);
			}
			vcpu->guest = guest;
			vmm_vcpu_regs_init(vcpu);
			vmm_vcpu_irq_initvcpu(vcpu);

			/* Increment vcpu number */
			vnum++;
		}
	}

	/* Find out tick delay in microseconds */
	vnode = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPRATOR_STRING
				   VMM_DEVTREE_VMMINFO_NODE_NAME);
	if (!vnode) {
		return VMM_EFAIL;
	}
	attrval = vmm_devtree_attrval(vnode,
				      VMM_DEVTREE_TICK_DELAY_USECS_ATTR_NAME);
	if (!attrval) {
		return VMM_EFAIL;
	}
	sched.tick_usecs = *((u32 *) attrval);

	return VMM_OK;
}
