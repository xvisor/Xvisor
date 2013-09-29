/**
 * Copyright (c) 2013 Sukanto Ghosh.
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
 * @file brd_main.c
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief main source file for board specific code
 */

#include <vmm_error.h>
#include <vmm_smp.h>
#include <vmm_spinlocks.h>
#include <vmm_devtree.h>
#include <vmm_devdrv.h>
#include <vmm_host_io.h>
#include <vmm_host_aspace.h>
#include <vmm_stdio.h>
#include <arch_board.h>
#include <arch_timer.h>
#include <generic_timer.h>

/*
 * Reset & Shutdown
 */

int arch_board_reset(void)
{
	/* FIXME: To be implemented. */
	vmm_printf("%s: not implemented\n", __func__);
	return VMM_EFAIL;
}

/*
 * Print board information
 */

void arch_board_print_info(struct vmm_chardev *cdev)
{
	/* FIXME: To be implemented. */
}

int arch_board_shutdown(void)
{
	/* FIXME: To be implemented. */
	vmm_printf("%s: not implemented\n", __func__);
	return VMM_EFAIL;
}

/*
 * Initialization functions
 */

int __init arch_board_early_init(void)
{
	/* Host aspace, Heap, Device tree, and Host IRQ available.
	 *
	 * Do necessary early stuff like:
	 * iomapping devices, 
	 * SOC clocking init, 
	 * Setting-up system data in device tree nodes,
	 * ....
	 */

	return VMM_OK;
}

int __init arch_clocksource_init(void)
{
	int rc;

	/* Initialize generic timer as clock source */
	rc = generic_timer_clocksource_init();
	if (rc) {
		vmm_printf("%s: generic clocksource init failed (error %d)\n",
			   __func__, rc);
	}

	return VMM_OK;
}

int __cpuinit arch_clockchip_init(void)
{
	int rc;

	/* Initialize generic timer as clock source */
	rc = generic_timer_clockchip_init();
	if (rc) {
		vmm_printf("%s: generic clockchip init failed (error %d)\n", 
			   __func__, rc);
	}

	return VMM_OK;
}

int __init arch_board_final_init(void)
{
	int rc;
	struct vmm_devtree_node *node;

	/* All VMM API's are available here */
	/* We can register a Board specific resource here */

	/* Find simple-bus node */
	node = vmm_devtree_find_compatible(NULL, NULL, "simple-bus");
	if (!node) {
		return VMM_ENODEV;
	}

	/* Do probing using device driver framework */
	rc = vmm_devdrv_probe(node);
	if (rc) {
		return rc;
	}

	return VMM_OK;
}
