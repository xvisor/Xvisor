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
 * @file vmm_host_aspace.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @author Anup patel (anup@brainfault.org)
 * @brief Source file for host virtual address space management.
 */

#include <arch_cpu.h>
#include <arch_board.h>
#include <vmm_error.h>
#include <vmm_math.h>
#include <vmm_list.h>
#include <vmm_string.h>
#include <vmm_stdio.h>
#include <vmm_host_ram.h>
#include <vmm_host_vapool.h>
#include <vmm_host_aspace.h>

virtual_addr_t vmm_host_memmap(physical_addr_t pa, 
			       virtual_size_t sz, 
			       u32 mem_flags)
{
	int rc, ite;
	virtual_addr_t va;
	physical_addr_t tpa;

	sz = VMM_ROUNDUP2_PAGE_SIZE(sz);

	if ((rc = vmm_host_vapool_alloc(&va, sz, FALSE))) {
		/* Don't have space */
		BUG_ON("%s: Don't have space\n", __func__);
	}

	tpa = pa & ~(VMM_PAGE_SIZE - 1);
	for (ite = 0; ite < (sz >> VMM_PAGE_SHIFT); ite++) {
		rc = arch_cpu_aspace_map(va + ite * VMM_PAGE_SIZE, 
					VMM_PAGE_SIZE, 
					tpa + ite * VMM_PAGE_SIZE, 
					mem_flags);
		if (rc) {
			/* We were not able to map physical address */
			BUG_ON("%s: map physical address failed\n", __func__);
		}
	}

	return va + (pa & (VMM_PAGE_SIZE - 1));
}

int vmm_host_memunmap(virtual_addr_t va, virtual_size_t sz)
{
	int rc, ite;

	sz = VMM_ROUNDUP2_PAGE_SIZE(sz);
	va &= ~(VMM_PAGE_SIZE - 1);

	for (ite = 0; ite < (sz >> VMM_PAGE_SHIFT); ite++) {
		rc = arch_cpu_aspace_unmap(va + ite * VMM_PAGE_SIZE, 
					  VMM_PAGE_SIZE);
		if (rc) {
			return rc;
		}
	}

	if ((rc = vmm_host_vapool_free(va, sz))) {
		return rc;
	}

	return VMM_OK;
}

virtual_addr_t vmm_host_alloc_pages(u32 page_count, u32 mem_flags)
{
	int rc = VMM_OK;
	physical_addr_t pa = 0x0;

	rc = vmm_host_ram_alloc(&pa, page_count * VMM_PAGE_SIZE, FALSE);
	if (rc) {
		return 0x0;
	}

	return vmm_host_memmap(pa, page_count * VMM_PAGE_SIZE, mem_flags);
}

int vmm_host_free_pages(virtual_addr_t page_va, u32 page_count)
{
	int rc = VMM_OK;
	physical_addr_t pa = 0x0;

	page_va &= ~VMM_PAGE_MASK;

	if ((rc = arch_cpu_aspace_va2pa(page_va, &pa))) {
		return rc;
	}

	if ((rc = vmm_host_memunmap(page_va, page_count * VMM_PAGE_SIZE))) {
		return rc;
	}

	return vmm_host_ram_free(pa, page_count * VMM_PAGE_SIZE);
}

int vmm_host_page_va2pa(virtual_addr_t page_va, physical_addr_t * page_pa)
{
	int rc = VMM_OK;
	physical_addr_t pa = 0x0;

	page_va &= ~VMM_PAGE_MASK;

	if ((rc = arch_cpu_aspace_va2pa(page_va, &pa))) {
		return rc;
	}

	if (!page_pa) {
		*page_pa = pa;
	}

	return VMM_OK;
}

u32 vmm_host_physical_read(physical_addr_t hphys_addr, 
			   void * dst, u32 len)
{
	u32 bytes_read = 0, to_read = 0;
	virtual_addr_t src = 0x0;

	/* FIXME: Added more sanity checkes for 
	 * allowable physical address 
	 */

	while (bytes_read < len) {
		if (hphys_addr & (VMM_PAGE_SIZE - 1)) {
			to_read = hphys_addr & (VMM_PAGE_SIZE - 1);
		} else {
			to_read = VMM_PAGE_SIZE;
		}
		to_read = (to_read < (len - bytes_read)) ? 
			   to_read : (len - bytes_read);

		src = vmm_host_memmap(hphys_addr, 
				      VMM_PAGE_SIZE, 
				      VMM_MEMORY_READABLE);
		vmm_memcpy(dst, (void *)src, to_read);
		vmm_host_memunmap(src, VMM_PAGE_SIZE);

		hphys_addr += to_read;
		bytes_read += to_read;
		dst += to_read;
	}

	return bytes_read;
}

u32 vmm_host_physical_write(physical_addr_t hphys_addr, 
			    void * src, u32 len)
{
	u32 bytes_written = 0, to_write = 0;
	virtual_addr_t dst = 0x0;

	/* FIXME: Added more sanity checkes for 
	 * allowable physical address 
	 */

	while (bytes_written < len) {
		if (hphys_addr & (VMM_PAGE_SIZE - 1)) {
			to_write = hphys_addr & (VMM_PAGE_SIZE - 1);
		} else {
			to_write = VMM_PAGE_SIZE;
		}
		to_write = (to_write < (len - bytes_written)) ? 
			    to_write : (len - bytes_written);

		dst = vmm_host_memmap(hphys_addr, 
				      VMM_PAGE_SIZE, 
				      VMM_MEMORY_WRITEABLE);
		vmm_memcpy((void *)dst, src, to_write);
		vmm_host_memunmap(dst, VMM_PAGE_SIZE);

		hphys_addr += to_write;
		bytes_written += to_write;
		src += to_write;
	}

	return bytes_written;
}

u32 vmm_host_free_initmem(void)
{
	int rc;
	virtual_addr_t init_start;
	virtual_size_t init_size;

	init_start = arch_init_text_vaddr();
	init_size = arch_init_text_size();
	init_size = VMM_ROUNDUP2_PAGE_SIZE(init_size);

	if ((rc = vmm_host_free_pages(init_start, init_size >> VMM_PAGE_SHIFT))) {
		BUG_ON("%s: Unable to free pages\n", __func__);
	}

	return (init_size >> VMM_PAGE_SHIFT) * VMM_PAGE_SIZE / 1024;
}

int __init vmm_host_aspace_init(void)
{
	int rc;
	physical_addr_t ram_start, resv_pa = 0x0;
	physical_size_t ram_size;
	virtual_addr_t vapool_start, resv_va = 0x0;
	virtual_size_t vapool_size, vapool_hksize, ram_hksize;
	virtual_size_t resv_sz = 0x0, hk_total_size = 0x0;

	/* Determine VAPOOL start, size, and hksize */
	vapool_start = arch_code_vaddr_start();
	vapool_size = (CONFIG_VAPOOL_SIZE << 20);
	vapool_hksize = vmm_host_vapool_estimate_hksize(vapool_size);

	/* Determine RAM start, size an hksize */
	if ((rc = arch_board_ram_start(&ram_start))) {
		return rc;
	}
	if ((rc = arch_board_ram_size(&ram_size))) {
		return rc;
	}
	if (ram_start & VMM_PAGE_MASK) {
		ram_size -= VMM_PAGE_SIZE;
		ram_size += ram_start & VMM_PAGE_MASK;
		ram_start += VMM_PAGE_SIZE;
		ram_start -= ram_start & VMM_PAGE_MASK;
	}
	if (ram_size & VMM_PAGE_MASK) {
		ram_size -= ram_size & VMM_PAGE_MASK;
	}
	ram_hksize = vmm_host_ram_estimate_hksize(ram_size);

	/* Calculate physical address, virtual address, and size of 
	 * reserved area for VAPOOL and RAM house-keeping */
	hk_total_size = vapool_hksize + ram_hksize;
	hk_total_size = VMM_ROUNDUP2_PAGE_SIZE(hk_total_size);
	resv_pa = ram_start;
	resv_va = vapool_start + arch_code_size();
	resv_sz = hk_total_size;
	if ((rc = arch_cpu_aspace_init(&resv_pa, &resv_va, &resv_sz))) {
		return rc;
	}
	if (resv_sz < hk_total_size) {
		return VMM_EFAIL;
	}
	if ((vapool_size <= resv_sz) || 
	    (ram_size <= resv_sz)) {
		return VMM_EFAIL;
	}

	/* Initialize VAPOOL managment */
	if ((rc = vmm_host_vapool_init(vapool_start, 
					vapool_size, 
					resv_va, 
					resv_va, 
					resv_sz))) {
		return rc;
	}

	/* Reserve pages used for Code in VAPOOL */
	if ((rc = vmm_host_vapool_reserve(arch_code_vaddr_start(), 
					  arch_code_size()))) {
		return rc;
	}

	/* Initialize RAM managment */
	if ((rc = vmm_host_ram_init(ram_start, 
				    ram_size, 
				    resv_va + vapool_hksize, 
				    resv_pa, 
				    resv_sz))) {
		return rc;
	}

	/* Reserve pages used for Code in RAM */
	if ((rc = vmm_host_ram_reserve(arch_code_paddr_start(), 
				       arch_code_size()))) {
		return rc;
	}

	return VMM_OK;
}

