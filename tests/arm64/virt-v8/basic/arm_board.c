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
 * @file arm_board.c
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief various platform specific functions
 */

#include <arm_types.h>
#include <arm_string.h>
#include <arm_board.h>
#include <arm_plat.h>
#include <pic/gic.h>
#include <timer/generic_timer.h>
#include <serial/virtio_console.h>

void arm_board_reset(void)
{
	/* Nothing to do */
}

void arm_board_init(void)
{
	/* Nothing to do */
}

char *arm_board_name(void)
{
	return "Virt-v8";
}

physical_addr_t arm_board_ram_start(void)
{
	return VIRT_V8_RAM0;
}

physical_size_t arm_board_ram_size(void)
{
	return VIRT_V8_RAM0_SIZE;
}

void arm_board_linux_default_cmdline(char *cmdline, u32 cmdline_sz)
{
	arm_strcpy(cmdline, "root=/dev/ram rw "
			    "earlyprintk=virtio-console,0x40600000 "
			    "console=hvc0");
}

physical_addr_t arm_board_flash_addr(void)
{
	return VIRT_V8_NOR_FLASH_0;
}

u32 arm_board_iosection_count(void)
{
	return 5;
}

physical_addr_t arm_board_iosection_addr(int num)
{
	physical_addr_t ret = 0;

	switch (num) {
	case 0:
		/* nor-flash */
		ret = VIRT_V8_NOR_FLASH_0;
		break;
	case 1:
		/* gic */
		ret = VIRT_V8_GIC;
		break;
	case 2:
		/* virtio-net */
		ret = VIRT_V8_VIRTIO_NET;
		break;
	case 3:
		/* virtio-blk */
		ret = VIRT_V8_VIRTIO_BLK;
		break;
	case 4:
		/* virtio-con */
		ret = VIRT_V8_VIRTIO_CON;
		break;
	default:
		while (1);
		break;
	}

	return ret;
}

u32 arm_board_pic_nr_irqs(void)
{
	return NR_IRQS_VIRT_V8;
}

int arm_board_pic_init(void)
{
	int rc;

	/*
	 * Initialize Generic Interrupt Controller
	 */
	rc = gic_dist_init(0, VIRT_V8_GIC_DIST, IRQ_VIRT_V8_GIC_START);
	if (rc) {
		return rc;
	}
	rc = gic_cpu_init(0, VIRT_V8_GIC_CPU);
	if (rc) {
		return rc;
	}

	return 0;
}

u32 arm_board_pic_active_irq(void)
{
	return gic_active_irq(0);
}

int arm_board_pic_ack_irq(u32 irq)
{
	return 0;
}

int arm_board_pic_eoi_irq(u32 irq)
{
	return gic_eoi_irq(0, irq);
}

int arm_board_pic_mask(u32 irq)
{
	return gic_mask(0, irq);
}

int arm_board_pic_unmask(u32 irq)
{
	return gic_unmask(0, irq);
}

void arm_board_timer_enable(void)
{
	return generic_timer_enable();
}

void arm_board_timer_disable(void)
{
	return generic_timer_disable();
}

u64 arm_board_timer_irqcount(void)
{
	return generic_timer_irqcount();
}

u64 arm_board_timer_irqdelay(void)
{
	return generic_timer_irqdelay();
}

u64 arm_board_timer_timestamp(void)
{
	return generic_timer_timestamp();
}

void arm_board_timer_change_period(u32 usecs)
{
	return generic_timer_change_period(usecs);
}

int arm_board_timer_init(u32 usecs)
{
	return generic_timer_init(usecs, IRQ_VIRT_V8_VIRT_TIMER);
}

int arm_board_serial_init(void)
{
	return virtio_console_init(VIRT_V8_VIRTIO_CON);
}

void arm_board_serial_putc(char ch)
{
	if (ch == '\n') {
		virtio_console_printch(VIRT_V8_VIRTIO_CON, '\r');
	}
	virtio_console_printch(VIRT_V8_VIRTIO_CON, ch);
}

char arm_board_serial_getc(void)
{
	char ch = virtio_console_getch(VIRT_V8_VIRTIO_CON);
	if (ch == '\r') {
		ch = '\n';
	}
	virtio_console_printch(VIRT_V8_VIRTIO_CON, ch);
	return ch;
}


