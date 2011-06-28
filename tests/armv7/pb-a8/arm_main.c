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
 * @file arm_main.c
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief ARM test code main file
 */

#include <arm_interrupts.h>
#include <arm_pl01x.h>

#define	PBA8_UART_BASE			0x10009000
#define	PBA8_UART_TYPE			PL01X_TYPE_1
#define	PBA8_UART_INCLK			24000000
#define	PBA8_UART_BAUD			115200

int arm_strcmp(const char *a, const char *b)
{
	while (*a == *b) {
		if (*a == '\0' || *b == '\0') {
			return (unsigned char)*a - (unsigned char)*b;
		}
		++a;
		++b;
	}
	return (unsigned char)*a - (unsigned char)*b;
}

/* Works in supervisor mode */
void arm_init(void)
{
	arm_irq_setup();

	arm_irq_enable();

	arm_pl01x_init(PBA8_UART_BASE, 
			PBA8_UART_TYPE, 
			PBA8_UART_BAUD, 
			PBA8_UART_INCLK);
}

/* Works in user mode */
void arm_main(void)
{
	char line[256];
	arm_pl01x_puts(PBA8_UART_BASE,
			PBA8_UART_TYPE,
			"ARM Realview PB-A8 Test Code\n\n");
	while(1) {
		arm_pl01x_puts(PBA8_UART_BASE,
				PBA8_UART_TYPE,
				"arm-test# ");

		arm_pl01x_gets(PBA8_UART_BASE,
				PBA8_UART_TYPE,
				line, 256, '\n');

		if (arm_strcmp(line, "hi") == 0) {
			arm_pl01x_puts(PBA8_UART_BASE,
					PBA8_UART_TYPE,
					"hello\n");
		} else if (arm_strcmp(line, "hello") == 0) {
			arm_pl01x_puts(PBA8_UART_BASE,
					PBA8_UART_TYPE,
					"hi\n");
		}
	}
}
