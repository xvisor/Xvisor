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
 * @file vmm_cmd_guest.c
 * @version 0.01
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of guest command
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_string.h>
#include <vmm_devtree.h>
#include <vmm_manager.h>
#include <vmm_mterm.h>
#include <vmm_host_aspace.h>
#include <vmm_guest_aspace.h>
#include <vmm_elf.h>

void cmd_guest_usage(void)
{
	vmm_printf("Usage:\n");
	vmm_printf("   guest help\n");
	vmm_printf("   guest list\n");
	vmm_printf("   guest load    <guest_num> <src_hphys_addr> <dest_gphys_addr> <img_sz>\n");
	vmm_printf("   guest reset   <guest_num>\n");
	vmm_printf("   guest kick    <guest_num>\n");
	vmm_printf("   guest pause   <guest_num>\n");
	vmm_printf("   guest resume  <guest_num>\n");
	vmm_printf("   guest halt    <guest_num>\n");
	vmm_printf("   guest dumpreg <guest_num>\n");
	vmm_printf("Note:\n");
	vmm_printf("   if guest_num is -1 then it means all guests\n");
}

void cmd_guest_list()
{
	int num, count;
	char path[256];
	vmm_guest_t *guest;
	vmm_printf("----------------------------------------"
		   "--------------------\n");
	vmm_printf("| %-5s| %-16s| %-32s|\n", "Num", "Name", "Device Path");
	vmm_printf("----------------------------------------"
		   "--------------------\n");
	count = vmm_manager_guest_count();
	for (num = 0; num < count; num++) {
		guest = vmm_manager_guest(num);
		vmm_devtree_getpath(path, guest->node);
		vmm_printf("| %-5d| %-16s| %-32s|\n", num, guest->node->name,
			   path);
	}
	vmm_printf("----------------------------------------"
		   "--------------------\n");
}

int cmd_guest_load(int num, physical_addr_t src_hphys_addr, physical_addr_t dest_gphys_addr, u32 img_sz)
{
	vmm_guest_t *guest;
	vmm_guest_region_t *guest_region;
	virtual_addr_t src_hvaddr, dest_gvaddr;

	guest = vmm_manager_guest(num);
	if (guest) {
		guest_region = vmm_guest_aspace_getregion(guest, dest_gphys_addr);
		if (!guest_region) {
			vmm_printf("Error: Cannot find a guest reqion containing address 0x%X\n", dest_gphys_addr);
			return VMM_EFAIL;
		}

		if (img_sz > guest_region->phys_size) {
			vmm_printf("(%s) Error: Image size is greater than the size of the requested guest region.\n",
				   __FUNCTION__);
			return VMM_EFAIL;
		}

		dest_gvaddr = vmm_host_iomap(guest_region->hphys_addr, guest_region->phys_size);
		if (!dest_gvaddr) {
			vmm_printf("(%s) Error: Cannot map host physical to host virtual.\n", __FUNCTION__);
			return VMM_EFAIL;
		}

		src_hvaddr = vmm_host_iomap(src_hphys_addr, guest_region->phys_size);
		if (!src_hvaddr) {
			vmm_printf("(%s) Error: Cannot map host source physical to host virtual.\n", __FUNCTION__);
			return VMM_EFAIL;
		}

		vmm_memcpy((void *)dest_gvaddr, (void *)src_hvaddr, img_sz);

		return VMM_OK;
	}

	return VMM_EFAIL;
}

int cmd_guest_reset(int num)
{
	int ret = VMM_EFAIL;
	vmm_guest_t *guest = vmm_manager_guest(num);
	if (guest) {
		if ((ret = vmm_manager_guest_reset(guest))) {
			vmm_printf("%s: Failed to reset\n", guest->node->name);
		} else {
			vmm_printf("%s: Reset done\n", guest->node->name);
		}
	} else {
		vmm_printf("Failed to find guest\n");
	}
	return ret;
}

int cmd_guest_kick(int num)
{
	int ret = VMM_EFAIL;
	vmm_guest_t *guest = vmm_manager_guest(num);
	if (guest) {
		if ((ret = vmm_manager_guest_kick(guest))) {
			vmm_printf("%s: Failed to kick\n", guest->node->name);
		} else {
			vmm_printf("%s: Kicked\n", guest->node->name);
		}
	} else {
		vmm_printf("Failed to find guest\n");
	}
	return ret;
}

int cmd_guest_pause(int num)
{
	int ret = VMM_EFAIL;
	vmm_guest_t *guest = vmm_manager_guest(num);
	if (guest) {
		;
		if ((ret = vmm_manager_guest_pause(guest))) {
			vmm_printf("%s: Failed to pause\n", guest->node->name);
		} else {
			vmm_printf("%s: Paused\n", guest->node->name);
		}
	} else {
		vmm_printf("Failed to find guest\n");
	}
	return ret;
}

int cmd_guest_resume(int num)
{
	int ret = VMM_EFAIL;
	vmm_guest_t *guest = vmm_manager_guest(num);
	if (guest) {
		if ((ret = vmm_manager_guest_resume(guest))) {
			vmm_printf("%s: Failed to resume\n", guest->node->name);
		} else {
			vmm_printf("%s: Resumed\n", guest->node->name);
		}
	} else {
		vmm_printf("Failed to find guest\n");
	}
	return ret;
}

int cmd_guest_halt(int num)
{
	int ret = VMM_EFAIL;
	vmm_guest_t *guest = vmm_manager_guest(num);
	if (guest) {
		if ((ret = vmm_manager_guest_halt(guest))) {
			vmm_printf("%s: Failed to halt\n", guest->node->name);
		} else {
			vmm_printf("%s: Halted\n", guest->node->name);
		}
	} else {
		vmm_printf("Failed to find guest\n");
	}
	return ret;
}

int cmd_guest_dumpreg(int num)
{
	int ret = VMM_EFAIL;
	vmm_guest_t *guest = vmm_manager_guest(num);
	if (guest) {
		if ((ret = vmm_manager_guest_dumpreg(guest))) {
			vmm_printf("%s: Failed to dumpreg\n", guest->node->name);
		}
	} else {
		vmm_printf("Failed to find guest\n");
	}
	return ret;
}

int cmd_guest_exec(int argc, char **argv)
{
	int num, count;
	u32 src_addr, dest_addr, size;
	int ret;
	if (argc == 2) {
		if (vmm_strcmp(argv[1], "help") == 0) {
			cmd_guest_usage();
			return VMM_OK;
		} else if (vmm_strcmp(argv[1], "list") == 0) {
			cmd_guest_list();
			return VMM_OK;
		}
	}
	if (argc < 3) {
		cmd_guest_usage();
		return VMM_EFAIL;
	}
	num = vmm_str2int(argv[2], 10);
	count = vmm_manager_guest_count();
	if (vmm_strcmp(argv[1], "reset") == 0) {
		if (num == -1) {
			for (num = 0; num < count; num++) {
				ret = cmd_guest_reset(num);
				if (ret) {
					return ret;
				}
			}
		} else {
			return cmd_guest_reset(num);
		}
	} else if (vmm_strcmp(argv[1], "kick") == 0) {
		if (num == -1) {
			for (num = 0; num < count; num++) {
				ret = cmd_guest_kick(num);
				if (ret) {
					return ret;
				}
			}
		} else {
			return cmd_guest_kick(num);
		}
	} else if (vmm_strcmp(argv[1], "pause") == 0) {
		if (num == -1) {
			for (num = 0; num < count; num++) {
				ret = cmd_guest_pause(num);
				if (ret) {
					return ret;
				}
			}
		} else {
			return cmd_guest_pause(num);
		}
	} else if (vmm_strcmp(argv[1], "resume") == 0) {
		if (num == -1) {
			for (num = 0; num < count; num++) {
				ret = cmd_guest_resume(num);
				if (ret) {
					return ret;
				}
			}
		} else {
			return cmd_guest_resume(num);
		}
	} else if (vmm_strcmp(argv[1], "halt") == 0) {
		if (num == -1) {
			for (num = 0; num < count; num++) {
				ret = cmd_guest_halt(num);
				if (ret) {
					return ret;
				}
			}
		} else {
			return cmd_guest_halt(num);
		}
	} else if (vmm_strcmp(argv[1], "dumpreg") == 0) {
		if (num == -1) {
			for (num = 0; num < count; num++) {
				ret = cmd_guest_dumpreg(num);
				if (ret) {
					return ret;
				}
			}
		} else {
			return cmd_guest_dumpreg(num);
		}
	} else if (vmm_strcmp(argv[1], "load") == 0) {
		if (num == -1) {
			vmm_printf("Error: Cannot load images in all guests simultaneously.\n");
			return VMM_EFAIL;
		}

		if (argc < 6) {
			vmm_printf("Error: Insufficient argument for command load.\n");
			cmd_guest_usage();
			return VMM_EFAIL;
		}

		src_addr = vmm_str2uint(argv[3], 16);
		dest_addr = vmm_str2uint(argv[4], 16);
		size = vmm_str2uint(argv[5], 16);

		return cmd_guest_load(num, src_addr, dest_addr, size);
	} else {
		cmd_guest_usage();
		return VMM_EFAIL;
	}
	return VMM_OK;
}

VMM_DECLARE_CMD(guest, "control commands for guest", cmd_guest_exec, NULL);
