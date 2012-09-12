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
 * @file vmm_stdio.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source file for standerd input/output
 */

#include <arch_defterm.h>
#include <vmm_error.h>
#include <vmm_main.h>
#include <vmm_stdio.h>
#include <stdarg.h>
#include <stringlib.h>
#include <mathlib.h>

/* NOTE: assuming sizeof(void *) == sizeof(int) */

#define PAD_RIGHT	1
#define PAD_ZERO	2
/* the following should be enough for 32 bit int */
#define PRINT_BUF_LEN	64
/* size of early buffer. 
 * This should be enough to hold 80x25 characters
 */
#define EARLY_BUF_SZ	2048

struct vmm_stdio_ctrl {
        vmm_spinlock_t lock;
        struct vmm_chardev *dev;
};

static struct vmm_stdio_ctrl stdio_ctrl;
static bool stdio_init_done = FALSE;
static u32 stdio_early_count = 0;
static char stdio_early_buffer[EARLY_BUF_SZ];

bool vmm_iscontrol(char c)
{
	return ((0 <= c) && (c < 32)) ? TRUE : FALSE;
}

bool vmm_isprintable(char c)
{
	if (((31 < c) && (c < 127)) || 
	   (c == '\r') ||
	   (c == '\n') ||
	   (c == '\t')) {
		return TRUE;
	}
	return FALSE;
}

int vmm_printchars(struct vmm_chardev *cdev, char *ch, u32 num_ch, bool block)
{
	int i, rc;

	if (!ch || !num_ch) {
		return VMM_EFAIL;
	}

	if (stdio_init_done) {
		if (cdev) {
			rc = vmm_chardev_dowrite(cdev, (u8 *)ch, 0, num_ch, 
						 block) ? VMM_OK : VMM_EFAIL;
		} else {
			for (i = 0; i < num_ch; i++) {
				while ((rc = arch_defterm_putc((u8)ch[i])) && block);
			}
		}
	} else {
		for (i = 0; i < num_ch; i++) {
			if (stdio_early_count < EARLY_BUF_SZ) {
				stdio_early_buffer[stdio_early_count] = ch[i];
				stdio_early_count++;
			}
		}
		rc = VMM_OK;
	}

	return rc;
}

void vmm_cputc(struct vmm_chardev *cdev, char ch)
{
	if (ch == '\n') {
		vmm_printchars(cdev, "\r", 1, TRUE);
	}
	vmm_printchars(cdev, &ch, 1, TRUE);
}

void vmm_putc(char ch)
{
	vmm_cputc(stdio_ctrl.dev, ch);
}

void vmm_cputs(struct vmm_chardev *cdev, char *str)
{
	if (!str) {
		return;
	}
	while (*str) {
		vmm_cputc(cdev, *str);
		str++;
	}
}

void vmm_puts(char *str)
{
	vmm_cputs(stdio_ctrl.dev, str);
}

static void flush_early_buffer(void)
{
	int i;

	if (!stdio_init_done) {
		return;
	}

	for (i = 0; i < stdio_early_count; i++) {
		vmm_putc(stdio_early_buffer[i]);
	}
}

static void printc(char **str, struct vmm_chardev *cdev, char ch)
{
	if (str) {
		if (*str) {
			**str = ch;
			++(*str);
		}
	} else {
		vmm_cputc(cdev, ch);
	}
}

static int prints(char **out, struct vmm_chardev *cdev, 
		  const char *string, int width, int pad)
{
	int pc = 0;
	char padchar = ' ';

	if (width > 0) {
		int len = 0;
		const char *ptr;
		for (ptr = string; *ptr; ++ptr)
			++len;
		if (len >= width)
			width = 0;
		else
			width -= len;
		if (pad & PAD_ZERO)
			padchar = '0';
	}
	if (!(pad & PAD_RIGHT)) {
		for (; width > 0; --width) {
			printc(out, cdev, padchar);
			++pc;
		}
	}
	for (; *string; ++string) {
		printc(out, cdev, *string);
		++pc;
	}
	for (; width > 0; --width) {
		printc(out, cdev, padchar);
		++pc;
	}

	return pc;
}

static int printi(char **out, struct vmm_chardev *cdev, 
		  long long i, int b, int sg, int width, int pad, int letbase)
{
	char print_buf[PRINT_BUF_LEN];
	char *s;
	int t, neg = 0, pc = 0;
	unsigned long long u = i;

	if (i == 0) {
		print_buf[0] = '0';
		print_buf[1] = '\0';
		return prints(out, cdev, print_buf, width, pad);
	}

	if (sg && b == 10 && i < 0) {
		neg = 1;
		u = -i;
	}

	s = print_buf + PRINT_BUF_LEN - 1;
	*s = '\0';

	while (u) {
		t = umod64(u, b);
		if (t >= 10)
			t += letbase - '0' - 10;
		*--s = t + '0';
		u = udiv64(u, b);
	}

	if (neg) {
		if (width && (pad & PAD_ZERO)) {
			printc(out, cdev, '-');
			++pc;
			--width;
		} else {
			*--s = '-';
		}
	}

	return pc + prints(out, cdev, s, width, pad);
}

static int print(char **out, struct vmm_chardev *cdev, const char *format, va_list args)
{
	int width, pad;
	int pc = 0;
	char scr[2];
	long long tmp;

	for (; *format != 0; ++format) {
		if (*format == '%') {
			++format;
			width = pad = 0;
			if (*format == '\0')
				break;
			if (*format == '%')
				goto out;
			if (*format == '-') {
				++format;
				pad = PAD_RIGHT;
			}
			while (*format == '0') {
				++format;
				pad |= PAD_ZERO;
			}
			for (; *format >= '0' && *format <= '9'; ++format) {
				width *= 10;
				width += *format - '0';
			}
			if (*format == 's') {
				char *s = va_arg(args, char *);
				pc += prints(out, cdev, s ? s : "(null)", width, pad);
				continue;
			}
			if (*format == 'd') {
				pc +=
				    printi(out, cdev, va_arg(args, int), 
					   10, 1, width, pad, '0');
				continue;
			}
			if (*format == 'x') {
				pc +=
				    printi(out, cdev, va_arg(args, unsigned int), 
					   16, 0, width, pad, 'a');
				continue;
			}
			if (*format == 'X') {
				pc +=
				    printi(out, cdev, va_arg(args, unsigned int), 
					   16, 0, width, pad, 'A');
				continue;
			}
			if (*format == 'u') {
				pc +=
				    printi(out, cdev, va_arg(args, unsigned int), 
					   10, 0, width, pad, 'a');
				continue;
			}
			if (*format == 'l' && *(format + 1) == 'l') {
				if (sizeof(long long) == 
						sizeof(unsigned long)) {
					tmp = va_arg(args, unsigned long long);
				} else {
					((unsigned long *)&tmp)[0] = 
						va_arg(args, unsigned long);
					((unsigned long *)&tmp)[1] = 
						va_arg(args, unsigned long);
				}
				if (*(format + 2) == 'u') {
					format += 2;
					pc += printi(out, cdev, tmp,
						10, 0, width, pad, 'a');
				} else if (*(format + 2) == 'x') {
					format += 2;
					pc += printi(out, cdev, tmp,
						16, 0, width, pad, 'a');
				} else if (*(format + 2) == 'X') {
					format += 2;
					pc += printi(out, cdev, tmp,
						16, 0, width, pad, 'A');
				} else {
					format += 1;
					pc += printi(out, cdev, tmp,
						10, 1, width, pad, '0');
				}
				continue;
			}
			if (*format == 'c') {
				/* char are converted to int then pushed on the stack */
				scr[0] = va_arg(args, int);
				scr[1] = '\0';
				pc += prints(out, cdev, scr, width, pad);
				continue;
			}
		} else {
out:
			printc(out, cdev, *format);
			++pc;
		}
	}
	if (out)
		**out = '\0';
	return pc;
}

int vmm_printf(const char *format, ...)
{
	va_list args;
	int retval;
	va_start(args, format);
	retval = print(NULL, stdio_ctrl.dev, format, args);
	va_end(args);
	return retval;
}

int vmm_sprintf(char *out, const char *format, ...)
{
	va_list args;
	int retval;
	va_start(args, format);
	retval = print(&out, stdio_ctrl.dev, format, args);
	va_end(args);
	return retval;
}

/* FIXME: */
int vmm_snprintf(char *out, u32 out_sz, const char *format, ...)
{
	va_list args;
	int retval;
	va_start(args, format);
	retval = print(&out, stdio_ctrl.dev, format, args);
	va_end(args);
	return retval;
}

int vmm_cprintf(struct vmm_chardev *cdev, const char *format, ...)
{
	va_list args;
	int retval;
	va_start(args, format);
	retval = print(NULL, cdev, format, args);
	va_end(args);
	return retval;
}

void __noreturn vmm_panic(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	print(NULL, stdio_ctrl.dev, format, args);
	va_end(args);
	dump_stacktrace();
	vmm_hang();
}

int vmm_scanchars(struct vmm_chardev *cdev, char *ch, u32 num_ch, bool block)
{
	int i, rc;

	if (!ch || !num_ch) {
		return VMM_EFAIL;
	}

	if (stdio_init_done) {
		if (cdev) {
			rc = (vmm_chardev_doread(cdev, (u8 *)ch, 0, num_ch,
						 block) ? VMM_OK : VMM_EFAIL);
		} else {
			for (i = 0; i < num_ch; i++) {
				while ((rc = arch_defterm_getc((u8 *)&ch[i])) && block);
			}
		}
	} else {
		for (i = 0; i < num_ch; i++) {
			ch[i] = '\0';
		}
		rc = VMM_OK;
	}

	return rc;
}

char vmm_cgetc(struct vmm_chardev *cdev) 
{
	char ch = 0;
	vmm_scanchars(cdev, &ch, 1, TRUE);
	if (ch == '\r') {
		ch = '\n';
	}
	if (vmm_isprintable(ch)) {
		vmm_cputc(cdev, ch);
	}
	return ch;
}

char vmm_getc(void)
{
	return vmm_cgetc(stdio_ctrl.dev);
}

char *vmm_cgets(struct vmm_chardev *cdev, char *s, int maxwidth, char endchar)
{
	char ch, ch1;
	bool add_ch, del_ch, to_left, to_right, to_start, to_end;
	u32 ite, pos = 0, count = 0;
	if (!s) {
		return NULL;
	}
	memset(s, 0, maxwidth);
	while (count < maxwidth) {
		to_left = FALSE;
		to_right = FALSE;
		to_start = FALSE;
		to_end = FALSE;
		add_ch = FALSE;
		del_ch = FALSE;
		if ((ch = vmm_cgetc(cdev)) == endchar) {
			break;
		}
		/* Note: we have to process all the required 
		 * ANSI escape seqences for special keyboard keys */
		if (vmm_isprintable(ch)) {
			add_ch = TRUE;
		} else if (ch == '\e') { /* Escape character */
			vmm_scanchars(cdev, &ch, 1, TRUE);
			vmm_scanchars(cdev, &ch1, 1, TRUE);
			if (ch == '[') {
				if (ch1 == 'A') { /* Up Key */
					/* Ignore it. */ 
					/* We will take care of it later. */
				} else if (ch1 == 'B') { /* Down Key */
					/* Ignore it. */
					/* We will take care of it later. */
				} else if (ch1 == 'C') { /* Right Key */
					to_right = TRUE;
				} else if (ch1 == 'D') { /* Left Key */
					to_left = TRUE;
				} else if (ch1 == 'H') { /* Home Key */
					to_start = TRUE;
				} else if (ch1 == 'F') { /* End Key */
					to_end = TRUE;
				} else if (ch1 == '3') {
					vmm_scanchars(cdev, &ch, 1, TRUE);
					if (ch == '~') { /* Delete Key */
						if (pos < count) {
							to_right = TRUE;
							del_ch = TRUE;
						}
					}
				}
			} else if (ch == 'O') {
				if (ch1 == 'H') { /* Home Key */
					to_start = TRUE;
				} else if (ch1 == 'F') { /* End Key */
					to_end = TRUE;
				}
			}
		} else if (ch == 127){ /* Delete character */
			if (pos > 0) {
				del_ch = TRUE;
			}
		}
		if (to_left) {
			if (pos > 0) {
				vmm_cputs(cdev, "\e[D");
				pos--;
			}
		}
		if (to_right) {
			if (pos < count) {
				vmm_cputs(cdev, "\e[C");
				pos++;
			}
		}
		if (to_start) {
			if (pos > 0) {
				vmm_cprintf(cdev, "\e[%dD", pos);
			}
			pos = 0;
		}
		if (to_end) {
			if (pos < count) {
				vmm_cprintf(cdev, "\e[%dC", count - pos);
			}
			pos = count;
		}
		if (add_ch) {
			for (ite = 0; ite < (count - pos); ite++) {
				s[count - ite] = s[(count - 1) - ite];
			}
			for (ite = pos; ite < count; ite++) {
				vmm_cputc(cdev, s[ite + 1]);
			}
			for (ite = pos; ite < count; ite++) {
				vmm_cputs(cdev, "\e[D");
			}
			s[pos] = ch;
			count++;
			pos++;
		}
		if (del_ch) {
			if (pos > 0) {
				for (ite = pos; ite < count; ite++) {
					s[ite - 1] = s[ite];
				}
				s[count] = '\0';
				pos--;
				count--;
			}
			vmm_cputs(cdev, "\e[D");
			for (ite = pos; ite < count; ite++) {
				vmm_cputc(cdev, s[ite]);
			}
			vmm_cputc(cdev, ' ');
			for (ite = pos; ite <= count; ite++) {
				vmm_cputs(cdev, "\e[D");
			}
		}
	}
	s[count] = '\0';
	return s;
}

char *vmm_gets(char *s, int maxwidth, char endchar)
{
	return vmm_cgets(stdio_ctrl.dev, s, maxwidth, endchar);
}

struct vmm_chardev *vmm_stdio_device(void)
{
	return stdio_ctrl.dev;
}

int vmm_stdio_change_device(struct vmm_chardev * cdev)
{
	if (!cdev) {
		return VMM_EFAIL;
	}

	vmm_spin_lock(&stdio_ctrl.lock);
	stdio_ctrl.dev = cdev;
	vmm_spin_unlock(&stdio_ctrl.lock);

	return VMM_OK;
}

int __init vmm_stdio_init(void)
{
	int rc;

	/* Reset memory of control structure */
	memset(&stdio_ctrl, 0, sizeof(stdio_ctrl));

	/* Initialize lock */
	INIT_SPIN_LOCK(&stdio_ctrl.lock);

	/* Set current device to NULL */
	stdio_ctrl.dev = NULL;

	/* Initialize default serial terminal (board specific) */
	if ((rc = arch_defterm_init())) {
		return rc;
	}

	/* Update init done flag */
	stdio_init_done = TRUE;

	/* Flush early buffer */
	flush_early_buffer();

	return VMM_OK;
}
