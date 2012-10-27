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
 * @file vmm_devtree.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Device Tree Header File.
 */
#ifndef __VMM_DEVTREE_H_
#define __VMM_DEVTREE_H_

#include <vmm_types.h>
#include <libs/list.h>

#define VMM_DEVTREE_PATH_SEPARATOR		'/'
#define VMM_DEVTREE_PATH_SEPARATOR_STRING	"/"

#define VMM_DEVTREE_MODEL_ATTR_NAME		"model"
#define VMM_DEVTREE_DEVICE_TYPE_ATTR_NAME	"device_type"
#define VMM_DEVTREE_DEVICE_TYPE_VAL_CPU		"cpu"
#define VMM_DEVTREE_DEVICE_TYPE_VAL_GUEST	"guest"
#define VMM_DEVTREE_DEVICE_TYPE_VAL_VCPU	"vcpu"
#define VMM_DEVTREE_DEVICE_TYPE_VAL_RAM		"ram"
#define VMM_DEVTREE_DEVICE_TYPE_VAL_ROM		"rom"
#define VMM_DEVTREE_COMPATIBLE_ATTR_NAME	"compatible"
#define VMM_DEVTREE_CLOCK_RATE_ATTR_NAME	"clock-rate"
#define VMM_DEVTREE_REG_ATTR_NAME		"reg"
#define VMM_DEVTREE_VIRTUAL_REG_ATTR_NAME	"virtual-reg"

#define VMM_DEVTREE_VMMINFO_NODE_NAME		"vmm"
#define VMM_DEVTREE_VMMNET_NODE_NAME		"net"
#define VMM_DEVTREE_NETSTACK_NODE_NAME		"hoststack"

#define VMM_DEVTREE_HOSTINFO_NODE_NAME		"host"
#define VMM_DEVTREE_CPUS_NODE_NAME		"cpus"
#define VMM_DEVTREE_CPU_FREQ_MHZ_ATTR_NAME	"cpu_freq_mhz"
#define VMM_DEVTREE_MEMORY_NODE_NAME		"memory"
#define VMM_DEVTREE_MEMORY_PHYS_ADDR_ATTR_NAME	"physical_addr"
#define VMM_DEVTREE_MEMORY_PHYS_SIZE_ATTR_NAME	"physical_size"

#define VMM_DEVTREE_GUESTINFO_NODE_NAME		"guests"
#define VMM_DEVTREE_VCPUS_NODE_NAME		"vcpus"
#define VMM_DEVTREE_START_PC_ATTR_NAME		"start_pc"
#define VMM_DEVTREE_START_SP_ATTR_NAME		"start_sp"
#define VMM_DEVTREE_PRIORITY_ATTR_NAME		"priority"
#define VMM_DEVTREE_TIME_SLICE_ATTR_NAME	"time_slice"
#define VMM_DEVTREE_ADDRSPACE_NODE_NAME		"aspace"
#define VMM_DEVTREE_H2GIRQMAP_ATTR_NAME		"h2g_irq_map"
#define VMM_DEVTREE_MANIFEST_TYPE_ATTR_NAME	"manifest_type"
#define VMM_DEVTREE_MANIFEST_TYPE_VAL_REAL	"real"
#define VMM_DEVTREE_MANIFEST_TYPE_VAL_VIRTUAL	"virtual"
#define VMM_DEVTREE_MANIFEST_TYPE_VAL_ALIAS	"alias"
#define VMM_DEVTREE_ADDRESS_TYPE_ATTR_NAME	"address_type"
#define VMM_DEVTREE_ADDRESS_TYPE_VAL_MEMORY	"memory"
#define VMM_DEVTREE_ADDRESS_TYPE_VAL_IO		"io"
#define VMM_DEVTREE_GUEST_PHYS_ATTR_NAME	"guest_physical_addr"
#define VMM_DEVTREE_HOST_PHYS_ATTR_NAME		"host_physical_addr"
#define VMM_DEVTREE_ALIAS_PHYS_ATTR_NAME	"alias_physical_addr"
#define VMM_DEVTREE_PHYS_SIZE_ATTR_NAME		"physical_size"
#define VMM_DEVTREE_SWITCH_ATTR_NAME		"switch"

enum vmm_devtree_attrypes {
	VMM_DEVTREE_ATTRTYPE_UNKNOWN = 0,
	VMM_DEVTREE_ATTRTYPE_STRING = 1,
	VMM_DEVTREE_ATTRTYPE_UINT32 = 2,
	VMM_DEVTREE_ATTRTYPE_UINT64 = 3,
	VMM_DEVTREE_ATTRTYPE_VIRTADDR=4,
	VMM_DEVTREE_ATTRTYPE_VIRTSIZE=5,
	VMM_DEVTREE_ATTRTYPE_PHYSADDR=6,
	VMM_DEVTREE_ATTRTYPE_PHYSSIZE=7
};

struct vmm_devtree_attr {
	struct dlist head;
	char *name;
	u32 type;
	void *value;
	u32 len;
};

struct vmm_devtree_node {
	struct dlist head;
	char *name;
	void *system_data; /* System data pointer 
			      (Arch. specific code can use this to 
			       pass inforation to device driver) */
	void *priv; /* Generic Private pointer */
	struct vmm_devtree_node *parent;
	struct dlist attr_list;
	struct dlist child_list;
};

/** Check whether given attribute type is literal or literal list 
 *  Note: literal means 32-bit or 64-bit number
 */
bool vmm_devtree_isliteral(u32 attrtype);

/** Get size of literal corresponding to attribute type */
u32 vmm_devtree_literal_size(u32 attrtype);

/** Estimate type of attribute from its name */
u32 vmm_devtree_estimate_attrtype(const char *name);

/** Map device registers to virtual address
 *  Note: This is based on 'reg' and 'virtual-reg' attributes 
 *  of device tree node
 */
int vmm_devtree_regmap(struct vmm_devtree_node *node, 
		       virtual_addr_t *addr, int regset);

/** Get physical size of device registers
 *  Note: This is based on 'reg' and 'virtual-reg' attributes 
 *  of device tree node
 */
int vmm_devtree_regsize(struct vmm_devtree_node *node, 
		        physical_size_t *size, int regset);

/** Get physical address of device registers
 *  Note: This is based on 'reg' and 'virtual-reg' attributes 
 *  of device tree node
 */
int vmm_devtree_regaddr(struct vmm_devtree_node *node, 
		        physical_addr_t *addr, int regset);

/** Unmap device registers from virtual address
 *  Note: This is based on 'reg' and 'virtual-reg' attributes 
 *  of device tree node
 */
int vmm_devtree_regunmap(struct vmm_devtree_node *node, 
			 virtual_addr_t addr, int regset);

/** Get attribute value */
void * vmm_devtree_attrval(struct vmm_devtree_node *node, 
			   const char *attrib);

/** Get length of attribute value */
u32 vmm_devtree_attrlen(struct vmm_devtree_node *node, 
			const char *attrib);

/** Set an attribute for a device tree node */
int vmm_devtree_setattr(struct vmm_devtree_node *node,
			const char *name,
			void *value,
			u32 type,
			u32 len);

/** Get an attribute from a device tree node */
struct vmm_devtree_attr * vmm_devtree_getattr(struct vmm_devtree_node *node,
					      const char *name);

/** Delete an attribute from a device tree node */
int vmm_devtree_delattr(struct vmm_devtree_node *node, const char *name);

/** Create a path string for a given node */
int vmm_devtree_getpath(char *out, struct vmm_devtree_node *node);

/** Get node corresponding to a path string */
struct vmm_devtree_node *vmm_devtree_getnode(const char *path);

/** Get child node below a given node */
struct vmm_devtree_node *vmm_devtree_getchild(struct vmm_devtree_node *node,
					      const char *path);

/** Add new node to device tree */
struct vmm_devtree_node *vmm_devtree_addnode(struct vmm_devtree_node *parent,
					     const char *name);

/** Copy a node to another location in device tree */
int vmm_devtree_copynode(struct vmm_devtree_node *parent,
			 const char *name,
			 struct vmm_devtree_node *src);

/** Delete a node from device tree */
int vmm_devtree_delnode(struct vmm_devtree_node *node);

/** Get the root node */
struct vmm_devtree_node *vmm_devtree_rootnode(void);

/** Initialize device tree */
int vmm_devtree_init(void);

#endif /* __VMM_DEVTREE_H_ */
