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
 * @file cpu_vcpu_cp15.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief VCPU CP15 Emulation
 * @details This source file implements CP15 coprocessor for each VCPU.
 *
 * The Translation table walk and CP15 register read/write has been 
 * largely adapted from QEMU 0.14.xx targe-arm/helper.c source file
 * which is licensed under GPL.
 */

#include <vmm_heap.h>
#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_devemu.h>
#include <vmm_scheduler.h>
#include <vmm_guest_aspace.h>
#include <vmm_vcpu_irq.h>
#include <cpu_mmu.h>
#include <cpu_cache.h>
#include <cpu_barrier.h>
#include <cpu_inline_asm.h>
#include <cpu_vcpu_helper.h>
#include <cpu_vcpu_emulate.h>
#include <cpu_vcpu_cp15.h>

static int cpu_vcpu_cp15_stage2_map(struct vmm_vcpu * vcpu, 
				    arch_regs_t * regs,
				    physical_addr_t fipa)
{
	int rc;
	u32 reg_flags = 0x0;
	struct cpu_page pg;
	physical_size_t availsz;

	vmm_memset(&pg, 0, sizeof(pg));

	pg.ia = fipa & TTBL_L3_MAP_MASK;
	pg.sz = TTBL_L3_BLOCK_SIZE;

	rc = vmm_guest_physical_map(vcpu->guest, pg.ia, pg.sz, 
				    &pg.oa, &availsz, &reg_flags);
	if (rc) {
		return rc;
	}

	if (availsz < TTBL_L3_BLOCK_SIZE) {
		return VMM_EFAIL;
	}

	if (reg_flags & VMM_REGION_VIRTUAL) {
		pg.af = 0;
		pg.ap = TTBL_HAP_NOACCESS;
	} else if (reg_flags & VMM_REGION_READONLY) {
		pg.af = 1;
		pg.ap = TTBL_HAP_READONLY;
	} else {
		pg.af = 1;
		pg.ap = TTBL_HAP_READWRITE;
	}

	if (reg_flags & VMM_REGION_ISRAM) {
		pg.memattr = 0xf;
	}

	/* FIXME: Handle Cacheable Regions
	 * if (reg_flags & VMM_REGION_CACHEABLE) {
	 * }
	 */

	/* FIXME: Handle Cacheable Regions
	 * if (reg_flags & VMM_REGION_BUFFERABLE) {
	 * }
	 */

	rc = cpu_mmu_map_page(arm_priv(vcpu)->cp15.ttbl, &pg);
	if (rc) {
		return rc;
	}

	return VMM_OK;
}

int cpu_vcpu_cp15_inst_abort(struct vmm_vcpu * vcpu, 
			     arch_regs_t * regs,
			     u32 il, u32 iss, 
			     physical_addr_t fipa)
{
	switch (iss & ISS_ABORT_FSR_MASK) {
	case FSR_TRANS_FAULT_LEVEL1:
	case FSR_TRANS_FAULT_LEVEL2:
	case FSR_TRANS_FAULT_LEVEL3:
		return cpu_vcpu_cp15_stage2_map(vcpu, regs, fipa);
	default:
		break;
	};

	return VMM_EFAIL;
}

int cpu_vcpu_cp15_data_abort(struct vmm_vcpu * vcpu, 
			     arch_regs_t * regs,
			     u32 il, u32 iss, 
			     physical_addr_t fipa)
{
	switch (iss & ISS_ABORT_FSR_MASK) {
	case FSR_TRANS_FAULT_LEVEL1:
	case FSR_TRANS_FAULT_LEVEL2:
	case FSR_TRANS_FAULT_LEVEL3:
		return cpu_vcpu_cp15_stage2_map(vcpu, regs, fipa);
	case FSR_ACCESS_FAULT_LEVEL1:
	case FSR_ACCESS_FAULT_LEVEL2:
	case FSR_ACCESS_FAULT_LEVEL3:
		if (!(iss & ISS_ABORT_ISV_MASK)) {
			return VMM_EFAIL;
		}
		if (iss & ISS_ABORT_WNR_MASK) {
			return cpu_vcpu_emulate_store(vcpu, regs, 
						      il, iss, fipa);
		} else {
			return cpu_vcpu_emulate_load(vcpu, regs, 
						     il, iss, fipa);
		}
	default:
		break;
	};

	return VMM_EFAIL;
}


bool cpu_vcpu_cp15_read(struct vmm_vcpu * vcpu, 
			arch_regs_t *regs,
			u32 opc1, u32 opc2, u32 CRn, u32 CRm, 
			u32 *data)
{
	*data = 0x0;
	switch (CRn) {
	case 0:		/* ID codes.  */
		switch (opc1) {
		case 0:
			switch (CRm) {
			case 0:
				switch (opc2) {
				case 0:	/* Device ID.  */
					*data = arm_priv(vcpu)->cp15.c0_cpuid;
					break;
				case 1:	/* Cache Type.  */
					*data =
					    arm_priv(vcpu)->cp15.c0_cachetype;
					break;
				case 2:	/* TCM status.  */
					*data = 0;
					break;
				case 3:	/* TLB type register.  */
					*data = 0;	/* No lockable TLB entries.  */
					break;
				case 5:	/* MPIDR */
					/* The MPIDR was standardised in v7; prior to
					 * this it was implemented only in the 11MPCore.
					 * For all other pre-v7 cores it does not exist.
					 */
					if (arm_feature(vcpu, ARM_FEATURE_V7) ||
					    arm_cpuid(vcpu) ==
					    ARM_CPUID_ARM11MPCORE) {
						int mpidr = vcpu->subid;
						/* We don't support setting cluster ID ([8..11])
						 * so these bits always RAZ.
						 */
						if (arm_feature
						    (vcpu, ARM_FEATURE_V7MP)) {
							mpidr |= (1 << 31);
							/* Cores which are uniprocessor (non-coherent)
							 * but still implement the MP extensions set
							 * bit 30. (For instance, A9UP.) However we do
							 * not currently model any of those cores.
							 */
						}
						*data = mpidr;
					}
					/* otherwise fall through to the unimplemented-reg case */
					break;
				default:
					goto bad_reg;
				}
				break;
			case 1:
				if (!arm_feature(vcpu, ARM_FEATURE_V6))
					goto bad_reg;
				*data = arm_priv(vcpu)->cp15.c0_c1[opc2];
				break;
			case 2:
				if (!arm_feature(vcpu, ARM_FEATURE_V6))
					goto bad_reg;
				*data = arm_priv(vcpu)->cp15.c0_c2[opc2];
				break;
			case 3:
			case 4:
			case 5:
			case 6:
			case 7:
				*data = 0;
				break;
			default:
				goto bad_reg;
			}
			break;
		case 1:
			/* These registers aren't documented on arm11 cores.  However
			 * Linux looks at them anyway.  */
			if (!arm_feature(vcpu, ARM_FEATURE_V6))
				goto bad_reg;
			if (CRm != 0)
				goto bad_reg;
			if (!arm_feature(vcpu, ARM_FEATURE_V7)) {
				*data = 0;
				break;
			}
			switch (opc2) {
			case 0:
				*data =
				    arm_priv(vcpu)->cp15.
				    c0_ccsid[arm_priv(vcpu)->cp15.c0_cssel];
				break;
			case 1:
				*data = arm_priv(vcpu)->cp15.c0_clid;
				break;
			case 7:
				*data = 0;
				break;
			default:
				goto bad_reg;
			}
			break;
		case 2:
			if (opc2 != 0 || CRm != 0)
				goto bad_reg;
			*data = arm_priv(vcpu)->cp15.c0_cssel;
			break;
		default:
			goto bad_reg;
		};
		break;
	case 1: /* System configuration.  */
		switch (opc2) {
		case 0: /* Control register.  */
			*data = arm_priv(vcpu)->cp15.c1_sctlr;
			break;
		case 1: /* Auxiliary control register.  */
			if (!arm_feature(vcpu, ARM_FEATURE_AUXCR))
				goto bad_reg;
			switch (arm_cpuid(vcpu)) {
			case ARM_CPUID_ARM1026:
				*data = 1;
				break;
			case ARM_CPUID_ARM1136:
			case ARM_CPUID_ARM1136_R2:
				*data = 7;
				break;
			case ARM_CPUID_ARM11MPCORE:
				*data = 1;
				break;
			case ARM_CPUID_CORTEXA8:
				*data = 2;
				break;
			case ARM_CPUID_CORTEXA9:
				*data = 0;
				break;
			default:
				goto bad_reg;
			}
			break;
		case 2: /* Coprocessor access register.  */
			*data = arm_priv(vcpu)->cp15.c1_cpacr;
			break;
		default:
			goto bad_reg;
		};
		break;
	}
	return TRUE;
bad_reg:
	return FALSE;
}

bool cpu_vcpu_cp15_write(struct vmm_vcpu * vcpu, 
			 arch_regs_t *regs,
			 u32 opc1, u32 opc2, u32 CRn, u32 CRm, 
			 u32 data)
{
	switch (CRn) {
	case 0:
		/* ID codes.  */
		if (arm_feature(vcpu, ARM_FEATURE_V7) &&
		    (opc1 == 2) && (CRm == 0) && (opc2 == 0)) {
			arm_priv(vcpu)->cp15.c0_cssel = data & 0xf;
			break;
		}
		goto bad_reg;
	case 1: /* System configuration.  */
		switch (opc2) {
		case 0:
			arm_priv(vcpu)->cp15.c1_sctlr = data;
			write_sctlr(data & ~(SCTLR_A_MASK));
			break;
		case 1: /* Auxiliary control register.  */
			/* Not implemented.  */
			break;
		case 2:
			arm_priv(vcpu)->cp15.c1_cpacr = data;
			write_cpacr(data);
			break;
		default:
			goto bad_reg;
		};
		break;
	}
	return TRUE;
bad_reg:
	return FALSE;
}

void cpu_vcpu_cp15_switch_context(struct vmm_vcpu * tvcpu, 
				  struct vmm_vcpu * vcpu)
{
	if (tvcpu && tvcpu->is_normal) {
		arm_priv(tvcpu)->cp15.c2_ttbr0 = read_ttbr0();
		arm_priv(tvcpu)->cp15.c2_ttbr1 = read_ttbr1();
		arm_priv(tvcpu)->cp15.c2_ttbcr = read_ttbcr();
		arm_priv(tvcpu)->cp15.c3_dacr = read_dacr();
		arm_priv(tvcpu)->cp15.c5_ifsr = read_ifsr();
		arm_priv(tvcpu)->cp15.c5_dfsr = read_dfsr();
		arm_priv(tvcpu)->cp15.c6_ifar = read_ifar();
		arm_priv(tvcpu)->cp15.c6_dfar = read_dfar();
		arm_priv(tvcpu)->cp15.c10_prrr = read_prrr();
		arm_priv(tvcpu)->cp15.c10_nmrr = read_nmrr();
		arm_priv(tvcpu)->cp15.c13_fcseidr = read_fcseidr();
		arm_priv(tvcpu)->cp15.c13_contextidr = read_contextidr();
		arm_priv(tvcpu)->cp15.c13_tls1 = read_tpidrurw();
		arm_priv(tvcpu)->cp15.c13_tls2 = read_tpidruro();
		arm_priv(tvcpu)->cp15.c13_tls3 = read_tpidrprw();
	}
	if (vcpu->is_normal) {
		cpu_mmu_stage2_chttbl(vcpu->id, arm_priv(vcpu)->cp15.ttbl);
		write_vpidr(arm_priv(vcpu)->cp15.c0_cpuid);
		write_vmpidr(vcpu->subid);
		write_sctlr(arm_priv(vcpu)->cp15.c1_sctlr & ~(SCTLR_A_MASK));
		write_cpacr(arm_priv(vcpu)->cp15.c1_cpacr);
		write_ttbr0(arm_priv(vcpu)->cp15.c2_ttbr0);
		write_ttbr1(arm_priv(vcpu)->cp15.c2_ttbr1);
		write_ttbcr(arm_priv(vcpu)->cp15.c2_ttbcr);
		write_dacr(arm_priv(vcpu)->cp15.c3_dacr);
		write_ifsr(arm_priv(vcpu)->cp15.c5_ifsr);
		write_dfsr(arm_priv(vcpu)->cp15.c5_dfsr);
		write_ifar(arm_priv(vcpu)->cp15.c6_ifar);
		write_dfar(arm_priv(vcpu)->cp15.c6_dfar);
		write_prrr(arm_priv(vcpu)->cp15.c10_prrr);
		write_nmrr(arm_priv(vcpu)->cp15.c10_nmrr);
		write_fcseidr(arm_priv(vcpu)->cp15.c13_fcseidr);
		write_contextidr(arm_priv(vcpu)->cp15.c13_contextidr);
		write_tpidrurw(arm_priv(vcpu)->cp15.c13_tls1);
		write_tpidruro(arm_priv(vcpu)->cp15.c13_tls2);
		write_tpidrprw(arm_priv(vcpu)->cp15.c13_tls3);
	}
}

static u32 cortexa9_cp15_c0_c1[8] =
{ 0x1031, 0x11, 0x000, 0, 0x00100103, 0x20000000, 0x01230000, 0x00002111 };

static u32 cortexa9_cp15_c0_c2[8] =
{ 0x00101111, 0x13112111, 0x21232041, 0x11112131, 0x00111142, 0, 0, 0 };

static u32 cortexa8_cp15_c0_c1[8] =
{ 0x1031, 0x11, 0x400, 0, 0x31100003, 0x20000000, 0x01202000, 0x11 };

static u32 cortexa8_cp15_c0_c2[8] =
{ 0x00101111, 0x12112111, 0x21232031, 0x11112131, 0x00111142, 0, 0, 0 };

int cpu_vcpu_cp15_init(struct vmm_vcpu * vcpu, u32 cpuid)
{
	int rc = VMM_OK;

	if (!vcpu->reset_count) {
		vmm_memset(&arm_priv(vcpu)->cp15, 0, sizeof(arm_priv(vcpu)->cp15));
		arm_priv(vcpu)->cp15.ttbl = cpu_mmu_ttbl_alloc(TTBL_STAGE2);
	}

	arm_priv(vcpu)->cp15.c0_cpuid = cpuid;
	arm_priv(vcpu)->cp15.c2_ttbcr = 0x0;
	arm_priv(vcpu)->cp15.c9_pmcr = (cpuid & 0xFF000000);
	/* Reset values of important registers */
	switch (cpuid) {
	case ARM_CPUID_CORTEXA8:
		vmm_memcpy(arm_priv(vcpu)->cp15.c0_c1, cortexa8_cp15_c0_c1, 
							8 * sizeof(u32));
		vmm_memcpy(arm_priv(vcpu)->cp15.c0_c2, cortexa8_cp15_c0_c2, 
							8 * sizeof(u32));
		arm_priv(vcpu)->cp15.c0_cachetype = 0x82048004;
		arm_priv(vcpu)->cp15.c0_clid = (1 << 27) | (2 << 24) | 3;
		arm_priv(vcpu)->cp15.c0_ccsid[0] = 0xe007e01a; /* 16k L1 dcache. */
		arm_priv(vcpu)->cp15.c0_ccsid[1] = 0x2007e01a; /* 16k L1 icache. */
		arm_priv(vcpu)->cp15.c0_ccsid[2] = 0xf0000000; /* No L2 icache. */
		arm_priv(vcpu)->cp15.c1_sctlr = 0x00c50078;
		break;
	case ARM_CPUID_CORTEXA9:
		vmm_memcpy(arm_priv(vcpu)->cp15.c0_c1, cortexa9_cp15_c0_c1, 
							8 * sizeof(u32));
		vmm_memcpy(arm_priv(vcpu)->cp15.c0_c2, cortexa9_cp15_c0_c2, 
							8 * sizeof(u32));
		arm_priv(vcpu)->cp15.c0_cachetype = 0x80038003;
		arm_priv(vcpu)->cp15.c0_clid = (1 << 27) | (1 << 24) | 3;
		arm_priv(vcpu)->cp15.c0_ccsid[0] = 0xe00fe015; /* 16k L1 dcache. */
		arm_priv(vcpu)->cp15.c0_ccsid[1] = 0x200fe015; /* 16k L1 icache. */
		arm_priv(vcpu)->cp15.c1_sctlr = 0x00c50078;
		break;
	default:
		break;
	};

	return rc;
}

int cpu_vcpu_cp15_deinit(struct vmm_vcpu * vcpu)
{
	int rc;

	if ((rc = cpu_mmu_ttbl_free(arm_priv(vcpu)->cp15.ttbl))) {
		return rc;
	}

	vmm_memset(&arm_priv(vcpu)->cp15, 0, sizeof(arm_priv(vcpu)->cp15));

	return VMM_OK;
}

