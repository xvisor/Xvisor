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
 * @file vmm_workqueue.h
 * @version 0.01
 * @author Anup Patel (anup@brainfault.org)
 * @brief Header file of workqueues (special worker threads).
 */

#ifndef __VMM_WORKQUEUE_H__
#define __VMM_WORKQUEUE_H__

#include <vmm_list.h>
#include <vmm_spinlocks.h>
#include <vmm_threads.h>

enum {
	VMM_WORK_STATE_CREATED=0x1,
	VMM_WORK_STATE_SCHEDULED=0x2,
	VMM_WORK_STATE_INPROGRESS=0x4,
	VMM_WORK_STATE_STOPPED=0x8
};

typedef struct vmm_work vmm_work_t;
typedef void (*vmm_work_func_t)(vmm_work_t * work);
typedef struct vmm_workqueue vmm_workqueue_t;

struct vmm_work {
	vmm_spinlock_t lock;
	struct dlist head;
	u32 flags;
	vmm_workqueue_t * wq;
	vmm_work_func_t func;
	void * data;
};

struct vmm_workqueue {
	vmm_spinlock_t lock;
	struct dlist head;
	struct dlist work_list;
	vmm_thread_t * thread;
};

#define INIT_WORK(work, func, data)	do { \
					INIT_SPIN_LOCK(&work->lock); \
					INIT_LIST_HEAD(&work->head); \
					work->flags = VMM_WORK_STATE_CREATED; \
					work->wq = NULL; \
					work->func = func; \
					work->data = data; \
					} while (0); 

/** Check if work is new */
bool vmm_workqueue_work_isnew(vmm_work_t * work);

/** Check if work is pending */
bool vmm_workqueue_work_pending(vmm_work_t * work);

/** Check if work is in-progress */
bool vmm_workqueue_work_inprogress(vmm_work_t * work);

/** Check if work is completed */
bool vmm_workqueue_work_completed(vmm_work_t * work);

/** Schedule work under specific workqueue 
 *  Note: if workqueue is NULL then system workqueues are used.
 */
int vmm_workqueue_schedule_work(vmm_workqueue_t * wq, vmm_work_t * work);

/** Stop a scheduled or in-progress work */
int vmm_workqueue_stop_work(vmm_work_t * work);

/** Forcefully flush all pending work in a workqueue */
int vmm_workqueue_flush(vmm_workqueue_t * wq);

/** Retrive workqueue instance from workqueue index */
vmm_thread_t *vmm_workqueue_get_thread(vmm_workqueue_t * wq);

/** Retrive workqueue instance from workqueue index */
vmm_workqueue_t *vmm_workqueue_index2workqueue(int index);

/** Count number of threads */
u32 vmm_workqueue_count(void);

/** Destroy workqueue */
int vmm_workqueue_destroy(vmm_workqueue_t * wq);

/** Create workqueue with given name and thread priority */
vmm_workqueue_t * vmm_workqueue_create(const char *name, u8 priority);

/** Initialize workqueue framework */
int vmm_workqueue_init(void);

#endif /* __VMM_WORKQUEUE_H__ */
