/**
 * Copyright (c) 2012 Jean-Christophe Dubois.
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
 * @file versatile_config.h
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief Versatile Platform Configuration Header
 */
#ifndef _VERSATILE_CONFIG_H__
#define _VERSATILE_CONFIG_H__

#include <versatile_board.h>

#if !defined(VERSATILE_VIC_NR_IRQS) || (VERSATILE_VIC_NR_IRQS < NR_IRQS_VERSATILE)
#undef VERSATILE_VIC_NR_IRQS
#define VERSATILE_VIC_NR_IRQS			NR_IRQS_VERSATILE
#endif

#if !defined(VERSATILE_VIC_MAX_NR) || (VERSATILE_VIC_MAX_NR < NR_VIC_VERSATILE)
#undef VERSATILE_VIC_MAX_NR
#define VERSATILE_VIC_MAX_NR		NR_VIC_VERSATILE
#endif

#endif
