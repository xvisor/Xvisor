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
 * @file vmm_devemu.c
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief source code for device emulation framework
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_string.h>
#include <vmm_stdio.h>
#include <vmm_scheduler.h>
#include <vmm_guest_aspace.h>
#include <vmm_devemu.h>

vmm_devemu_ctrl_t dectrl;

int vmm_devemu_emulate_read(vmm_guest_t *guest, 
			    physical_addr_t gphys_addr,
			    void *dst, u32 dst_len)
{
	vmm_emudev_t *edev;
	vmm_guest_region_t *reg;

	reg = vmm_guest_aspace_getregion(guest, gphys_addr);
	if (!reg || !reg->is_virtual) {
		return VMM_EFAIL;
	}

	edev = (vmm_emudev_t *)reg->priv;
	if (!edev || !edev->read) {
		return VMM_EFAIL;
	}

	return edev->read(edev, gphys_addr - reg->gphys_addr, dst, dst_len);
}

int vmm_devemu_emulate_write(vmm_guest_t *guest, 
			     physical_addr_t gphys_addr,
			     void *src, u32 src_len)
{
	vmm_emudev_t *edev;
	vmm_guest_region_t *reg;

	reg = vmm_guest_aspace_getregion(guest, gphys_addr);
	if (!reg || !reg->is_virtual) {
		return VMM_EFAIL;
	}

	edev = (vmm_emudev_t *)reg->priv;
	if (!edev || !edev->write) {
		return VMM_EFAIL;
	}

	return edev->write(edev, gphys_addr - reg->gphys_addr, src, src_len);
}

int vmm_devemu_emulate_irq(vmm_guest_t *guest, u32 irq_num, int irq_level)
{
	struct dlist *l;
	vmm_emupic_t *ep;
	vmm_emuguest_t *eg;

	if (!guest) {
		return VMM_EFAIL;
	}

	eg = (vmm_emuguest_t *)guest->aspace.priv;

	list_for_each(l, &eg->emupic_list) {
		ep = list_entry(l, vmm_emupic_t, head);
		ep->hndl(ep, irq_num, irq_level);
	}

	return VMM_OK;
}

int vmm_devemu_register_pic(vmm_guest_t *guest, vmm_emupic_t * pic)
{
	bool found;
	struct dlist *l;
	vmm_emupic_t *ep;
	vmm_emuguest_t *eg;

	if (!guest || !pic) {
		return VMM_EFAIL;
	}

	eg = (vmm_emuguest_t *)guest->aspace.priv;
	ep = NULL;
	found = FALSE;
	list_for_each(l, &eg->emupic_list) {
		ep = list_entry(l, vmm_emupic_t, head);
		if (vmm_strcmp(ep->name, pic->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (found) {
		return VMM_EINVALID;
	}

	INIT_LIST_HEAD(&pic->head);

	list_add_tail(&eg->emupic_list, &pic->head);

	return VMM_OK;
}

int vmm_devemu_unregister_pic(vmm_guest_t *guest, vmm_emupic_t * pic)
{
	bool found;
	struct dlist *l;
	vmm_emupic_t *ep;
	vmm_emuguest_t *eg;

	if (!guest || !pic) {
		return VMM_EFAIL;
	}

	eg = (vmm_emuguest_t *)guest->aspace.priv;

	if (list_empty(&eg->emupic_list)) {
		return VMM_EFAIL;
	}

	ep = NULL;
	found = FALSE;
	list_for_each(l, &eg->emupic_list) {
		ep = list_entry(l, vmm_emupic_t, head);
		if (vmm_strcmp(ep->name, pic->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		return VMM_ENOTAVAIL;
	}

	list_del(&ep->head);

	return VMM_OK;
}

vmm_emupic_t *vmm_devemu_find_pic(vmm_guest_t *guest, const char *name)
{
	bool found;
	struct dlist *l;
	vmm_emuguest_t *eg;
	vmm_emupic_t *ep;

	if (!guest || !name) {
		return NULL;
	}

	eg = (vmm_emuguest_t *)guest->aspace.priv;
	found = FALSE;
	ep = NULL;

	list_for_each(l, &eg->emupic_list) {
		ep = list_entry(l, vmm_emupic_t, head);
		if (vmm_strcmp(ep->name, name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		return NULL;
	}

	return ep;
}

vmm_emupic_t *vmm_devemu_pic(vmm_guest_t *guest, int index)
{
	bool found;
	struct dlist *l;
	vmm_emuguest_t *eg;
	vmm_emupic_t *retval;

	if (!guest) {
		return NULL;
	}
	if (index < 0) {
		return NULL;
	}

	eg = (vmm_emuguest_t *)guest->aspace.priv;
	retval = NULL;
	found = FALSE;

	list_for_each(l, &eg->emupic_list) {
		retval = list_entry(l, vmm_emupic_t, head);
		if (!index) {
			found = TRUE;
			break;
		}
		index--;
	}

	if (!found) {
		return NULL;
	}

	return retval;
}

u32 vmm_devemu_pic_count(vmm_guest_t *guest)
{
	u32 retval = 0;
	vmm_emuguest_t *eg;
	struct dlist *l;

	if (!guest) {
		return 0;
	}

	eg = (vmm_emuguest_t *)guest->aspace.priv;

	list_for_each(l, &eg->emupic_list) {
		retval++;
	}

	return retval;
}

static void devemu_clk_tick(vmm_user_regs_t * regs, u32 ticks_left)
{
	struct dlist *l;
	vmm_guest_t *guest = vmm_scheduler_current_guest();
	vmm_emuguest_t *eg = NULL;
	vmm_emuclk_t *ec;
	
	if (!guest) {
		return;
	}

	eg = (vmm_emuguest_t *)guest->aspace.priv;
	
	list_for_each(l, &eg->emuclk_list) {
		ec = list_entry(l, vmm_emuclk_t, head);
		ec->tick(ec);
	}
}

u32 vmm_devemu_clk_microsecs(void)
{
	/* FIXME: Need more fine tuning here */
	return vmm_scheduler_tick_usecs();
}

int vmm_devemu_register_clk(vmm_guest_t *guest, vmm_emuclk_t * clk)
{
	bool found;
	struct dlist *l;
	vmm_vcpu_t *vcpu;
	vmm_emuclk_t *ec;
	vmm_emuguest_t *eg;

	if (!guest || !clk) {
		return VMM_EFAIL;
	}

	eg = (vmm_emuguest_t *)guest->aspace.priv;
	ec = NULL;
	found = FALSE;
	list_for_each(l, &eg->emuclk_list) {
		ec = list_entry(l, vmm_emuclk_t, head);
		if (vmm_strcmp(ec->name, clk->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (found) {
		return VMM_EINVALID;
	}

	INIT_LIST_HEAD(&clk->head);

	list_add_tail(&eg->emuclk_list, &clk->head);

	/* Update tick function of vcpu instances */
	list_for_each(l, &guest->vcpu_list) {
		vcpu = list_entry(l, vmm_vcpu_t, head);
		vcpu->tick_func = &devemu_clk_tick;
	}

	return VMM_OK;
}

int vmm_devemu_unregister_clk(vmm_guest_t *guest, vmm_emuclk_t * clk)
{
	bool found;
	struct dlist *l;
	vmm_emuclk_t *ec;
	vmm_emuguest_t *eg;

	if (!guest || !clk) {
		return VMM_EFAIL;
	}

	eg = (vmm_emuguest_t *)guest->aspace.priv;

	if (list_empty(&eg->emuclk_list)) {
		return VMM_EFAIL;
	}

	ec = NULL;
	found = FALSE;
	list_for_each(l, &eg->emuclk_list) {
		ec = list_entry(l, vmm_emuclk_t, head);
		if (vmm_strcmp(ec->name, clk->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		return VMM_ENOTAVAIL;
	}

	list_del(&ec->head);

	return VMM_OK;
}

vmm_emuclk_t *vmm_devemu_find_clk(vmm_guest_t *guest, const char *name)
{
	bool found;
	struct dlist *l;
	vmm_emuguest_t *eg;
	vmm_emuclk_t *ec;

	if (!guest || !name) {
		return NULL;
	}

	eg = (vmm_emuguest_t *)guest->aspace.priv;
	found = FALSE;
	ec = NULL;

	list_for_each(l, &eg->emuclk_list) {
		ec = list_entry(l, vmm_emuclk_t, head);
		if (vmm_strcmp(ec->name, name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		return NULL;
	}

	return ec;
}

vmm_emuclk_t *vmm_devemu_clk(vmm_guest_t *guest, int index)
{
	bool found;
	struct dlist *l;
	vmm_emuguest_t *eg;
	vmm_emuclk_t *retval;

	if (!guest) {
		return NULL;
	}
	if (index < 0) {
		return NULL;
	}

	eg = (vmm_emuguest_t *)guest->aspace.priv;
	retval = NULL;
	found = FALSE;

	list_for_each(l, &eg->emuclk_list) {
		retval = list_entry(l, vmm_emuclk_t, head);
		if (!index) {
			found = TRUE;
			break;
		}
		index--;
	}

	if (!found) {
		return NULL;
	}

	return retval;
}

u32 vmm_devemu_clk_count(vmm_guest_t *guest)
{
	u32 retval = 0;
	vmm_emuguest_t *eg;
	struct dlist *l;

	if (!guest) {
		return 0;
	}

	eg = (vmm_emuguest_t *)guest->aspace.priv;

	list_for_each(l, &eg->emuclk_list) {
		retval++;
	}

	return retval;
}

int vmm_devemu_register_emulator(vmm_emulator_t * emu)
{
	bool found;
	struct dlist *l;
	vmm_emulator_t *e;

	if (emu == NULL) {
		return VMM_EFAIL;
	}

	e = NULL;
	found = FALSE;
	list_for_each(l, &dectrl.emu_list) {
		e = list_entry(l, vmm_emulator_t, head);
		if (vmm_strcmp(e->name, emu->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (found) {
		return VMM_EINVALID;
	}

	INIT_LIST_HEAD(&emu->head);

	list_add_tail(&dectrl.emu_list, &emu->head);

	return VMM_OK;
}

int vmm_devemu_unregister_emulator(vmm_emulator_t * emu)
{
	bool found;
	struct dlist *l;
	vmm_emulator_t *e;

	if (emu == NULL || list_empty(&dectrl.emu_list)) {
		return VMM_EFAIL;
	}

	e = NULL;
	found = FALSE;
	list_for_each(l, &dectrl.emu_list) {
		e = list_entry(l, vmm_emulator_t, head);
		if (vmm_strcmp(e->name, emu->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		return VMM_ENOTAVAIL;
	}

	list_del(&e->head);

	return VMM_OK;
}

vmm_emulator_t *vmm_devemu_find_emulator(const char *name)
{
	bool found;
	struct dlist *l;
	vmm_emulator_t *emu;

	if (!name) {
		return NULL;
	}

	found = FALSE;
	emu = NULL;

	list_for_each(l, &dectrl.emu_list) {
		emu = list_entry(l, vmm_emulator_t, head);
		if (vmm_strcmp(emu->name, name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		return NULL;
	}

	return emu;
}

vmm_emulator_t *vmm_devemu_emulator(int index)
{
	bool found;
	struct dlist *l;
	vmm_emulator_t *retval;

	if (index < 0) {
		return NULL;
	}

	retval = NULL;
	found = FALSE;

	list_for_each(l, &dectrl.emu_list) {
		retval = list_entry(l, vmm_emulator_t, head);
		if (!index) {
			found = TRUE;
			break;
		}
		index--;
	}

	if (!found) {
		return NULL;
	}

	return retval;
}

u32 vmm_devemu_emulator_count(void)
{
	u32 retval;
	struct dlist *l;

	retval = 0;

	list_for_each(l, &dectrl.emu_list) {
		retval++;
	}

	return retval;
}

int devemu_device_is_compatible(vmm_devtree_node_t * node, const char *compat)
{
	const char *cp;
	int cplen, l;

	cp = vmm_devtree_attrval(node, VMM_DEVTREE_COMPATIBLE_ATTR_NAME);
	cplen = vmm_devtree_attrlen(node, VMM_DEVTREE_COMPATIBLE_ATTR_NAME);
	if (cp == NULL)
		return 0;
	while (cplen > 0) {
		if (vmm_strncmp(cp, compat, vmm_strlen(compat)) == 0)
			return 1;
		l = vmm_strlen(cp) + 1;
		cp += l;
		cplen -= l;
	}

	return 0;
}

const vmm_emuid_t *devemu_match_node(const vmm_emuid_t * matches,
				     vmm_devtree_node_t * node)
{
	const char *node_type;

	if (!matches || !node) {
		return NULL;
	}

	node_type = vmm_devtree_attrval(node,
					VMM_DEVTREE_DEVICE_TYPE_ATTR_NAME);
	while (matches->name[0] || matches->type[0] || matches->compatible[0]) {
		int match = 1;
		if (matches->name[0])
			match &= node->name
			    && !vmm_strcmp(matches->name, node->name);
		if (matches->type[0])
			match &= node_type
			    && !vmm_strcmp(matches->type, node_type);
		if (matches->compatible[0])
			match &= devemu_device_is_compatible(node,
							     matches->
							     compatible);
		if (match)
			return matches;
		matches++;
	}

	return NULL;
}

int vmm_devemu_probe(vmm_guest_t *guest, vmm_guest_region_t *reg)
{
	int rc;
	struct dlist *l1;
	vmm_emudev_t *einst;
	vmm_emuguest_t *eginst;
	vmm_emulator_t *emu;
	const vmm_emuid_t *matches;
	const vmm_emuid_t *match;

	if (!guest->aspace.priv) {
		eginst = vmm_malloc(sizeof(vmm_emuguest_t));
		INIT_LIST_HEAD(&eginst->emupic_list);
		INIT_LIST_HEAD(&eginst->emuclk_list);
		guest->aspace.priv = eginst;
	} else {
		eginst = guest->aspace.priv;
	}

	list_for_each(l1, &dectrl.emu_list) {
		emu = list_entry(l1, vmm_emulator_t, head);
		matches = emu->match_table;
		match = devemu_match_node(matches, reg->node);
		if (match) {
			einst = vmm_malloc(sizeof(vmm_emudev_t));
			INIT_SPIN_LOCK(&einst->lock);
			einst->node = reg->node;
			einst->probe = emu->probe;
			einst->read = emu->read;
			einst->write = emu->write;
			einst->reset = emu->reset;
			einst->remove = emu->remove;
			einst->priv = NULL;
			reg->priv = einst;
			reg->node->type = VMM_DEVTREE_NODETYPE_EDEVICE;
			reg->node->priv = einst;
			vmm_printf("Probe edevice %s/%s\n",
				   guest->node->name, reg->node->name);
			if ((rc = einst->probe(guest, einst, match))) {
				vmm_printf("Error %d\n", rc);
				vmm_free(einst);
				reg->priv = NULL;
				reg->node->type = VMM_DEVTREE_NODETYPE_UNKNOWN;
				reg->node->priv = NULL;
			}
			if ((rc = einst->reset(einst))) {
				vmm_printf("Error %d\n", rc);
				vmm_free(einst);
				reg->priv = NULL;
				reg->node->type = VMM_DEVTREE_NODETYPE_UNKNOWN;
				reg->node->priv = NULL;
			}
			break;
		}
	}

	return VMM_OK;
}

int vmm_devemu_init(void)
{
	vmm_memset(&dectrl, 0, sizeof(dectrl));

	INIT_LIST_HEAD(&dectrl.emu_list);

	return VMM_OK;
}
