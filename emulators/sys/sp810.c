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
 * @file sp810.c
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief PrimeCell SP810 System Controller Emulator.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_string.h>
#include <vmm_modules.h>
#include <vmm_devemu.h>

#define MODULE_VARID			sp810_emulator_module
#define MODULE_NAME			"SP810 Serial Emulator"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			sp810_emulator_init
#define	MODULE_EXIT			sp810_emulator_exit

struct sp810_state {
	vmm_guest_t *guest;
	vmm_spinlock_t lock;
	u32 sysctrl;
};

static int sp810_emulator_read(vmm_emudev_t *edev,
			       physical_addr_t offset, 
			       void *dst, u32 dst_len)
{
	int rc = VMM_OK;
	u32 regval = 0x0;
	struct sp810_state * s = edev->priv;

	vmm_spin_lock(&s->lock);

	switch (offset & ~0x3) {
	case 0x00: /* SYSCTRL */
		regval = s->sysctrl;
		break;
	default:
		rc = VMM_EFAIL;
		break;
	}

	vmm_spin_unlock(&s->lock);

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

static int sp810_emulator_write(vmm_emudev_t *edev,
				physical_addr_t offset, 
				void *src, u32 src_len)
{
	int rc = VMM_OK, i;
	u32 regmask = 0x0, regval = 0x0;
	struct sp810_state * s = edev->priv;

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

	vmm_spin_lock(&s->lock);

	switch (offset & ~0x3) {
	case 0x00: /* SYSCTRL */
		s->sysctrl &= regmask;
		s->sysctrl |= regval;
		break;
	default:
		rc = VMM_EFAIL;
		break;
	}

	vmm_spin_unlock(&s->lock);

	return rc;
}

static int sp810_emulator_reset(vmm_emudev_t *edev)
{
	struct sp810_state * s = edev->priv;

	vmm_spin_lock(&s->lock);

	s->sysctrl = 0;

	vmm_spin_unlock(&s->lock);

	return VMM_OK;
}

static int sp810_emulator_probe(vmm_guest_t *guest,
				vmm_emudev_t *edev,
				const vmm_emuid_t *eid)
{
	int rc = VMM_OK;
	struct sp810_state * s;

	s = vmm_malloc(sizeof(struct sp810_state));
	if (!s) {
		rc = VMM_EFAIL;
		goto sp810_emulator_probe_done;
	}
	vmm_memset(s, 0x0, sizeof(struct sp810_state));

	s->guest = guest;
	INIT_SPIN_LOCK(&s->lock);

	edev->priv = s;

sp810_emulator_probe_done:
	return rc;
}

static int sp810_emulator_remove(vmm_emudev_t *edev)
{
	struct sp810_state * s = edev->priv;

	vmm_free(s);

	return VMM_OK;
}

static vmm_emuid_t sp810_emuid_table[] = {
	{ .type = "sys", 
	  .compatible = "primecell,sp810", 
	},
	{ /* end of list */ },
};

static vmm_emulator_t sp810_emulator = {
	.name = "sp810",
	.match_table = sp810_emuid_table,
	.probe = sp810_emulator_probe,
	.read = sp810_emulator_read,
	.write = sp810_emulator_write,
	.reset = sp810_emulator_reset,
	.remove = sp810_emulator_remove,
};

static int __init sp810_emulator_init(void)
{
	return vmm_devemu_register_emulator(&sp810_emulator);
}

static void sp810_emulator_exit(void)
{
	vmm_devemu_unregister_emulator(&sp810_emulator);
}

VMM_DECLARE_MODULE(MODULE_VARID, 
			MODULE_NAME, 
			MODULE_AUTHOR, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
