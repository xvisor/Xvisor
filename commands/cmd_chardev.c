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
 * @file cmd_chardev.c
 * @version 0.01
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of chardev command
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_string.h>
#include <vmm_devtree.h>
#include <vmm_chardev.h>
#include <vmm_modules.h>
#include <vmm_cmdmgr.h>

#define MODULE_VARID			cmd_chardev_module
#define MODULE_NAME			"Command chardev"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_chardev_init
#define	MODULE_EXIT			cmd_chardev_exit

void cmd_chardev_usage(vmm_chardev_t *cdev)
{
	vmm_cprintf(cdev, "Usage:\n");
	vmm_cprintf(cdev, "   chardev help\n");
	vmm_cprintf(cdev, "   chardev list\n");
}

void cmd_chardev_list(vmm_chardev_t *cdev)
{
	int num, count;
	char path[1024];
	vmm_chardev_t *cd;
	count = vmm_chardev_count();
	for (num = 0; num < count; num++) {
		cd = vmm_chardev_get(num);
		if (!cd->dev) {
			vmm_cprintf(cdev, "%s: ---\n", cd->name);
		} else {
			vmm_devtree_getpath(path, cd->dev->node);
			vmm_cprintf(cdev, "%s: %s\n", cd->name, path);
		}
	}
}

int cmd_chardev_exec(vmm_chardev_t *cdev, int argc, char **argv)
{
	if (argc == 2) {
		if (vmm_strcmp(argv[1], "help") == 0) {
			cmd_chardev_usage(cdev);
			return VMM_OK;
		} else if (vmm_strcmp(argv[1], "list") == 0) {
			cmd_chardev_list(cdev);
			return VMM_OK;
		}
	}
	if (argc < 3) {
		cmd_chardev_usage(cdev);
		return VMM_EFAIL;
	}
	return VMM_OK;
}

static vmm_cmd_t cmd_chardev = {
	.name = "chardev",
	.desc = "character device commands",
	.usage = cmd_chardev_usage,
	.exec = cmd_chardev_exec,
};

static int cmd_chardev_init(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_chardev);
}

static void cmd_chardev_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_chardev);
}

VMM_DECLARE_MODULE(MODULE_VARID, 
			MODULE_NAME, 
			MODULE_AUTHOR, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
