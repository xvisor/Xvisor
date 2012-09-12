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
 * @file stringlib.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief implementation of string library
 */

#include <vmm_types.h>
#include <vmm_host_io.h>
#include <stringlib.h>

size_t strlen(const char *s)
{
	size_t ret = 0;
	while (s[ret]) {
		ret++;
	}
	return ret;
}

char *strcpy(char *dest, const char *src)
{
	u32 i;
	for (i = 0; src[i] != '\0'; ++i)
		dest[i] = src[i];
	dest[i] = '\0';
	return dest;
}

char *strncpy(char *dest, const char *src, size_t n)
{
	u32 i;
	for (i = 0; src[i] != '\0' && n; ++i, n--)
		dest[i] = src[i];
	dest[i] = '\0';
	return dest;
}

char *strcat(char *dest, const char *src)
{
	char *save = dest;

	for (; *dest; ++dest) ;
	while ((*dest++ = *src++) != 0) ;

	return (save);
}

int strcmp(const char *a, const char *b)
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

int strncmp(const char *a, const char *b, size_t n)
{
	if (n == 0)
		return 0;
	while (*a == *b && n > 0) {
		if (*a == '\0' || *b == '\0') {
			if (n) {
				return (unsigned char)*a - (unsigned char)*b;
			} else {
				return 0;
			}
		}
		++a;
		++b;
		--n;
	}
	if (n) {
		return (unsigned char)*a - (unsigned char)*b;
	} else {
		return 0;
	}
}

void str2lower(char * s)
{
	if (!s) {
		return;
	}

	while (*s) {
		if ('A' <= *s && *s <= 'Z') {
			*s = (*s - 'A') + 'a';
		}
		s++;
	}
}

void str2upper(char * s)
{
	if (!s) {
		return;
	}

	while (*s) {
		if ('a' <= *s && *s <= 'z') {
			*s = (*s - 'a') + 'A';
		}
		s++;
	}
}

long long str2longlong(const char *s, unsigned int base)
{
	long long val = 0;
	unsigned int digit;
	int neg = 0;

	if (base < 2 || base > 16)
		return 0;

	while (*s == ' ' || *s == '\t') {
		s++;
	}

	if (*s == '-') {
		neg = 1;
		s++;
	} else if (*s == '+') {
		s++;
	}

	if ((*s == '0') && (*(s+1) == 'x')) {
		base = 16;
		s++;
		s++;
	}

	while (*s) {
		if ('A' <= *s && *s <= 'F')
			digit = 10 + (*s - 'A');
		else if ('a' <= *s && *s <= 'f')
			digit = 10 + (*s - 'a');
		else if ('0' <= *s && *s <= '9')
			digit = *s - '0';
		else
			digit = 0;

		val = val * base + digit;

		s++;
	}

	if (neg) {
		return -val;
	}

	return val;
}

int str2int(const char *s, unsigned int base)
{
	return str2longlong(s, base);
}

unsigned long long str2ulonglong(const char *s, unsigned int base)
{
	unsigned long long val = 0;
	unsigned int digit;

	if (base < 2 || base > 16)
		return 0;

	while (*s == ' ' || *s == '\t') {
		s++;
	}

	if ((*s == '0') && (*(s+1) == 'x')) {
		base = 16;
		s++;
		s++;
	}

	while (*s) {
		if ('A' <= *s && *s <= 'F')
			digit = 10 + (*s - 'A');
		else if ('a' <= *s && *s <= 'f')
			digit = 10 + (*s - 'a');
		else if ('0' <= *s && *s <= '9')
			digit = *s - '0';
		else
			digit = 0;

		val = val * base + digit;

		s++;
	}

	return val;
}

unsigned int str2uint(const char *s, unsigned int base)
{
	return str2ulonglong(s, base);
}

int str2ipaddr(unsigned char *ipaddr, const char *str)
{
	unsigned char tmp;
	char c;
	unsigned char i, j;

	tmp = 0;

	for(i = 0; i < 4; ++i) {
		j = 0;
		do {
			c = *str;
			++j;
			if(j > 4) {
				return 0;
			}
			if(c == '.' || c == 0) {
				*ipaddr = tmp;
				++ipaddr;
				tmp = 0;
			} else if(c >= '0' && c <= '9') {
				tmp = (tmp * 10) + (c - '0');
			} else {
				return 0;
			}
			++str;
		} while(c != '.' && c != 0);
	}
	return 1;
}

void *memcpy(void *dest, const void *src, size_t count)
{
	u8 *dst8 = (u8 *) dest;
	u8 *src8 = (u8 *) src;

	if (count & 1) {
		dst8[0] = src8[0];
		dst8 += 1;
		src8 += 1;
	}

	count /= 2;
	while (count--) {
		dst8[0] = src8[0];
		dst8[1] = src8[1];

		dst8 += 2;
		src8 += 2;
	}

	return dest;
}

void *memcpy_toio(void *dest, const void *src, size_t count)
{
	u8 *dst8 = (u8 *) dest;
	u8 *src8 = (u8 *) src;

	if (count & 1) {
		vmm_writeb(src8[0], &dst8[0]);
		dst8 += 1;
		src8 += 1;
	}

	count /= 2;
	while (count--) {
		vmm_writeb(src8[0], &dst8[0]);
		vmm_writeb(src8[1], &dst8[1]);

		dst8 += 2;
		src8 += 2;
	}

	return dest;
}

void *memcpy_fromio(void *dest, const void *src, size_t count)
{
	u8 *dst8 = (u8 *) dest;
	u8 *src8 = (u8 *) src;

	if (count & 1) {
		dst8[0] = vmm_readb(&src8[0]);
		dst8 += 1;
		src8 += 1;
	}

	count /= 2;
	while (count--) {
		dst8[0] = vmm_readb(&src8[0]);
		dst8[1] = vmm_readb(&src8[1]);

		dst8 += 2;
		src8 += 2;
	}

	return dest;
}

void *memmove(void *dest, const void *src, size_t count)
{
	u8 *dst8 = (u8 *) dest;
	const u8 *src8 = (u8 *) src;

	if (src8 > dst8) {
		if (count & 1) {
			dst8[0] = src8[0];
			dst8 += 1;
			src8 += 1;
		}

		count /= 2;
		while (count--) {
			dst8[0] = src8[0];
			dst8[1] = src8[1];

			dst8 += 2;
			src8 += 2;
		}
	} else {
		dst8 += count;
		src8 += count;

		if (count & 1) {
			dst8 -= 1;
			src8 -= 1;
			dst8[0] = src8[0];
		}

		count /= 2;
		while (count--) {
			dst8 -= 2;
			src8 -= 2;

			dst8[0] = src8[0];
			dst8[1] = src8[1];
		}
	}

	return dest;
}

void *memset(void *dest, int c, size_t count)
{
	u8 *dst8 = (u8 *) dest;
	u8 ch = (u8) c;

	if (count & 1) {
		dst8[0] = ch;
		dst8 += 1;
	}

	count /= 2;
	while (count--) {
		dst8[0] = ch;
		dst8[1] = ch;
		dst8 += 2;
	}

	return dest;
}

void *memset_io(void *dest, int c, size_t count)
{
	u8 *dst8 = (u8 *) dest;
	u8 ch = (u8) c;

	if (count & 1) {
		vmm_writeb(ch, &dst8[0]);
		dst8 += 1;
	}

	count /= 2;
	while (count--) {
		vmm_writeb(ch, &dst8[0]);
		vmm_writeb(ch, &dst8[1]);
		dst8 += 2;
	}

	return dest;
}

int memcmp(const void *s1, const void *s2, size_t count)
{
	u8 *p1 = (u8 *) s1;
	u8 *p2 = (u8 *) s2;
	if (count != 0) {
		do {
			if (*p1++ != *p2++)
				return (*--p1 - *--p2);
		} while (--count != 0);
	}
	return (0);
}

void *memchr(const void *s, int c, size_t n)
{
	u32 i;

	for (i = 0; i < n; i++) {
		if (((const char *)s)[i] == (char)c) {
			return (void *)(s + i);
		}
	}

	return NULL;
}

