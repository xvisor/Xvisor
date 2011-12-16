/**
 * Copyright (c) 2011 Anup Patel.
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
 * @file vmm_semaphore.c
 * @version 0.01
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of sempahore locks for Orphan VCPU (or Thread).
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_scheduler.h>
#include <vmm_semaphore.h>

bool vmm_semaphore_avail(vmm_semaphore_t * sem)
{
	BUG_ON(!sem, "%s: NULL poniter to semaphore\n", __func__);

	return vmm_cpu_atomic_read(&(sem)->value) ? TRUE : FALSE;
}

u32 vmm_semaphore_limit(vmm_semaphore_t * sem)
{
	BUG_ON(!sem, "%s: NULL poniter to semaphore\n", __func__);

	return sem->limit;
}

int vmm_semaphore_up(vmm_semaphore_t * sem)
{
	int rc;
	u32 value;

	/* Sanity Check */
	BUG_ON(!sem, "%s: NULL poniter to semaphore\n", __func__);

	/* Try to increment the semaphore */
	rc = VMM_EFAIL;
	value = vmm_cpu_atomic_read(&sem->value);
	while ((value < sem->limit) && 
		(rc = vmm_cpu_atomic_testnset(&sem->value, value, value + 1))) {
		value = vmm_cpu_atomic_read(&sem->value);
	}

	/* If successful then wakeup all sleeping threads */
	if (!rc) {
		vmm_waitqueue_wakeall(&sem->wq);
	}

	return rc;
}

int vmm_semaphore_down(vmm_semaphore_t * sem)
{
	int rc;
	u32 value;

	/* Sanity Check */
	BUG_ON(!sem, "%s: NULL poniter to semaphore\n", __func__);
	BUG_ON(!vmm_scheduler_orphan_context(), 
		"%s: Down allowed in Orphan VCPU (or Thread) context only\n",
		 __func__);

	/* Decrement the semaphore */
	rc = VMM_EFAIL;
	while (rc) {
		/* Sleep if semaphore not available */
		while (!(value = vmm_cpu_atomic_read(&sem->value))) {
			vmm_waitqueue_sleep(&sem->wq);
		}

		/* Try to decrement the semaphore */
		rc = vmm_cpu_atomic_testnset(&sem->value, value, value - 1);
	}

	return rc;
}

