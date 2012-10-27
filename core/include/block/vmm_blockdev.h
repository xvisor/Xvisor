/**
 * Copyright (c) 2012 Anup Patel.
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
 * @file vmm_blockdev.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Block Device framework header
 */

#ifndef __VMM_BLOCKDEV_H_
#define __VMM_BLOCKDEV_H_

#include <vmm_types.h>
#include <vmm_devdrv.h>
#include <vmm_spinlocks.h>

#define VMM_BLOCKDEV_CLASS_NAME				"block"
#define VMM_BLOCKDEV_CLASS_IPRIORITY			1
#define VMM_BLOCKDEV_MAX_NAME_SIZE			32

enum vmm_request_type {
	VMM_REQUEST_READ=0,
	VMM_REQUEST_WRITE=1
};

struct vmm_request {
	struct vmm_blockdev *bdev; /* No need to set this field. 
				    * submit_request() will set this field.
				    */

	enum vmm_request_type type;
	u64 lba;
	u32 bcnt;
	void *data;

	void (*completed)(struct vmm_request *);
	void (*failed)(struct vmm_request *);
	void *priv;
};

struct vmm_request_queue {
	vmm_spinlock_t lock;

	int (*make_request)(struct vmm_request_queue *rq, 
			    struct vmm_request *r);
	int (*abort_request)(struct vmm_request_queue *rq, 
			    struct vmm_request *r);

	void *priv;
};

struct vmm_blockdev {
	struct dlist head;
	struct vmm_blockdev *parent;
	
	vmm_spinlock_t child_lock;
	u32 child_count;
	struct dlist child_list;

	char name[VMM_BLOCKDEV_MAX_NAME_SIZE];
	struct vmm_device *dev;
	
	u64 start_lba;
	u64 num_blocks;
	u32 block_size;

	struct vmm_request_queue *rq;
};

/** Generic block IO submit request */
int vmm_blockdev_submit_request(struct vmm_blockdev *bdev,
				struct vmm_request *r);

/** Generic block IO complete reuest */
int vmm_blockdev_complete_request(struct vmm_request *r);

/** Generic block IO fail reuest */
int vmm_blockdev_fail_request(struct vmm_request *r);

/** Generic block IO abort request */
int vmm_blockdev_abort_request(struct vmm_request *r);

/** Allocate block device */
struct vmm_blockdev *vmm_blockdev_alloc(void);

/** Free block device */
void vmm_blockdev_free(struct vmm_blockdev *bdev);

/** Register block device to device driver framework */
int vmm_blockdev_register(struct vmm_blockdev *bdev);

/** Add child block device and register it. */
int vmm_blockdev_add_child(struct vmm_blockdev *bdev, 
			   u64 start_lba, u64 num_blocks);

/** Unregister block device from device driver framework */
int vmm_blockdev_unregister(struct vmm_blockdev *bdev);

/** Find a block device in device driver framework */
struct vmm_blockdev *vmm_blockdev_find(const char *name);

/** Get block device with given number */
struct vmm_blockdev *vmm_blockdev_get(int num);

/** Count number of block devices */
u32 vmm_blockdev_count(void);

#endif /* __VMM_BLOCKDEV_H_ */
