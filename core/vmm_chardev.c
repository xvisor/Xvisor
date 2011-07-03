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
 * @file vmm_chardev.c
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief Character Device framework source
 */

#include <vmm_error.h>
#include <vmm_list.h>
#include <vmm_heap.h>
#include <vmm_string.h>
#include <vmm_chardev.h>

int vmm_chardev_doioctl(vmm_chardev_t * cdev,
			int cmd, void *buf, size_t buf_len)
{
	int ret;

	if (!cdev) {
		return VMM_EFAIL;
	}
	if (!(cdev->ioctl)) {
		return VMM_EFAIL;
	}

	ret = cdev->ioctl(cdev, cmd, buf, buf_len);

	return ret;
}

u32 vmm_chardev_doread(vmm_chardev_t * cdev,
		       u8 *dest, size_t offset, size_t len)
{
	if (!cdev) {
		return 0;
	}
	if (!(cdev->read)) {
		return 0;
	}

	return cdev->read(cdev, dest, offset, len);
}

u32 vmm_chardev_dowrite(vmm_chardev_t * cdev,
			u8 *src, size_t offset, size_t len)
{
	if (!cdev) {
		return 0;
	}
	if (!(cdev->write)) {
		return 0;
	}

	return cdev->write(cdev, src, offset, len);
}

int vmm_chardev_register(vmm_chardev_t * cdev)
{
	vmm_classdev_t *cd;

	if (cdev == NULL) {
		return VMM_EFAIL;
	}
	if (cdev->read == NULL || cdev->write == NULL) {
		return VMM_EFAIL;
	}

	cd = vmm_malloc(sizeof(vmm_classdev_t));
	if (!cd) {
		return VMM_EFAIL;
	}

	INIT_LIST_HEAD(&cd->head);
	vmm_strcpy(cd->name, cdev->name);
	cd->dev = cdev->dev;
	cd->priv = cdev;

	vmm_devdrv_register_classdev(VMM_CHARDEV_CLASS_NAME, cd);

	return VMM_OK;
}

int vmm_chardev_unregister(vmm_chardev_t * cdev)
{
	int rc;
	vmm_classdev_t *cd;

	if (cdev == NULL) {
		return VMM_EFAIL;
	}

	cd = vmm_devdrv_find_classdev(VMM_CHARDEV_CLASS_NAME, cdev->name);
	if (!cd) {
		return VMM_EFAIL;
	}

	rc = vmm_devdrv_unregister_classdev(VMM_CHARDEV_CLASS_NAME, cd);

	if (!rc) {
		vmm_free(cd);
	}

	return rc;
}

vmm_chardev_t *vmm_chardev_find(const char *name)
{
	vmm_classdev_t *cd;

	cd = vmm_devdrv_find_classdev(VMM_CHARDEV_CLASS_NAME, name);

	if (!cd) {
		return NULL;
	}

	return cd->priv;
}

vmm_chardev_t *vmm_chardev_get(int num)
{
	vmm_classdev_t *cd;

	cd = vmm_devdrv_classdev(VMM_CHARDEV_CLASS_NAME, num);

	if (!cd) {
		return NULL;
	}

	return cd->priv;
}

u32 vmm_chardev_count(void)
{
	return vmm_devdrv_classdev_count(VMM_CHARDEV_CLASS_NAME);
}

int vmm_chardev_init(void)
{
	int rc;
	vmm_class_t *c;

	c = vmm_malloc(sizeof(vmm_class_t));
	if (!c) {
		return VMM_EFAIL;
	}

	INIT_LIST_HEAD(&c->head);
	vmm_strcpy(c->name, VMM_CHARDEV_CLASS_NAME);
	INIT_LIST_HEAD(&c->classdev_list);

	rc = vmm_devdrv_register_class(c);
	if (rc) {
		vmm_free(c);
		return rc;
	}

	return VMM_OK;
}
