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
#include <vmm_string.h>
#include <vmm_stdio.h>
#include <vmm_heap.h>
#include <vmm_guest_aspace.h>
#include <vmm_devemu.h>

struct vmm_devemu_vcpu_context {
	u32 rd_victim;
	physical_addr_t rd_gphys[CONFIG_VGPA2REG_CACHE_SIZE];
	vmm_region_t * rd_reg[CONFIG_VGPA2REG_CACHE_SIZE];
	u32 wr_victim;
	physical_addr_t wr_gphys[CONFIG_VGPA2REG_CACHE_SIZE];
	vmm_region_t * wr_reg[CONFIG_VGPA2REG_CACHE_SIZE];
};

struct vmm_devemu_guest_context {
	struct dlist emupic_list;
};

struct vmm_devemu_ctrl {
        struct dlist emu_list;
};

static struct vmm_devemu_ctrl dectrl;

int vmm_devemu_emulate_read(vmm_vcpu_t *vcpu, 
			    physical_addr_t gphys_addr,
			    void *dst, u32 dst_len)
{
	u32 ite;
	bool found;
	struct vmm_devemu_vcpu_context * ev;
	vmm_emudev_t *edev;
	vmm_region_t *reg;

	if (!vcpu || !(vcpu->guest)) {
		return VMM_EFAIL;
	}

	ev = vcpu->devemu_priv;
	found = FALSE;
	for (ite = 0; ite < CONFIG_VGPA2REG_CACHE_SIZE; ite++) {
		if (ev->rd_reg[ite] && (ev->rd_gphys[ite] == gphys_addr)) {
			reg = ev->rd_reg[ite];
			found = TRUE;
			break;
		}
	}

	if (!found) {
		reg = vmm_guest_find_region(vcpu->guest, gphys_addr, FALSE);
		if (!reg || !(reg->flags & VMM_REGION_VIRTUAL)) {
			return VMM_EFAIL;
		}
		ev->rd_gphys[ev->rd_victim] = gphys_addr;
		ev->rd_reg[ev->rd_victim] = reg;
		if (ev->rd_victim == (CONFIG_VGPA2REG_CACHE_SIZE - 1)) {
			ev->rd_victim = 0;
		} else {
			ev->rd_victim++;
		}
	}

	edev = (vmm_emudev_t *)reg->devemu_priv;
	if (!edev || !edev->read) {
		return VMM_EFAIL;
	}

	return edev->read(edev, gphys_addr - reg->gphys_addr, dst, dst_len);
}

int vmm_devemu_emulate_write(vmm_vcpu_t *vcpu, 
			     physical_addr_t gphys_addr,
			     void *src, u32 src_len)
{
	u32 ite;
	bool found;
	struct vmm_devemu_vcpu_context * ev;
	vmm_emudev_t *edev;
	vmm_region_t *reg;

	if (!vcpu || !(vcpu->guest)) {
		return VMM_EFAIL;
	}

	ev = vcpu->devemu_priv;
	found = FALSE;
	for (ite = 0; ite < CONFIG_VGPA2REG_CACHE_SIZE; ite++) {
		if (ev->wr_reg[ite] && (ev->wr_gphys[ite] == gphys_addr)) {
			reg = ev->wr_reg[ite];
			found = TRUE;
			break;
		}
	}

	if (!found) {
		reg = vmm_guest_find_region(vcpu->guest, gphys_addr, FALSE);
		if (!reg || !(reg->flags & VMM_REGION_VIRTUAL)) {
			return VMM_EFAIL;
		}
		ev->wr_gphys[ev->wr_victim] = gphys_addr;
		ev->wr_reg[ev->wr_victim] = reg;
		if (ev->wr_victim == (CONFIG_VGPA2REG_CACHE_SIZE - 1)) {
			ev->wr_victim = 0;
		} else {
			ev->wr_victim++;
		}
	}

	edev = (vmm_emudev_t *)reg->devemu_priv;
	if (!edev || !edev->write) {
		return VMM_EFAIL;
	}

	return edev->write(edev, gphys_addr - reg->gphys_addr, src, src_len);
}

int vmm_devemu_emulate_irq(vmm_guest_t *guest, u32 irq_num, int irq_level)
{
	struct dlist *l;
	vmm_emupic_t *ep;
	struct vmm_devemu_guest_context *eg;

	if (!guest) {
		return VMM_EFAIL;
	}

	eg = (struct vmm_devemu_guest_context *)guest->aspace.devemu_priv;

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
	struct vmm_devemu_guest_context *eg;

	if (!guest || !pic) {
		return VMM_EFAIL;
	}

	eg = (struct vmm_devemu_guest_context *)guest->aspace.devemu_priv;
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
	struct vmm_devemu_guest_context *eg;

	if (!guest || !pic) {
		return VMM_EFAIL;
	}

	eg = (struct vmm_devemu_guest_context *)guest->aspace.devemu_priv;

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
	struct vmm_devemu_guest_context *eg;
	vmm_emupic_t *ep;

	if (!guest || !name) {
		return NULL;
	}

	eg = (struct vmm_devemu_guest_context *)guest->aspace.devemu_priv;
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
	struct vmm_devemu_guest_context *eg;
	vmm_emupic_t *retval;

	if (!guest) {
		return NULL;
	}
	if (index < 0) {
		return NULL;
	}

	eg = (struct vmm_devemu_guest_context *)guest->aspace.devemu_priv;
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
	struct vmm_devemu_guest_context *eg;
	struct dlist *l;

	if (!guest) {
		return 0;
	}

	eg = (struct vmm_devemu_guest_context *)guest->aspace.devemu_priv;

	list_for_each(l, &eg->emupic_list) {
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

int vmm_devemu_reset_context(vmm_guest_t *guest)
{
	u32 ite;
	struct dlist * l;
	struct vmm_devemu_vcpu_context *ev;
	vmm_vcpu_t * vcpu;

	if (!guest) {
		return VMM_EFAIL;
	}

	list_for_each(l, &guest->vcpu_list) {
		vcpu = list_entry(l, vmm_vcpu_t, head);
		if (vcpu->devemu_priv) {
			ev = vcpu->devemu_priv;
			ev->rd_victim = 0;
			ev->wr_victim = 0;
			for (ite = 0; 
			     ite < CONFIG_VGPA2REG_CACHE_SIZE; 
			     ite++) {
				ev->rd_gphys[ite] = 0;
				ev->rd_reg[ite] = NULL;
				ev->wr_gphys[ite] = 0;
				ev->wr_reg[ite] = NULL;
			}
		}
	}

	return VMM_OK;
}

int vmm_devemu_reset_region(vmm_guest_t *guest, vmm_region_t *reg)
{
	vmm_emudev_t *edev;

	if (!guest || !reg) {
		return VMM_EFAIL;
	}

	if (!(reg->flags & VMM_REGION_VIRTUAL)) {
		return VMM_EFAIL;
	}

	edev = (vmm_emudev_t *)reg->devemu_priv;
	if (!edev || !edev->reset) {
		return VMM_EFAIL;
	}

	return edev->reset(edev);
}

int vmm_devemu_probe_region(vmm_guest_t *guest, vmm_region_t *reg)
{
	int rc;
	struct dlist *l1;
	vmm_emudev_t *einst;
	struct vmm_devemu_guest_context *eginst;
	vmm_emulator_t *emu;
	const vmm_emuid_t *matches;
	const vmm_emuid_t *match;

	if (!guest || !reg) {
		return VMM_EFAIL;
	}

	if (!(reg->flags & VMM_REGION_VIRTUAL)) {
		return VMM_EFAIL;
	}

	eginst = guest->aspace.devemu_priv;

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
			reg->devemu_priv = einst;
			reg->node->type = VMM_DEVTREE_NODETYPE_EDEVICE;
			reg->node->priv = einst;
#if defined(CONFIG_VERBOSE_MODE)
			vmm_printf("Probe edevice %s/%s\n",
				   guest->node->name, reg->node->name);
#endif
			if ((rc = einst->probe(guest, einst, match))) {
				vmm_printf("%s: %s/%s probe error %d\n", 
				__func__, guest->node->name, reg->node->name, rc);
				vmm_free(einst);
				reg->devemu_priv = NULL;
				reg->node->type = VMM_DEVTREE_NODETYPE_UNKNOWN;
				reg->node->priv = NULL;
			}
			if ((rc = einst->reset(einst))) {
				vmm_printf("%s: %s/%s reset error %d\n", 
				__func__, guest->node->name, reg->node->name, rc);
				vmm_free(einst);
				reg->devemu_priv = NULL;
				reg->node->type = VMM_DEVTREE_NODETYPE_UNKNOWN;
				reg->node->priv = NULL;
			}
			break;
		}
	}

	return VMM_OK;
}

int vmm_devemu_init_context(vmm_guest_t *guest)
{
	u32 ite;
	struct dlist * l;
	struct vmm_devemu_vcpu_context *ev;
	struct vmm_devemu_guest_context *eg;
	vmm_vcpu_t * vcpu;

	if (!guest) {
		return VMM_EFAIL;
	}

	if (!guest->aspace.devemu_priv) {
		eg = vmm_malloc(sizeof(struct vmm_devemu_guest_context));
		INIT_LIST_HEAD(&eg->emupic_list);
		guest->aspace.devemu_priv = eg;
	}

	list_for_each(l, &guest->vcpu_list) {
		vcpu = list_entry(l, vmm_vcpu_t, head);
		if (!vcpu->devemu_priv) {
			ev = vmm_malloc(sizeof(struct vmm_devemu_vcpu_context));
			vmm_memset(ev, 0, sizeof(struct vmm_devemu_vcpu_context));
			ev->rd_victim = 0;
			ev->wr_victim = 0;
			for (ite = 0; 
			     ite < CONFIG_VGPA2REG_CACHE_SIZE; 
			     ite++) {
				ev->rd_gphys[ite] = 0;
				ev->rd_reg[ite] = NULL;
				ev->wr_gphys[ite] = 0;
				ev->wr_reg[ite] = NULL;
			}
			vcpu->devemu_priv = ev;
		}
	}

	return VMM_OK;
}

int __init vmm_devemu_init(void)
{
	vmm_memset(&dectrl, 0, sizeof(dectrl));

	INIT_LIST_HEAD(&dectrl.emu_list);

	return VMM_OK;
}
