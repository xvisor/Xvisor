/**
 * Copyright (c) 2010 Himanshu Chauhan.
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
 * @file vmm_types.h
 * @version 0.01
 * @author Himanshu Chauhan (hchauhan@nulltrace.org)
 * @brief header file for common types used in xvisor.
 */

#ifndef __VMM_TYPES_H__
#define __VMM_TYPES_H__

#include <vmm_cpu_types.h>

typedef char s8;
typedef short s16;
typedef int s32;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned int size_t;
typedef unsigned int bool;
typedef unsigned int ulong;

/** Boolean macros */
#define TRUE		1
#define FALSE		0
#define NULL 		((void *)0)

#define stringify(s)	tostring(s)
#define tostring(s)	#s

#define BUG_ON(x, bug_string, ...)				\
	do {							\
		if (x) {					\
			vmm_panic(bug_string, #__VA_ARGS__);	\
		}						\
	} while(0);

#define __alwaysinline __attribute__((always_inline))
#define __unused __attribute__((unused))
#define __noreturn __attribute__((noreturn))

#endif /* __VMM_TYPES_H__ */
