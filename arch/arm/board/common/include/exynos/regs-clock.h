/**
 * Copyright (c) 2013 Jean-Christophe Dubois.
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
 * @file regs-clock.h
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief EXYNOS4 Clock register definitions
 *
 * Adapted from linux/arch/arm/mach-exynos4/include/mach/regs-clock.h
 *
 * Copyright (c) 2010-2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS4 - Clock register definitions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __ASM_ARCH_REGS_CLOCK_H
#define __ASM_ARCH_REGS_CLOCK_H __FILE__

#include <exynos/plat/cpu.h>
#include <exynos/mach/map.h>

#define EXYNOS_CLKREG(x)			(x)

#define EXYNOS4_CLKDIV_LEFTBUS			EXYNOS_CLKREG(0x04500)
#define EXYNOS4_CLKDIV_STAT_LEFTBUS		EXYNOS_CLKREG(0x04600)
#define EXYNOS4_CLKGATE_IP_LEFTBUS		EXYNOS_CLKREG(0x04800)

#define EXYNOS4_CLKDIV_RIGHTBUS			EXYNOS_CLKREG(0x08500)
#define EXYNOS4_CLKDIV_STAT_RIGHTBUS		EXYNOS_CLKREG(0x08600)
#define EXYNOS4_CLKGATE_IP_RIGHTBUS		EXYNOS_CLKREG(0x08800)

#define EXYNOS4_EPLL_LOCK			EXYNOS_CLKREG(0x0C010)
#define EXYNOS4_VPLL_LOCK			EXYNOS_CLKREG(0x0C020)

#define EXYNOS4_EPLL_CON0			EXYNOS_CLKREG(0x0C110)
#define EXYNOS4_EPLL_CON1			EXYNOS_CLKREG(0x0C114)
#define EXYNOS4_VPLL_CON0			EXYNOS_CLKREG(0x0C120)
#define EXYNOS4_VPLL_CON1			EXYNOS_CLKREG(0x0C124)

#define EXYNOS4_CLKSRC_TOP0			EXYNOS_CLKREG(0x0C210)
#define EXYNOS4_CLKSRC_TOP1			EXYNOS_CLKREG(0x0C214)
#define EXYNOS4_CLKSRC_CAM			EXYNOS_CLKREG(0x0C220)
#define EXYNOS4_CLKSRC_TV			EXYNOS_CLKREG(0x0C224)
#define EXYNOS4_CLKSRC_MFC			EXYNOS_CLKREG(0x0C228)
#define EXYNOS4_CLKSRC_G3D			EXYNOS_CLKREG(0x0C22C)
#define EXYNOS4_CLKSRC_IMAGE			EXYNOS_CLKREG(0x0C230)
#define EXYNOS4_CLKSRC_LCD0			EXYNOS_CLKREG(0x0C234)
#define EXYNOS4_CLKSRC_MAUDIO			EXYNOS_CLKREG(0x0C23C)
#define EXYNOS4_CLKSRC_FSYS			EXYNOS_CLKREG(0x0C240)
#define EXYNOS4_CLKSRC_PERIL0			EXYNOS_CLKREG(0x0C250)
#define EXYNOS4_CLKSRC_PERIL1			EXYNOS_CLKREG(0x0C254)

#define EXYNOS4_CLKSRC_MASK_TOP			EXYNOS_CLKREG(0x0C310)
#define EXYNOS4_CLKSRC_MASK_CAM			EXYNOS_CLKREG(0x0C320)
#define EXYNOS4_CLKSRC_MASK_TV			EXYNOS_CLKREG(0x0C324)
#define EXYNOS4_CLKSRC_MASK_LCD0		EXYNOS_CLKREG(0x0C334)
#define EXYNOS4_CLKSRC_MASK_MAUDIO		EXYNOS_CLKREG(0x0C33C)
#define EXYNOS4_CLKSRC_MASK_FSYS		EXYNOS_CLKREG(0x0C340)
#define EXYNOS4_CLKSRC_MASK_PERIL0		EXYNOS_CLKREG(0x0C350)
#define EXYNOS4_CLKSRC_MASK_PERIL1		EXYNOS_CLKREG(0x0C354)

#define EXYNOS4_CLKDIV_TOP			EXYNOS_CLKREG(0x0C510)
#define EXYNOS4_CLKDIV_CAM			EXYNOS_CLKREG(0x0C520)
#define EXYNOS4_CLKDIV_TV			EXYNOS_CLKREG(0x0C524)
#define EXYNOS4_CLKDIV_MFC			EXYNOS_CLKREG(0x0C528)
#define EXYNOS4_CLKDIV_G3D			EXYNOS_CLKREG(0x0C52C)
#define EXYNOS4_CLKDIV_IMAGE			EXYNOS_CLKREG(0x0C530)
#define EXYNOS4_CLKDIV_LCD0			EXYNOS_CLKREG(0x0C534)
#define EXYNOS4_CLKDIV_MAUDIO			EXYNOS_CLKREG(0x0C53C)
#define EXYNOS4_CLKDIV_FSYS0			EXYNOS_CLKREG(0x0C540)
#define EXYNOS4_CLKDIV_FSYS1			EXYNOS_CLKREG(0x0C544)
#define EXYNOS4_CLKDIV_FSYS2			EXYNOS_CLKREG(0x0C548)
#define EXYNOS4_CLKDIV_FSYS3			EXYNOS_CLKREG(0x0C54C)
#define EXYNOS4_CLKDIV_PERIL0			EXYNOS_CLKREG(0x0C550)
#define EXYNOS4_CLKDIV_PERIL1			EXYNOS_CLKREG(0x0C554)
#define EXYNOS4_CLKDIV_PERIL2			EXYNOS_CLKREG(0x0C558)
#define EXYNOS4_CLKDIV_PERIL3			EXYNOS_CLKREG(0x0C55C)
#define EXYNOS4_CLKDIV_PERIL4			EXYNOS_CLKREG(0x0C560)
#define EXYNOS4_CLKDIV_PERIL5			EXYNOS_CLKREG(0x0C564)
#define EXYNOS4_CLKDIV2_RATIO			EXYNOS_CLKREG(0x0C580)

#define EXYNOS4_CLKDIV_STAT_TOP			EXYNOS_CLKREG(0x0C610)
#define EXYNOS4_CLKDIV_STAT_MFC			EXYNOS_CLKREG(0x0C628)

#define EXYNOS4_CLKGATE_SCLKCAM			EXYNOS_CLKREG(0x0C820)
#define EXYNOS4_CLKGATE_IP_CAM			EXYNOS_CLKREG(0x0C920)
#define EXYNOS4_CLKGATE_IP_TV			EXYNOS_CLKREG(0x0C924)
#define EXYNOS4_CLKGATE_IP_MFC			EXYNOS_CLKREG(0x0C928)
#define EXYNOS4_CLKGATE_IP_G3D			EXYNOS_CLKREG(0x0C92C)
#define EXYNOS4_CLKGATE_IP_IMAGE		(soc_is_exynos4210() ? \
						EXYNOS_CLKREG(0x0C930) : \
						EXYNOS_CLKREG(0x04930))
#define EXYNOS4210_CLKGATE_IP_IMAGE		EXYNOS_CLKREG(0x0C930)
#define EXYNOS4212_CLKGATE_IP_IMAGE		EXYNOS_CLKREG(0x04930)
#define EXYNOS4_CLKGATE_IP_LCD0			EXYNOS_CLKREG(0x0C934)
#define EXYNOS4_CLKGATE_IP_FSYS			EXYNOS_CLKREG(0x0C940)
#define EXYNOS4_CLKGATE_IP_GPS			EXYNOS_CLKREG(0x0C94C)
#define EXYNOS4_CLKGATE_IP_PERIL		EXYNOS_CLKREG(0x0C950)
#define EXYNOS4_CLKGATE_IP_PERIR		(soc_is_exynos4210() ? \
						EXYNOS_CLKREG(0x0C960) : \
						EXYNOS_CLKREG(0x08960))
#define EXYNOS4210_CLKGATE_IP_PERIR		EXYNOS_CLKREG(0x0C960)
#define EXYNOS4212_CLKGATE_IP_PERIR		EXYNOS_CLKREG(0x08960)
#define EXYNOS4_CLKGATE_BLOCK			EXYNOS_CLKREG(0x0C970)

#define EXYNOS4_CLKSRC_MASK_DMC			EXYNOS_CLKREG(0x10300)
#define EXYNOS4_CLKSRC_DMC			EXYNOS_CLKREG(0x10200)
#define EXYNOS4_CLKDIV_DMC0			EXYNOS_CLKREG(0x10500)
#define EXYNOS4_CLKDIV_DMC1			EXYNOS_CLKREG(0x10504)
#define EXYNOS4_CLKDIV_STAT_DMC0		EXYNOS_CLKREG(0x10600)
#define EXYNOS4_CLKDIV_STAT_DMC1		EXYNOS_CLKREG(0x10604)
#define EXYNOS4_CLKGATE_IP_DMC			EXYNOS_CLKREG(0x10900)

#define EXYNOS4_DMC_PAUSE_CTRL			EXYNOS_CLKREG(0x11094)
#define EXYNOS4_DMC_PAUSE_ENABLE		(1 << 0)

#define EXYNOS4_APLL_LOCK			EXYNOS_CLKREG(0x14000)
#define EXYNOS4_MPLL_LOCK			(soc_is_exynos4210() ? \
						EXYNOS_CLKREG(0x14004) :  \
						EXYNOS_CLKREG(0x10008))
#define EXYNOS4_APLL_CON0			EXYNOS_CLKREG(0x14100)
#define EXYNOS4_APLL_CON1			EXYNOS_CLKREG(0x14104)
#define EXYNOS4_MPLL_CON0			(soc_is_exynos4210() ? \
						EXYNOS_CLKREG(0x14108) : \
						EXYNOS_CLKREG(0x10108))
#define EXYNOS4_MPLL_CON1			(soc_is_exynos4210() ? \
						EXYNOS_CLKREG(0x1410C) : \
						EXYNOS_CLKREG(0x1010C))

#define EXYNOS4_CLKSRC_CPU			EXYNOS_CLKREG(0x14200)
#define EXYNOS4_CLKMUX_STATCPU			EXYNOS_CLKREG(0x14400)

#define EXYNOS4_CLKDIV_CPU			EXYNOS_CLKREG(0x14500)
#define EXYNOS4_CLKDIV_CPU1			EXYNOS_CLKREG(0x14504)
#define EXYNOS4_CLKDIV_STATCPU			EXYNOS_CLKREG(0x14600)
#define EXYNOS4_CLKDIV_STATCPU1			EXYNOS_CLKREG(0x14604)

#define EXYNOS4_CLKGATE_SCLKCPU			EXYNOS_CLKREG(0x14800)
#define EXYNOS4_CLKGATE_IP_CPU			EXYNOS_CLKREG(0x14900)

#define EXYNOS4_CLKGATE_IP_ISP0			EXYNOS_CLKREG(0x18800)
#define EXYNOS4_CLKGATE_IP_ISP1			EXYNOS_CLKREG(0x18804)

#define EXYNOS4_APLL_LOCKTIME			(0x1C20)	/* 300us */

#define EXYNOS4_APLLCON0_ENABLE_SHIFT		(31)
#define EXYNOS4_APLLCON0_LOCKED_SHIFT		(29)
#define EXYNOS4_APLL_VAL_1000			((250 << 16) | (6 << 8) | 1)
#define EXYNOS4_APLL_VAL_800			((200 << 16) | (6 << 8) | 1)

#define EXYNOS4_EPLLCON0_ENABLE_SHIFT		(31)
#define EXYNOS4_EPLLCON0_LOCKED_SHIFT		(29)

#define EXYNOS4_VPLLCON0_ENABLE_SHIFT		(31)
#define EXYNOS4_VPLLCON0_LOCKED_SHIFT		(29)

#define EXYNOS4_CLKSRC_CPU_MUXCORE_SHIFT	(16)
#define EXYNOS4_CLKMUX_STATCPU_MUXCORE_MASK	(0x7 << EXYNOS4_CLKSRC_CPU_MUXCORE_SHIFT)

#define EXYNOS4_CLKDIV_CPU0_CORE_SHIFT		(0)
#define EXYNOS4_CLKDIV_CPU0_CORE_MASK		(0x7 << EXYNOS4_CLKDIV_CPU0_CORE_SHIFT)
#define EXYNOS4_CLKDIV_CPU0_COREM0_SHIFT	(4)
#define EXYNOS4_CLKDIV_CPU0_COREM0_MASK		(0x7 << EXYNOS4_CLKDIV_CPU0_COREM0_SHIFT)
#define EXYNOS4_CLKDIV_CPU0_COREM1_SHIFT	(8)
#define EXYNOS4_CLKDIV_CPU0_COREM1_MASK		(0x7 << EXYNOS4_CLKDIV_CPU0_COREM1_SHIFT)
#define EXYNOS4_CLKDIV_CPU0_PERIPH_SHIFT	(12)
#define EXYNOS4_CLKDIV_CPU0_PERIPH_MASK		(0x7 << EXYNOS4_CLKDIV_CPU0_PERIPH_SHIFT)
#define EXYNOS4_CLKDIV_CPU0_ATB_SHIFT		(16)
#define EXYNOS4_CLKDIV_CPU0_ATB_MASK		(0x7 << EXYNOS4_CLKDIV_CPU0_ATB_SHIFT)
#define EXYNOS4_CLKDIV_CPU0_PCLKDBG_SHIFT	(20)
#define EXYNOS4_CLKDIV_CPU0_PCLKDBG_MASK	(0x7 << EXYNOS4_CLKDIV_CPU0_PCLKDBG_SHIFT)
#define EXYNOS4_CLKDIV_CPU0_APLL_SHIFT		(24)
#define EXYNOS4_CLKDIV_CPU0_APLL_MASK		(0x7 << EXYNOS4_CLKDIV_CPU0_APLL_SHIFT)
#define EXYNOS4_CLKDIV_CPU0_CORE2_SHIFT		28
#define EXYNOS4_CLKDIV_CPU0_CORE2_MASK		(0x7 << EXYNOS4_CLKDIV_CPU0_CORE2_SHIFT)

#define EXYNOS4_CLKDIV_CPU1_COPY_SHIFT		0
#define EXYNOS4_CLKDIV_CPU1_COPY_MASK		(0x7 << EXYNOS4_CLKDIV_CPU1_COPY_SHIFT)
#define EXYNOS4_CLKDIV_CPU1_HPM_SHIFT		4
#define EXYNOS4_CLKDIV_CPU1_HPM_MASK		(0x7 << EXYNOS4_CLKDIV_CPU1_HPM_SHIFT)
#define EXYNOS4_CLKDIV_CPU1_CORES_SHIFT		8
#define EXYNOS4_CLKDIV_CPU1_CORES_MASK		(0x7 << EXYNOS4_CLKDIV_CPU1_CORES_SHIFT)

#define EXYNOS4_CLKDIV_DMC0_ACP_SHIFT		(0)
#define EXYNOS4_CLKDIV_DMC0_ACP_MASK		(0x7 << EXYNOS4_CLKDIV_DMC0_ACP_SHIFT)
#define EXYNOS4_CLKDIV_DMC0_ACPPCLK_SHIFT	(4)
#define EXYNOS4_CLKDIV_DMC0_ACPPCLK_MASK	(0x7 << EXYNOS4_CLKDIV_DMC0_ACPPCLK_SHIFT)
#define EXYNOS4_CLKDIV_DMC0_DPHY_SHIFT		(8)
#define EXYNOS4_CLKDIV_DMC0_DPHY_MASK		(0x7 << EXYNOS4_CLKDIV_DMC0_DPHY_SHIFT)
#define EXYNOS4_CLKDIV_DMC0_DMC_SHIFT		(12)
#define EXYNOS4_CLKDIV_DMC0_DMC_MASK		(0x7 << EXYNOS4_CLKDIV_DMC0_DMC_SHIFT)
#define EXYNOS4_CLKDIV_DMC0_DMCD_SHIFT		(16)
#define EXYNOS4_CLKDIV_DMC0_DMCD_MASK		(0x7 << EXYNOS4_CLKDIV_DMC0_DMCD_SHIFT)
#define EXYNOS4_CLKDIV_DMC0_DMCP_SHIFT		(20)
#define EXYNOS4_CLKDIV_DMC0_DMCP_MASK		(0x7 << EXYNOS4_CLKDIV_DMC0_DMCP_SHIFT)
#define EXYNOS4_CLKDIV_DMC0_COPY2_SHIFT		(24)
#define EXYNOS4_CLKDIV_DMC0_COPY2_MASK		(0x7 << EXYNOS4_CLKDIV_DMC0_COPY2_SHIFT)
#define EXYNOS4_CLKDIV_DMC0_CORETI_SHIFT	(28)
#define EXYNOS4_CLKDIV_DMC0_CORETI_MASK		(0x7 << EXYNOS4_CLKDIV_DMC0_CORETI_SHIFT)

#define EXYNOS4_CLKDIV_DMC1_G2D_ACP_SHIFT	(0)
#define EXYNOS4_CLKDIV_DMC1_G2D_ACP_MASK	(0xf << EXYNOS4_CLKDIV_DMC1_G2D_ACP_SHIFT)
#define EXYNOS4_CLKDIV_DMC1_C2C_SHIFT		(4)
#define EXYNOS4_CLKDIV_DMC1_C2C_MASK		(0x7 << EXYNOS4_CLKDIV_DMC1_C2C_SHIFT)
#define EXYNOS4_CLKDIV_DMC1_PWI_SHIFT		(8)
#define EXYNOS4_CLKDIV_DMC1_PWI_MASK		(0xf << EXYNOS4_CLKDIV_DMC1_PWI_SHIFT)
#define EXYNOS4_CLKDIV_DMC1_C2CACLK_SHIFT	(12)
#define EXYNOS4_CLKDIV_DMC1_C2CACLK_MASK	(0x7 << EXYNOS4_CLKDIV_DMC1_C2CACLK_SHIFT)
#define EXYNOS4_CLKDIV_DMC1_DVSEM_SHIFT		(16)
#define EXYNOS4_CLKDIV_DMC1_DVSEM_MASK		(0x7f << EXYNOS4_CLKDIV_DMC1_DVSEM_SHIFT)
#define EXYNOS4_CLKDIV_DMC1_DPM_SHIFT		(24)
#define EXYNOS4_CLKDIV_DMC1_DPM_MASK		(0x7f << EXYNOS4_CLKDIV_DMC1_DPM_SHIFT)

#define EXYNOS4_CLKDIV_MFC_SHIFT		(0)
#define EXYNOS4_CLKDIV_MFC_MASK			(0x7 << EXYNOS4_CLKDIV_MFC_SHIFT)

#define EXYNOS4_CLKDIV_TOP_ACLK200_SHIFT	(0)
#define EXYNOS4_CLKDIV_TOP_ACLK200_MASK		(0x7 << EXYNOS4_CLKDIV_TOP_ACLK200_SHIFT)
#define EXYNOS4_CLKDIV_TOP_ACLK100_SHIFT	(4)
#define EXYNOS4_CLKDIV_TOP_ACLK100_MASK		(0xF << EXYNOS4_CLKDIV_TOP_ACLK100_SHIFT)
#define EXYNOS4_CLKDIV_TOP_ACLK160_SHIFT	(8)
#define EXYNOS4_CLKDIV_TOP_ACLK160_MASK		(0x7 << EXYNOS4_CLKDIV_TOP_ACLK160_SHIFT)
#define EXYNOS4_CLKDIV_TOP_ACLK133_SHIFT	(12)
#define EXYNOS4_CLKDIV_TOP_ACLK133_MASK		(0x7 << EXYNOS4_CLKDIV_TOP_ACLK133_SHIFT)
#define EXYNOS4_CLKDIV_TOP_ONENAND_SHIFT	(16)
#define EXYNOS4_CLKDIV_TOP_ONENAND_MASK		(0x7 << EXYNOS4_CLKDIV_TOP_ONENAND_SHIFT)
#define EXYNOS4_CLKDIV_TOP_ACLK266_GPS_SHIFT	(20)
#define EXYNOS4_CLKDIV_TOP_ACLK266_GPS_MASK	(0x7 << EXYNOS4_CLKDIV_TOP_ACLK266_GPS_SHIFT)
#define EXYNOS4_CLKDIV_TOP_ACLK400_MCUISP_SHIFT	(24)
#define EXYNOS4_CLKDIV_TOP_ACLK400_MCUISP_MASK	(0x7 << EXYNOS4_CLKDIV_TOP_ACLK400_MCUISP_SHIFT)

#define EXYNOS4_CLKDIV_BUS_GDLR_SHIFT		(0)
#define EXYNOS4_CLKDIV_BUS_GDLR_MASK		(0x7 << EXYNOS4_CLKDIV_BUS_GDLR_SHIFT)
#define EXYNOS4_CLKDIV_BUS_GPLR_SHIFT		(4)
#define EXYNOS4_CLKDIV_BUS_GPLR_MASK		(0x7 << EXYNOS4_CLKDIV_BUS_GPLR_SHIFT)

#define EXYNOS4_CLKDIV_CAM_FIMC0_SHIFT		(0)
#define EXYNOS4_CLKDIV_CAM_FIMC0_MASK		(0xf << EXYNOS4_CLKDIV_CAM_FIMC0_SHIFT)
#define EXYNOS4_CLKDIV_CAM_FIMC1_SHIFT		(4)
#define EXYNOS4_CLKDIV_CAM_FIMC1_MASK		(0xf << EXYNOS4_CLKDIV_CAM_FIMC1_SHIFT)
#define EXYNOS4_CLKDIV_CAM_FIMC2_SHIFT		(8)
#define EXYNOS4_CLKDIV_CAM_FIMC2_MASK		(0xf << EXYNOS4_CLKDIV_CAM_FIMC2_SHIFT)
#define EXYNOS4_CLKDIV_CAM_FIMC3_SHIFT		(12)
#define EXYNOS4_CLKDIV_CAM_FIMC3_MASK		(0xf << EXYNOS4_CLKDIV_CAM_FIMC3_SHIFT)

/* Only for EXYNOS4210 */

#define EXYNOS4210_CLKSRC_LCD1			EXYNOS_CLKREG(0x0C238)
#define EXYNOS4210_CLKSRC_MASK_LCD1		EXYNOS_CLKREG(0x0C338)
#define EXYNOS4210_CLKDIV_LCD1			EXYNOS_CLKREG(0x0C538)
#define EXYNOS4210_CLKGATE_IP_LCD1		EXYNOS_CLKREG(0x0C938)

/* Only for EXYNOS4212 */

#define EXYNOS4_CLKDIV_CAM1			EXYNOS_CLKREG(0x0C568)

#define EXYNOS4_CLKDIV_STAT_CAM1		EXYNOS_CLKREG(0x0C668)

#define EXYNOS4_CLKDIV_CAM1_JPEG_SHIFT		(0)
#define EXYNOS4_CLKDIV_CAM1_JPEG_MASK		(0xf << EXYNOS4_CLKDIV_CAM1_JPEG_SHIFT)

/* For EXYNOS5250 */

#define EXYNOS5_APLL_LOCK			EXYNOS_CLKREG(0x00000)
#define EXYNOS5_APLL_CON0			EXYNOS_CLKREG(0x00100)
#define EXYNOS5_CLKSRC_CPU			EXYNOS_CLKREG(0x00200)
#define EXYNOS5_CLKMUX_STATCPU			EXYNOS_CLKREG(0x00400)
#define EXYNOS5_CLKDIV_CPU0			EXYNOS_CLKREG(0x00500)
#define EXYNOS5_CLKDIV_CPU1			EXYNOS_CLKREG(0x00504)
#define EXYNOS5_CLKDIV_STATCPU0			EXYNOS_CLKREG(0x00600)
#define EXYNOS5_CLKDIV_STATCPU1			EXYNOS_CLKREG(0x00604)

#define EXYNOS5_MPLL_CON0			EXYNOS_CLKREG(0x04100)
#define EXYNOS5_CLKSRC_CORE1			EXYNOS_CLKREG(0x04204)

#define EXYNOS5_CLKGATE_IP_CORE			EXYNOS_CLKREG(0x04900)

#define EXYNOS5_CLKDIV_ACP			EXYNOS_CLKREG(0x08500)

#define EXYNOS5_EPLL_CON0			EXYNOS_CLKREG(0x10130)
#define EXYNOS5_EPLL_CON1			EXYNOS_CLKREG(0x10134)
#define EXYNOS5_EPLL_CON2			EXYNOS_CLKREG(0x10138)
#define EXYNOS5_VPLL_CON0			EXYNOS_CLKREG(0x10140)
#define EXYNOS5_VPLL_CON1			EXYNOS_CLKREG(0x10144)
#define EXYNOS5_VPLL_CON2			EXYNOS_CLKREG(0x10148)
#define EXYNOS5_CPLL_CON0			EXYNOS_CLKREG(0x10120)

#define EXYNOS5_CLKSRC_TOP0			EXYNOS_CLKREG(0x10210)
#define EXYNOS5_CLKSRC_TOP1			EXYNOS_CLKREG(0x10214)
#define EXYNOS5_CLKSRC_TOP2			EXYNOS_CLKREG(0x10218)
#define EXYNOS5_CLKSRC_TOP3			EXYNOS_CLKREG(0x1021C)
#define EXYNOS5_CLKSRC_GSCL			EXYNOS_CLKREG(0x10220)
#define EXYNOS5_CLKSRC_DISP1_0			EXYNOS_CLKREG(0x1022C)
#define EXYNOS5_CLKSRC_MAUDIO			EXYNOS_CLKREG(0x10240)
#define EXYNOS5_CLKSRC_FSYS			EXYNOS_CLKREG(0x10244)
#define EXYNOS5_CLKSRC_PERIC0			EXYNOS_CLKREG(0x10250)
#define EXYNOS5_CLKSRC_PERIC1			EXYNOS_CLKREG(0x10254)
#define EXYNOS5_SCLK_SRC_ISP			EXYNOS_CLKREG(0x10270)

#define EXYNOS5_CLKSRC_MASK_TOP			EXYNOS_CLKREG(0x10310)
#define EXYNOS5_CLKSRC_MASK_GSCL		EXYNOS_CLKREG(0x10320)
#define EXYNOS5_CLKSRC_MASK_DISP1_0		EXYNOS_CLKREG(0x1032C)
#define EXYNOS5_CLKSRC_MASK_MAUDIO		EXYNOS_CLKREG(0x10334)
#define EXYNOS5_CLKSRC_MASK_FSYS		EXYNOS_CLKREG(0x10340)
#define EXYNOS5_CLKSRC_MASK_PERIC0		EXYNOS_CLKREG(0x10350)
#define EXYNOS5_CLKSRC_MASK_PERIC1		EXYNOS_CLKREG(0x10354)

#define EXYNOS5_CLKDIV_TOP0			EXYNOS_CLKREG(0x10510)
#define EXYNOS5_CLKDIV_TOP1			EXYNOS_CLKREG(0x10514)
#define EXYNOS5_CLKDIV_GSCL			EXYNOS_CLKREG(0x10520)
#define EXYNOS5_CLKDIV_DISP1_0			EXYNOS_CLKREG(0x1052C)
#define EXYNOS5_CLKDIV_GEN			EXYNOS_CLKREG(0x1053C)
#define EXYNOS5_CLKDIV_MAUDIO			EXYNOS_CLKREG(0x10544)
#define EXYNOS5_CLKDIV_FSYS0			EXYNOS_CLKREG(0x10548)
#define EXYNOS5_CLKDIV_FSYS1			EXYNOS_CLKREG(0x1054C)
#define EXYNOS5_CLKDIV_FSYS2			EXYNOS_CLKREG(0x10550)
#define EXYNOS5_CLKDIV_FSYS3			EXYNOS_CLKREG(0x10554)
#define EXYNOS5_CLKDIV_PERIC0			EXYNOS_CLKREG(0x10558)
#define EXYNOS5_CLKDIV_PERIC1			EXYNOS_CLKREG(0x1055C)
#define EXYNOS5_CLKDIV_PERIC2			EXYNOS_CLKREG(0x10560)
#define EXYNOS5_CLKDIV_PERIC3			EXYNOS_CLKREG(0x10564)
#define EXYNOS5_CLKDIV_PERIC4			EXYNOS_CLKREG(0x10568)
#define EXYNOS5_CLKDIV_PERIC5			EXYNOS_CLKREG(0x1056C)
#define EXYNOS5_SCLK_DIV_ISP			EXYNOS_CLKREG(0x10580)

#define EXYNOS5_CLKGATE_IP_ACP			EXYNOS_CLKREG(0x08800)
#define EXYNOS5_CLKGATE_IP_ISP0			EXYNOS_CLKREG(0x0C800)
#define EXYNOS5_CLKGATE_IP_ISP1			EXYNOS_CLKREG(0x0C804)
#define EXYNOS5_CLKGATE_IP_GSCL			EXYNOS_CLKREG(0x10920)
#define EXYNOS5_CLKGATE_IP_DISP1		EXYNOS_CLKREG(0x10928)
#define EXYNOS5_CLKGATE_IP_MFC			EXYNOS_CLKREG(0x1092C)
#define EXYNOS5_CLKGATE_IP_G3D			EXYNOS_CLKREG(0x10930)
#define EXYNOS5_CLKGATE_IP_GEN			EXYNOS_CLKREG(0x10934)
#define EXYNOS5_CLKGATE_IP_FSYS			EXYNOS_CLKREG(0x10944)
#define EXYNOS5_CLKGATE_IP_GPS			EXYNOS_CLKREG(0x1094C)
#define EXYNOS5_CLKGATE_IP_PERIC		EXYNOS_CLKREG(0x10950)
#define EXYNOS5_CLKGATE_IP_PERIS		EXYNOS_CLKREG(0x10960)
#define EXYNOS5_CLKGATE_BLOCK			EXYNOS_CLKREG(0x10980)

#define EXYNOS5_BPLL_CON0			EXYNOS_CLKREG(0x20110)
#define EXYNOS5_CLKSRC_CDREX			EXYNOS_CLKREG(0x20200)
#define EXYNOS5_CLKDIV_CDREX			EXYNOS_CLKREG(0x20500)

#define EXYNOS5_PLL_DIV2_SEL			EXYNOS_CLKREG(0x20A24)

#define EXYNOS5_EPLL_LOCK			EXYNOS_CLKREG(0x10030)

#define EXYNOS5_EPLLCON0_LOCKED_SHIFT		(29)

/* Compatibility defines and inclusion */

#include <exynos/regs-pmu.h>

#define S5P_EPLL_CON				EXYNOS4_EPLL_CON0

#endif /* __ASM_ARCH_REGS_CLOCK_H */
