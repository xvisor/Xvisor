/**
 * Copyright (c) 2012 Anup Patel.
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
 * @file cmd_host.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of host command
 */

#include <vmm_math.h>
#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_stdio.h>
#include <vmm_host_irq.h>
#include <vmm_host_ram.h>
#include <vmm_host_vapool.h>
#include <vmm_host_aspace.h>
#include <vmm_modules.h>
#include <vmm_cmdmgr.h>

#define MODULE_VARID			cmd_host_module
#define MODULE_NAME			"Command host"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_host_init
#define	MODULE_EXIT			cmd_host_exit

void cmd_host_usage(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "Usage:\n");
	vmm_cprintf(cdev, "   host help\n");
	vmm_cprintf(cdev, "   host info\n");
	vmm_cprintf(cdev, "   host irq stats\n");
	vmm_cprintf(cdev, "   host ram stats\n");
	vmm_cprintf(cdev, "   host ram bitmap [<column count>]\n");
	vmm_cprintf(cdev, "   host vapool stats\n");
	vmm_cprintf(cdev, "   host vapool bitmap [<column count>]\n");
}

void cmd_host_info(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "CPU   : %s\n", CONFIG_CPU);
	vmm_cprintf(cdev, "Board : %s\n", CONFIG_BOARD);
}

void cmd_host_irq_stats(struct vmm_chardev *cdev)
{
	u32 num, stats, count = vmm_host_irq_count();
	struct vmm_host_irq *irq;
	struct vmm_host_irq_chip *chip;
	vmm_cprintf(cdev, "----------------------------------------\n");
	vmm_cprintf(cdev, "| %-8s| %-10s| %-15s|\n", 
			  "IRQ Num", "IRQ Chip", "IRQ Count");
	vmm_cprintf(cdev, "----------------------------------------\n");
	for (num = 0; num < count; num++) {
		irq = vmm_host_irq_get(num);
		if (!vmm_host_irq_isenabled(irq)) {
			continue;
		}
		stats = vmm_host_irq_get_count(irq);
		chip = vmm_host_irq_get_chip(irq);
		vmm_cprintf(cdev, "| %-8d| %-10s| %-15d|\n", 
				  num, chip->name, stats);
	}
	vmm_cprintf(cdev, "----------------------------------------\n");
}

void cmd_host_ram_stats(struct vmm_chardev *cdev)
{
	u32 free = vmm_host_ram_free_frame_count();
	u32 total = vmm_host_ram_total_frame_count();
	physical_addr_t base = vmm_host_ram_base();
	if (sizeof(u64) == sizeof(physical_addr_t)) {
		vmm_cprintf(cdev, "Base Address : 0x%016llx\n", base);
	} else {
		vmm_cprintf(cdev, "Base Address : 0x%08x\n", base);
	}
	vmm_cprintf(cdev, "Frame Size   : %d (0x%08x)\n", 
					VMM_PAGE_SIZE, VMM_PAGE_SIZE);
	vmm_cprintf(cdev, "Free Frames  : %d (0x%08x)\n", free, free);
	vmm_cprintf(cdev, "Total Frames : %d (0x%08x)\n", total, total);
}

void cmd_host_ram_bitmap(struct vmm_chardev *cdev, int colcnt)
{
	u32 ite, total = vmm_host_ram_total_frame_count();
	physical_addr_t base = vmm_host_ram_base();
	vmm_cprintf(cdev, "0 : free\n");
	vmm_cprintf(cdev, "1 : used");
	for (ite = 0; ite < total; ite++) {
		if (vmm_umod32(ite, colcnt) == 0) {
			if (sizeof(u64) == sizeof(physical_addr_t)) {
				vmm_cprintf(cdev, "\n0x%016llx: ", 
						base + ite * VMM_PAGE_SIZE);
			} else {
				vmm_cprintf(cdev, "\n0x%08x: ", 
						base + ite * VMM_PAGE_SIZE);
			}
		}
		if (vmm_host_ram_frame_isfree(base + ite * VMM_PAGE_SIZE)) {
			vmm_cprintf(cdev, "0");
		} else {
			vmm_cprintf(cdev, "1");
		}
	}
	vmm_cprintf(cdev, "\n");
}

void cmd_host_vapool_stats(struct vmm_chardev *cdev)
{
	u32 free = vmm_host_vapool_free_page_count();
	u32 total = vmm_host_vapool_total_page_count();
	virtual_addr_t base = vmm_host_vapool_base();
	if (sizeof(u64) == sizeof(virtual_addr_t)) {
		vmm_cprintf(cdev, "Base Address : 0x%016llx\n", base);
	} else {
		vmm_cprintf(cdev, "Base Address : 0x%08x\n", base);
	}
	vmm_cprintf(cdev, "Page Size    : %d (0x%08x)\n", 
					VMM_PAGE_SIZE, VMM_PAGE_SIZE);
	vmm_cprintf(cdev, "Free Pages   : %d (0x%08x)\n", free, free);
	vmm_cprintf(cdev, "Total Pages  : %d (0x%08x)\n", total, total);
}

void cmd_host_vapool_bitmap(struct vmm_chardev *cdev, int colcnt)
{
	u32 ite, total = vmm_host_vapool_total_page_count();
	virtual_addr_t base = vmm_host_vapool_base();
	vmm_cprintf(cdev, "0 : free\n");
	vmm_cprintf(cdev, "1 : used");
	for (ite = 0; ite < total; ite++) {
		if (vmm_umod32(ite, colcnt) == 0) {
			if (sizeof(u64) == sizeof(virtual_addr_t)) {
				vmm_cprintf(cdev, "\n0x%016llx: ", 
						base + ite * VMM_PAGE_SIZE);
			} else {
				vmm_cprintf(cdev, "\n0x%08x: ", 
						base + ite * VMM_PAGE_SIZE);
			}
		}
		if (vmm_host_vapool_page_isfree(base + ite * VMM_PAGE_SIZE)) {
			vmm_cprintf(cdev, "0");
		} else {
			vmm_cprintf(cdev, "1");
		}
	}
	vmm_cprintf(cdev, "\n");
}

int cmd_host_exec(struct vmm_chardev *cdev, int argc, char **argv)
{
	int colcnt;
	if (1 < argc) {
		if (vmm_strcmp(argv[1], "help") == 0) {
			cmd_host_usage(cdev);
			return VMM_OK;
		} else if (vmm_strcmp(argv[1], "info") == 0) {
			cmd_host_info(cdev);
			return VMM_OK;
		} else if ((vmm_strcmp(argv[1], "irq") == 0) && (2 < argc)) {
			if (vmm_strcmp(argv[2], "stats") == 0) {
				cmd_host_irq_stats(cdev);
				return VMM_OK;
			}
		} else if ((vmm_strcmp(argv[1], "ram") == 0) && (2 < argc)) {
			if (vmm_strcmp(argv[2], "stats") == 0) {
				cmd_host_ram_stats(cdev);
				return VMM_OK;
			} else if (vmm_strcmp(argv[2], "bitmap") == 0) {
				if (3 < argc) {
					colcnt = vmm_str2int(argv[3], 10);
				} else {
					colcnt = 64;
				}
				cmd_host_ram_bitmap(cdev, colcnt);
				return VMM_OK;
			}
		} else if ((vmm_strcmp(argv[1], "vapool") == 0) && (2 < argc)) {
			if (vmm_strcmp(argv[2], "stats") == 0) {
				cmd_host_vapool_stats(cdev);
				return VMM_OK;
			} else if (vmm_strcmp(argv[2], "bitmap") == 0) {
				if (3 < argc) {
					colcnt = vmm_str2int(argv[3], 10);
				} else {
					colcnt = 64;
				}
				cmd_host_vapool_bitmap(cdev, colcnt);
				return VMM_OK;
			}
		}
	}
	cmd_host_usage(cdev);
	return VMM_EFAIL;
}

static struct vmm_cmd cmd_host = {
	.name = "host",
	.desc = "host information",
	.usage = cmd_host_usage,
	.exec = cmd_host_exec,
};

static int __init cmd_host_init(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_host);
}

static void cmd_host_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_host);
}

VMM_DECLARE_MODULE(MODULE_VARID, 
			MODULE_NAME, 
			MODULE_AUTHOR, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
