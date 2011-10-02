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
 * @file arm_mmu.h
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief Header file for MMU functions
 */

#ifndef __ARM_MMU_H_
#define __ARM_MMU_H_

#include <arm_types.h>

void arm_mmu_test(void);
void arm_mmu_setup(void);
void arm_mmu_cleanup(void);

#endif /* __ARM_MMU_H_ */
