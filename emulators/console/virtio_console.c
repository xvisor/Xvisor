/**
 * Copyright (c) 2013 Anup Patel.
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
 * @file virtio_console.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief VirtIO based console Emulator.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_devemu.h>
#include <vmm_vserial.h>
#include <libs/stringlib.h>

#include <emu/virtio.h>
#include <emu/virtio_console.h>

#define MODULE_DESC			"VirtIO Console Emulator"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(VIRTIO_IPRIORITY + 1)
#define MODULE_INIT			virtio_console_init
#define MODULE_EXIT			virtio_console_exit

#define VIRTIO_CONSOLE_QUEUE_SIZE	128
#define VIRTIO_CONSOLE_NUM_QUEUES	2
#define VIRTIO_CONSOLE_RX_QUEUE		0
#define VIRTIO_CONSOLE_TX_QUEUE		1

#define VIRTIO_CONSOLE_VSERIAL_FIFO_SZ	1024

struct virtio_console_dev {
	struct virtio_device *vdev;

	struct virtio_queue vqs[VIRTIO_CONSOLE_NUM_QUEUES];
	struct virtio_iovec rx_iov[VIRTIO_CONSOLE_QUEUE_SIZE];
	struct virtio_iovec tx_iov[VIRTIO_CONSOLE_QUEUE_SIZE];
	struct virtio_console_config config;
	u32 features;

	char name[VIRTIO_DEVICE_MAX_NAME_LEN];
	struct vmm_vserial *vser;
};

static u32 virtio_console_get_host_features(struct virtio_device *dev)
{
	/* No host features so, ignore it. */
	return 0;
}

static void virtio_console_set_guest_features(struct virtio_device *dev,
					  u32 features)
{
	/* No host features so, ignore it. */
}

static int virtio_console_init_vq(struct virtio_device *dev,
				  u32 vq, u32 page_size, u32 align, u32 pfn)
{
	int rc;
	struct virtio_console_dev *cdev = dev->emu_data;

	switch (vq) {
	case VIRTIO_CONSOLE_RX_QUEUE:
	case VIRTIO_CONSOLE_TX_QUEUE:
		rc = virtio_queue_setup(&cdev->vqs[vq], dev->guest, 
			pfn, page_size, VIRTIO_CONSOLE_QUEUE_SIZE, align);
		break;
	default:
		rc = VMM_EINVALID;
		break;
	};

	return rc;
}

static int virtio_console_get_pfn_vq(struct virtio_device *dev, u32 vq)
{
	int rc;
	struct virtio_console_dev *cdev = dev->emu_data;

	switch (vq) {
	case VIRTIO_CONSOLE_RX_QUEUE:
	case VIRTIO_CONSOLE_TX_QUEUE:
		rc = virtio_queue_guest_pfn(&cdev->vqs[vq]);
		break;
	default:
		rc = VMM_EINVALID;
		break;
	};

	return rc;
}

static int virtio_console_get_size_vq(struct virtio_device *dev, u32 vq)
{
	int rc;

	switch (vq) {
	case VIRTIO_CONSOLE_RX_QUEUE:
	case VIRTIO_CONSOLE_TX_QUEUE:
		rc = VIRTIO_CONSOLE_QUEUE_SIZE;
		break;
	default:
		rc = 0;
		break;
	};

	return rc;
}

static int virtio_console_set_size_vq(struct virtio_device *dev, u32 vq, int size)
{
	/* FIXME: dynamic */
	return size;
}

static int virtio_console_do_tx(struct virtio_device *dev,
				struct virtio_console_dev *cdev)
{
	u8 buf[8];
	u16 head = 0;
	u32 i, len, iov_cnt = 0, total_len = 0;
	struct virtio_queue *vq = &cdev->vqs[VIRTIO_CONSOLE_TX_QUEUE];
	struct virtio_iovec *iov = cdev->tx_iov;
	struct virtio_iovec tiov;

	while (virtio_queue_available(vq)) {
		head = virtio_queue_get_iovec(vq, iov, &iov_cnt, &total_len);

		for (i = 0; i < iov_cnt; i++) {
			memcpy(&tiov, &iov[i], sizeof(tiov));
			while (tiov.len) {
				len = virtio_iovec_to_buf_read(dev, &tiov, 1,
								&buf, sizeof(buf));
				vmm_vserial_receive(cdev->vser, buf, len);
				tiov.addr += len;
				tiov.len -= len;
			}
		}

		virtio_queue_set_used_elem(vq, head, total_len);
	}

	return VMM_OK;
}

static int virtio_console_notify_vq(struct virtio_device *dev, u32 vq)
{
	int rc = VMM_OK;
	struct virtio_console_dev *cdev = dev->emu_data;

	switch (vq) {
	case VIRTIO_CONSOLE_TX_QUEUE:
		rc = virtio_console_do_tx(dev, cdev);
		break;
	case VIRTIO_CONSOLE_RX_QUEUE:
		break;
	default:
		rc = VMM_EINVALID;
		break;
	}

	return rc;
}

static bool virtio_console_vserial_can_send(struct vmm_vserial *vser)
{
	struct virtio_console_dev *cdev = vser->priv;
	struct virtio_queue *vq = &cdev->vqs[VIRTIO_CONSOLE_RX_QUEUE];

	if (!virtio_queue_available(vq)) {
		return FALSE;
	}

	return TRUE;
}

static int virtio_console_vserial_send(struct vmm_vserial *vser, u8 data)
{
	u16 head = 0;
	u32 iov_cnt = 0, total_len = 0;
	struct virtio_console_dev *cdev = vser->priv;
	struct virtio_queue *vq = &cdev->vqs[VIRTIO_CONSOLE_RX_QUEUE];
	struct virtio_iovec *iov = cdev->rx_iov;
	struct virtio_device *dev = cdev->vdev;

	if (virtio_queue_available(vq)) {
		head = virtio_queue_get_iovec(vq, iov, &iov_cnt, &total_len);
	}

	if (iov_cnt) {
		virtio_buf_to_iovec_write(dev, &iov[0], 1, &data, 1);

		virtio_queue_set_used_elem(vq, head, 1);

		if (virtio_queue_should_signal(vq)) {
			dev->tra->notify(dev, VIRTIO_CONSOLE_RX_QUEUE);
		}
	}

	return VMM_OK;
}

static int virtio_console_read_config(struct virtio_device *dev, 
				      u32 offset, void *dst, u32 dst_len)
{
	struct virtio_console_dev *cdev = dev->emu_data;
	u8 *src = (u8 *)&cdev->config;
	u32 i, src_len = sizeof(cdev->config);

	for (i = 0; (i < dst_len) && ((offset + i) < src_len); i++) {
		*((u8 *)dst + i) = src[offset + i];
	}

	return VMM_OK;
}

static int virtio_console_write_config(struct virtio_device *dev,
				       u32 offset, void *src, u32 src_len)
{
	/* Ignore config writes. */
	return VMM_OK;
}

static int virtio_console_reset(struct virtio_device *dev)
{
	int rc;
	struct virtio_console_dev *cdev = dev->emu_data;

	rc = virtio_queue_cleanup(&cdev->vqs[VIRTIO_CONSOLE_RX_QUEUE]);
	if (rc) {
		return rc;
	}

	rc = virtio_queue_cleanup(&cdev->vqs[VIRTIO_CONSOLE_TX_QUEUE]);
	if (rc) {
		return rc;
	}

	return VMM_OK;
}

static int virtio_console_connect(struct virtio_device *dev, 
				  struct virtio_emulator *emu)
{
	struct virtio_console_dev *cdev;

	cdev = vmm_zalloc(sizeof(struct virtio_console_dev));
	if (!cdev) {
		vmm_printf("Failed to allocate virtio console device....\n");
		return VMM_ENOMEM;
	}
	cdev->vdev = dev;

	vmm_snprintf(cdev->name, VIRTIO_DEVICE_MAX_NAME_LEN, "%s", dev->name);
	cdev->vser = vmm_vserial_create(cdev->name, 
					&virtio_console_vserial_can_send, 
					&virtio_console_vserial_send, 
					VIRTIO_CONSOLE_VSERIAL_FIFO_SZ, cdev);
	if (!cdev->vser) {
		return VMM_EFAIL;
	}
	
	cdev->config.cols = 80;
	cdev->config.rows = 24;
	cdev->config.max_nr_ports = 1;

	dev->emu_data = cdev;

	return VMM_OK;
}

static void virtio_console_disconnect(struct virtio_device *dev)
{
	struct virtio_console_dev *cdev = dev->emu_data;

	vmm_vserial_destroy(cdev->vser);
	vmm_free(cdev);
}

struct virtio_device_id virtio_console_emu_id[] = {
	{.type = VIRTIO_ID_CONSOLE},
	{ },
};

struct virtio_emulator virtio_console = {
	.name = "virtio_console",
	.id_table = virtio_console_emu_id,

	/* VirtIO operations */
	.get_host_features      = virtio_console_get_host_features,
	.set_guest_features     = virtio_console_set_guest_features,
	.init_vq                = virtio_console_init_vq,
	.get_pfn_vq             = virtio_console_get_pfn_vq,
	.get_size_vq            = virtio_console_get_size_vq,
	.set_size_vq            = virtio_console_set_size_vq,
	.notify_vq              = virtio_console_notify_vq,

	/* Emulator operations */
	.read_config = virtio_console_read_config,
	.write_config = virtio_console_write_config,
	.reset = virtio_console_reset,
	.connect = virtio_console_connect,
	.disconnect = virtio_console_disconnect,
};

static int __init virtio_console_init(void)
{
	return virtio_register_emulator(&virtio_console);
}

static void __exit virtio_console_exit(void)
{
	virtio_unregister_emulator(&virtio_console);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
