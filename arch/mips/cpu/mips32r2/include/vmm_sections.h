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
 * @file vmm_sections.h
 * @version 0.01
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief Architecture specific implementation of synchronization mechanisms.
 */

#ifndef __VMM_SECTIONS_H__
#define __VMM_SECTIONS_H__

#include "vmm_types.h"

#define __lock_section		__attribute__((section(".spinlock.text")))
#define __modtbl_section	__attribute__((section(".modtbl")))

extern char _heap_start;
extern char _eheap;
extern char _modtbl_start;
extern char _modtbl_end;
extern char _mm_hk_start;
extern char _mm_hk_end;

static inline virtual_addr_t vmm_mm_hk_start(void)
{
        return (virtual_addr_t)&_mm_hk_start;
}

static inline virtual_size_t vmm_mm_hk_size(void)
{
        return (virtual_size_t)(&_mm_hk_end - &_mm_hk_start);
}

static inline virtual_addr_t vmm_modtbl_start(void) 
{
	return (virtual_addr_t)&_modtbl_start;
}

static inline virtual_size_t vmm_modtbl_size(void) 
{
	return (virtual_size_t)(&_modtbl_end - &_modtbl_start);
}

static inline virtual_addr_t vmm_heap_start(void)
{
	return (virtual_addr_t)&_heap_start;
}

static inline virtual_size_t vmm_heap_size(void)
{
	return (virtual_addr_t)(&_eheap - &_heap_start);
}

#endif /* __VMM_SECTIONS_H__ */
