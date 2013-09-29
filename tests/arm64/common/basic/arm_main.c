/**
 * Copyright (c) 2012 Jean-Christophe Dubois.
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
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief Basic firmware main file
 */

#include <arm_io.h>
#include <arm_heap.h>
#include <arm_mmu.h>
#include <arm_irq.h>
#include <arm_string.h>
#include <arm_stdio.h>
#include <arm_board.h>
#include <dhry.h>
#include <libfdt/libfdt.h>
#include <libfdt/fdt_support.h>

static unsigned long memory_size = 0x0;

unsigned long arm_linux_memory_size(void)
{
	return memory_size;
}

/* Works in supervisor mode */
void arm_init(void)
{
	arm_heap_init();

	arm_irq_disable();

	arm_irq_setup();

	arm_stdio_init();

	arm_board_timer_init(10000);

	memory_size = arm_board_ram_size();

	arm_board_timer_enable();

	arm_irq_enable();
}

void arm_cmd_help(int argc, char **argv)
{
	arm_puts("help        - List commands and their usage\n");
	arm_puts("\n");
	arm_puts("hi          - Say hi to ARM test code\n");
	arm_puts("\n");
	arm_puts("hello       - Say hello to ARM test code\n");
	arm_puts("\n");
	arm_puts("wfi_test    - Run wait for irq instruction test for ARM test code\n");
	arm_puts("            Usage: wfi_test [<msecs>]\n");
	arm_puts("            <msecs>  = delay in milliseconds to wait for\n");
	arm_puts("\n");
	arm_puts("mmu_setup   - Setup MMU for ARM test code\n");
	arm_puts("\n");
	arm_puts("mmu_state   - MMU is enabled/disabled for ARM test code\n");
	arm_puts("\n");
	arm_puts("mmu_test    - Run MMU test suite for ARM test code\n");
	arm_puts("\n");
	arm_puts("mmu_cleanup - Cleanup MMU for ARM test code\n");
	arm_puts("\n");
	arm_puts("timer       - Display timer information\n");
	arm_puts("\n");
	arm_puts("dhrystone   - Dhrystone 2.1 benchmark\n");
	arm_puts("            Usage: dhrystone [<iterations>]\n");
	arm_puts("\n");
	arm_puts("hexdump     - Dump memory contents in hex format\n");
	arm_puts("            Usage: hexdump <addr> <count>\n");
	arm_puts("            <addr>  = memory address in hex\n");
	arm_puts("            <count> = byte count in hex\n");
	arm_puts("\n");
	arm_puts("copy        - Copy to target memory from source memory\n");
	arm_puts("            Usage: copy <dest> <src> <count>\n");
	arm_puts("            <dest>  = destination address in hex\n");
	arm_puts("            <src>   = source address in hex\n");
	arm_puts("            <count> = byte count in hex\n");
	arm_puts("\n");
	arm_puts("start_linux - Start linux kernel\n");
	arm_puts("            Usage: start_linux <kernel_addr> <initrd_addr> <initrd_size>\n");
	arm_puts("            <kernel_addr>  = kernel load address\n");
	arm_puts("            <initrd_addr>  = initrd load address\n");
	arm_puts("            <initrd_size>  = initrd size\n");
	arm_puts("\n");
	arm_puts("linux_cmdline - Show/Update linux command line\n");
	arm_puts("            Usage: linux_cmdline <new_linux_cmdline> \n");
	arm_puts("            <new_linux_cmdline>  = linux command line\n");
	arm_puts("\n");
	arm_puts("linux_memory_size - Show/Update linux memory size\n");
	arm_puts("            Usage: linux_memory_size <memory_size> \n");
	arm_puts("            <memory_size>  = memory size in hex\n");
	arm_puts("\n");
	arm_puts("autoexec    - autoexec command list from flash\n");
	arm_puts("            Usage: autoexec\n");
	arm_puts("\n");
	arm_puts("go          - Jump to a given address\n");
	arm_puts("            Usage: go <addr>\n");
	arm_puts("            <addr>  = jump address in hex\n");
	arm_puts("\n");
	arm_puts("reset       - Reset the system\n");
	arm_puts("\n");
}

void arm_cmd_hi(int argc, char **argv)
{
	if (argc != 1) {
		arm_puts ("hi: no parameters required\n");
		return;
	}

	arm_puts("hello\n");
}

void arm_cmd_hello(int argc, char **argv)
{
	if (argc != 1) {
		arm_puts ("hello: no parameters required\n");
		return;
	}

	arm_puts("hi\n");
}

void arm_cmd_wfi_test(int argc, char **argv)
{
	u64 tstamp;
	char time[256];
	int delay = 1000;

	if (argc > 2) {
		arm_puts ("wfi_test: could provide only <delay>\n");
		return;
	} else if (argc == 2) {
		delay = arm_str2int(argv[1]);
	}

	arm_puts("Executing WFI instruction\n");
	arm_board_timer_disable();
	arm_board_timer_change_period(delay*1000);
	arm_board_timer_enable();
	tstamp = arm_board_timer_timestamp();
	arm_irq_wfi();
	tstamp = arm_board_timer_timestamp() - tstamp;
	arm_board_timer_disable();
	arm_board_timer_change_period(10000);
	arm_board_timer_enable();
	arm_puts("Resumed from WFI instruction\n");
	arm_puts("Time spent in WFI: ");
	arm_ulonglong2str(time, tstamp);
	arm_puts(time);
	arm_puts(" nsecs\n");
}

#if 0
void arm_cmd_mmu_setup(int argc, char **argv)
{
	if (argc != 1) {
		arm_puts ("mmu_setup: no parameters required\n");
		return;
	}

	arm_mmu_setup();
}
#endif

void arm_cmd_mmu_state(int argc, char **argv)
{
	if (argc != 1) {
		arm_puts ("mmu_state: no parameters required\n");
		return;
	}

	if (arm_mmu_is_enabled()) {
		arm_puts("MMU Enabled\n");
	} else {
		arm_puts("MMU Disabled\n");
	}
}

#if 0
void arm_cmd_mmu_test(int argc, char **argv)
{
	char str[32];
	u32 total = 0x0, pass = 0x0, fail = 0x0;

	if (argc != 1) {
		arm_puts ("mmu_test: no parameters required\n");
		return;
	}

	arm_puts("MMU Section Test Suite ...\n");
	total = 0x0;
	pass = 0x0;
	fail = 0x0;
	arm_mmu_section_test(&total, &pass, &fail);
	arm_puts("  Total: ");
	arm_int2str(str, total);
	arm_puts(str);
	arm_puts("\n");
	arm_puts("  Pass : ");
	arm_int2str(str, pass);
	arm_puts(str);
	arm_puts("\n");
	arm_puts("  Fail : ");
	arm_int2str(str, fail);
	arm_puts(str);
	arm_puts("\n");
	arm_puts("MMU Page Test Suite ...\n");
	total = 0x0;
	pass = 0x0;
	fail = 0x0;
	arm_mmu_page_test(&total, &pass, &fail);
	arm_puts("  Total: ");
	arm_int2str(str, total);
	arm_puts(str);
	arm_puts("\n");
	arm_puts("  Pass : ");
	arm_int2str(str, pass);
	arm_puts(str);
	arm_puts("\n");
	arm_puts("  Fail : ");
	arm_int2str(str, fail);
	arm_puts(str);
	arm_puts("\n");
}
#endif

void arm_cmd_mmu_cleanup(int argc, char **argv)
{
	if (argc != 1) {
		arm_puts ("mmu_cleanup: no parameters required\n");
		return;
	}

	arm_mmu_cleanup();
}

void arm_cmd_timer(int argc, char **argv)
{
	char str[32];
	u64 irq_count, irq_delay, tstamp;

	if (argc != 1) {
		arm_puts ("timer: no parameters required\n");
		return;
	}

	irq_count = arm_board_timer_irqcount();
	irq_delay = arm_board_timer_irqdelay();
	tstamp = arm_board_timer_timestamp();
	arm_puts("Timer Information ...\n");
	arm_puts("  IRQ Count:  0x");
	arm_ulonglong2hexstr(str, irq_count);
	arm_puts(str);
	arm_puts("\n");
	arm_puts("  IRQ Delay:  0x");
	arm_ulonglong2hexstr(str, irq_delay);
	arm_puts(str);
	arm_puts("\n");
	arm_puts("  Time Stamp: 0x");
	arm_ulonglong2hexstr(str, tstamp);
	arm_puts(str);
	arm_puts("\n");
}

void arm_cmd_dhrystone(int argc, char **argv)
{
	char str[32];
	int iters = 1000000;
	if (argc > 2) {
		arm_puts ("dhrystone: could provide only <iter_number>\n");
		return;
	} else if (argc == 2) {
		iters = arm_str2int(argv[1]);
	} else {
		arm_puts ("dhrystone: number of iterations not provided\n");
		arm_puts ("dhrystone: using default ");
		arm_int2str (str, iters);
		arm_puts (str);
		arm_puts (" iterations\n");
	}
	arm_board_timer_disable();
	dhry_main(iters);
	arm_board_timer_enable();
}

void arm_cmd_hexdump(int argc, char **argv)
{
	char str[32];
	u32 *addr;
	u32 i, count, len;
	if (argc != 3) {
		arm_puts ("hexdump: must provide <addr> and <count>\n");
		return;
	}
	addr = (u32 *)arm_hexstr2ulonglong(argv[1]);
	count = arm_hexstr2uint(argv[2]);
	for (i = 0; i < (count / 4); i++) {
		if (i % 4 == 0) {
			arm_ulonglong2hexstr(str, (u64)&addr[i]);
			len = arm_strlen(str);
			while (len < 8) {
				arm_puts("0");
				len++;
			}
			arm_puts(str);
			arm_puts(": ");
		}
		arm_uint2hexstr(str, addr[i]);
		len = arm_strlen(str);
		while (len < 8) {
			arm_puts("0");
			len++;
		}
		arm_puts(str);
		if (i % 4 == 3) {
			arm_puts("\n");
		} else {
			arm_puts(" ");
		}
	}
	arm_puts("\n");
}

void arm_cmd_copy(int argc, char **argv)
{
	u64 tstamp;
	char time[256];
	u64 *dest, *src;
	u32 i, count;
	if (argc != 4) {
		arm_puts ("copy: must provide <dest>, <src>, and <count>\n");
		return;
	}
	dest = (u64 *)arm_hexstr2ulonglong(argv[1]);
	src = (u64 *)arm_hexstr2ulonglong(argv[2]);
	count = arm_hexstr2uint(argv[3]);
	arm_board_timer_disable();
	tstamp = arm_board_timer_timestamp();
	for (i = 0; i < (count/sizeof(*dest)); i++) {
		dest[i] = src[i];
	}
	tstamp = arm_board_timer_timestamp() - tstamp;
	arm_board_timer_enable();
	arm_ulonglong2str(time, tstamp);
	arm_puts("copy took ");
	arm_puts(time);
	arm_puts(" ns for ");
	arm_puts(argv[3]);
	arm_puts(" bytes\n");
}

#define CONFIG_RAM_SIZE		(arm_linux_memory_size() >> 20)
#define CONFIG_NR_CPUS		2
char linux_cmdline[1024];

typedef void (* linux_entry_t) (u64 fdt_addr);

void dump_fdt(void *);

void arm_cmd_start_linux(int argc, char **argv)
{
	u64 kernel_addr, fdt_addr;
	u64 initrd_addr, initrd_size;
	int err;
	char cfg_str[10];
	u64 meminfo[2];

	if (argc != 5) {
		arm_puts ("start_linux: must provide <kernel_addr>, <initrd_addr>, <initrd_size> and <fdt_addr>\n");
		return;
	}

	/* Parse the arguments from command line */
	kernel_addr = arm_hexstr2ulonglong(argv[1]);
	initrd_addr = arm_hexstr2ulonglong(argv[2]);
	initrd_size = arm_hexstr2ulonglong(argv[3]);
	fdt_addr = arm_hexstr2ulonglong(argv[4]);

	meminfo[0] = arm_board_ram_start();
	meminfo[1] = arm_board_ram_size();
	/* Fillup/fixup the fdt blob with following:
	 * 		- initrd start, end
	 * 		- kernel cmd line
	 * 		- number of cpus   */
	if ((err = fdt_fixup_memory_banks((void *)fdt_addr, (&meminfo[0]), 
							(&meminfo[1]), 1))) {
		arm_printf("%s: fdt_fixup_memory_banks() failed: %s\n", __func__, 
				fdt_strerror(err));
		return;
	}
	sprintf(cfg_str, " mem=%dM maxcpus=%d", 
				(int)(meminfo[1] >> 20), CONFIG_NR_CPUS);
	arm_strcat(linux_cmdline, cfg_str);
	if ((err = fdt_chosen((void *)fdt_addr, 1))) {
		arm_printf("%s: fdt_chosen() failed: %s\n", __func__, 
				fdt_strerror(err));
		return;
	}
	if ((err = fdt_initrd((void *)fdt_addr, initrd_addr, 
					initrd_addr + initrd_size, 1))) {
		arm_printf("%s: fdt_initrd() failed: %s\n", __func__, 
				fdt_strerror(err));
		return;
	}

	/* Disable interrupts and timer */
	arm_board_timer_disable();
	arm_irq_disable();
	arm_mmu_cleanup();

	/* Jump to Linux Kernel
	 * r0 -> dtb address
	 */
	arm_puts("Jumping into linux ...\n");
	((linux_entry_t)kernel_addr)(fdt_addr);

	/* We should never reach here */
	while (1);

	return;
}

void arm_cmd_linux_cmdline(int argc, char **argv)
{
	if (argc >= 2) {
		int cnt = 1;
		linux_cmdline[0] = 0;

		while (cnt < argc) {
			arm_strcat(linux_cmdline, argv[cnt]);
			arm_strcat(linux_cmdline, " ");
			cnt++;
		}
	}

	arm_puts ("linux_cmdline = \"");
	arm_puts(linux_cmdline);
	arm_puts ("\"\n");

	return;
}

void arm_cmd_linux_memory_size(int argc, char **argv)
{
	char str[32];

	if (argc == 2) {
		memory_size = (u32)arm_hexstr2uint(argv[1]);
	}

	arm_puts ("linux_memory_size = 0x");
	arm_uint2hexstr(str, memory_size);
	arm_puts(str);
	arm_puts (" Bytes\n");

	return;
}

void arm_exec(char *line);

void arm_cmd_autoexec(int argc, char **argv)
{
	static int lock = 0;
	int len;
	/* commands to execute are stored in NOR flash */
	char *ptr = (char *)(arm_board_flash_addr() + 0xFF000);
	char buffer[4096];

	if (argc != 1) {
		arm_puts ("autoexec: no parameters required\n");
		return;
	}

	/* autoexec is not recursive */
	if (lock) {
		arm_puts("ignoring autoexec calling autoexec\n");
		return;
	}

	lock = 1;

	if ((len = arm_strlen(ptr))) {
		int pos = 0;

		/* copy commands from NOR flash */
		arm_strcpy(buffer, ptr);

		/* now we process them */
		while (pos < len) {
			ptr = &buffer[pos];

			/* We need to separate the commands */
			while ((buffer[pos] != '\r') &&
				(buffer[pos] != '\n') &&
				(buffer[pos] != 0)) {
				pos++;
			}
			buffer[pos] = '\0';
			pos++;

			/* print the command */
			arm_puts("autoexec(");
			arm_puts(ptr);
			arm_puts(")\n");
			/* execute it */
			arm_exec(ptr);
		}

	}

	lock = 0;

	return;
}

void arm_cmd_go(int argc, char **argv)
{
	char str[32];
	void (* jump)(void);

	if (argc != 2) {
		arm_puts ("go: must provide destination address\n");
		return;
	}

	arm_board_timer_disable();

	jump = (void (*)(void))arm_hexstr2ulonglong(argv[1]);
	arm_ulonglong2hexstr(str, (u64)jump);
	arm_puts("Jumping to location 0x");
	arm_puts(str);
	arm_puts(" ...\n");
	jump ();

	arm_board_timer_enable();
}

void arm_cmd_reset(int argc, char **argv)
{
	if (argc != 1) {
		arm_puts ("reset: no parameters required\n");
		return;
	}

	arm_puts("System reset ...\n\n");

	arm_board_reset();

	while (1);
}

#define ARM_MAX_ARG_SIZE	32

void arm_exec(char *line)
{
	int argc = 0, pos = 0, cnt = 0;
	char *argv[ARM_MAX_ARG_SIZE];

	while (line[pos] && (argc < ARM_MAX_ARG_SIZE)) {
		if ((line[pos] == '\r') ||
		    (line[pos] == '\n')) {
			line[pos] = '\0';
			break;
		}
		if (line[pos] == ' ') {
			if (cnt > 0) {
				line[pos] = '\0';
				cnt = 0;
			}
		} else {
			if (cnt == 0) {
				argv[argc] = &line[pos];
				argc++;
			}
			cnt++;
		}
		pos++;
	}

	if (argc) {
		if (arm_strcmp(argv[0], "help") == 0) {
			arm_cmd_help(argc, argv);
		} else if (arm_strcmp(argv[0], "hi") == 0) {
			arm_cmd_hi(argc, argv);
		} else if (arm_strcmp(argv[0], "hello") == 0) {
			arm_cmd_hello(argc, argv);
		} else if (arm_strcmp(argv[0], "wfi_test") == 0) {
			arm_cmd_wfi_test(argc, argv);
#if 0
		} else if (arm_strcmp(argv[0], "mmu_setup") == 0) {
			arm_cmd_mmu_setup(argc, argv);
#endif
		} else if (arm_strcmp(argv[0], "mmu_state") == 0) {
			arm_cmd_mmu_state(argc, argv);
#if 0
		} else if (arm_strcmp(argv[0], "mmu_test") == 0) {
			arm_cmd_mmu_test(argc, argv);
#endif
		} else if (arm_strcmp(argv[0], "mmu_cleanup") == 0) {
			arm_cmd_mmu_cleanup(argc, argv);
		} else if (arm_strcmp(argv[0], "timer") == 0) {
			arm_cmd_timer(argc, argv);
		} else if (arm_strcmp(argv[0], "dhrystone") == 0) {
			arm_cmd_dhrystone(argc, argv);
		} else if (arm_strcmp(argv[0], "hexdump") == 0) {
			arm_cmd_hexdump(argc, argv);
		} else if (arm_strcmp(argv[0], "copy") == 0) {
			arm_cmd_copy(argc, argv);
		} else if (arm_strcmp(argv[0], "start_linux") == 0) {
			arm_cmd_start_linux(argc, argv);
		} else if (arm_strcmp(argv[0], "linux_cmdline") == 0) {
			arm_cmd_linux_cmdline(argc, argv);
		} else if (arm_strcmp(argv[0], "linux_memory_size") == 0) {
			arm_cmd_linux_memory_size(argc, argv);
		} else if (arm_strcmp(argv[0], "autoexec") == 0) {
			arm_cmd_autoexec(argc, argv);
		} else if (arm_strcmp(argv[0], "go") == 0) {
			arm_cmd_go(argc, argv);
		} else if (arm_strcmp(argv[0], "reset") == 0) {
			arm_cmd_reset(argc, argv);
		} else {
			arm_puts("Unknown command\n");
		}
	}
}

#define ARM_MAX_CMD_STR_SIZE	256

/* Works in user mode */
void arm_main(void)
{
	char line[ARM_MAX_CMD_STR_SIZE];

	/* Setup board specific linux default cmdline */
	arm_board_linux_default_cmdline(linux_cmdline, 
					sizeof(linux_cmdline));

	arm_puts(arm_board_name());
	arm_puts(" Basic Firmware\n\n");

	arm_board_init();

	while(1) {
		arm_puts("basic# ");

		arm_gets(line, ARM_MAX_CMD_STR_SIZE, '\n');

		arm_exec(line);
	}
}
