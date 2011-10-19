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
 * @file vmm_vserial.h
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for virtual serial port
 */
#ifndef _VMM_VSERIAL_H__
#define _VMM_VSERIAL_H__

#include <vmm_types.h>
#include <vmm_list.h>
#include <vmm_ringbuf.h>

typedef struct vmm_vserial_receiver vmm_vserial_receiver_t;
typedef struct vmm_vserial vmm_vserial_t;
typedef struct vmm_vserial_ctrl vmm_vserial_ctrl_t;
typedef bool (*vmm_vserial_can_send_t) (vmm_vserial_t *vser);
typedef int (*vmm_vserial_send_t) (vmm_vserial_t *vser, u8 data);
typedef void (*vmm_vserial_recv_t) (vmm_vserial_t *vser, void * priv, u8 data);

struct vmm_vserial_receiver {
	struct dlist head;
	vmm_vserial_recv_t recv;
	void * priv;
};

struct vmm_vserial {
	struct dlist head;
	char name[64];
	vmm_vserial_can_send_t can_send;
	vmm_vserial_send_t send;
	struct dlist receiver_list;
	vmm_ringbuf_t * receive_buf;
	void *priv;
};

struct vmm_vserial_ctrl {
	struct dlist vser_list;
};

/** Send bytes to virtual serial port */
u32 vmm_vserial_send(vmm_vserial_t * vser, u8 *src, u32 len);

/** Receive bytes from virtual serial port */
u32 vmm_vserial_receive(vmm_vserial_t * vser, u8 *dst, u32 len);

/** Register receiver to a virtual serial port */
int vmm_vserial_register_receiver(vmm_vserial_t * vser, 
				  vmm_vserial_recv_t recv, 
				  void * priv);

/** Unregister a virtual serial port */
int vmm_vserial_unregister_receiver(vmm_vserial_t * vser,
				    vmm_vserial_recv_t recv,
				    void * priv);

/** Alloc a virtual serial port */
vmm_vserial_t * vmm_vserial_alloc(const char * name,
				  vmm_vserial_can_send_t can_send,
				  vmm_vserial_send_t send,
				  u32 receive_buf_size,
				  void * priv);

/** Free a virtual serial port */
int vmm_vserial_free(vmm_vserial_t * vser);

/** Find a virtual serial port with given name */
vmm_vserial_t * vmm_vserial_find(const char *name);

/** Get a virtual serial port with given index */
vmm_vserial_t * vmm_vserial_get(int index);

/** Count of available virtual serial ports */
u32 vmm_vserial_count(void);

/** Initialize virtual serial port framework */
int vmm_vserial_init(void);

#endif
