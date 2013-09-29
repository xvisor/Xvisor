/**
 * Copyright (c) 2013 Sukanto Ghosh.
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
 * @file cpu_vcpu_emulate.c
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief Implementation of hardware assisted instruction emulation
 */

#include <vmm_types.h>
#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_scheduler.h>
#include <vmm_vcpu_irq.h>
#include <vmm_host_aspace.h>
#include <vmm_devemu.h>
#include <cpu_inline_asm.h>
#include <cpu_vcpu_helper.h>
#include <cpu_vcpu_spr.h>
#include <cpu_vcpu_emulate.h>
#include <arm_features.h>
#include <generic_timer.h>
#include <emulate_psci.h>

/**
 * A conditional instruction can trap, even though its condition was
 * FALSE. Hence emulate condition checking in software!
 */
static bool cpu_vcpu_condition_check(struct vmm_vcpu *vcpu, 
				     arch_regs_t *regs,
				     u32 iss)
{
	bool ret = FALSE;
	u32 cond;

	/* Condition check parameters */
	if (iss & ISS_CV_MASK) {
		cond = (iss & ISS_COND_MASK) >> ISS_COND_SHIFT;
	} else {
		/* This can happen in Thumb mode: examine IT state. */
		u32 it;

		it = ((regs->pstate >> 8) & 0xFC) | ((regs->pstate >> 25) & 0x3);

		/* it == 0 => unconditional. */
		if (it == 0)
                       return TRUE;

               /* The cond for this insn works out as the top 4 bits. */
               cond = (it >> 4);
	}

	/* Emulate condition checking */
	switch (cond >> 1) {
	case 0:
		ret = (regs->pstate & PSR_ZERO_MASK) ? TRUE : FALSE;
		break;
	case 1:
		ret = (regs->pstate & PSR_CARRY_MASK) ? TRUE : FALSE;
		break;
	case 2:
		ret = (regs->pstate & PSR_NEGATIVE_MASK) ? TRUE : FALSE;
		break;
	case 3:
		ret = (regs->pstate & PSR_OVERFLOW_MASK) ? TRUE : FALSE;
		break;
	case 4:
		ret = (regs->pstate & PSR_CARRY_MASK) ? TRUE : FALSE;
		ret = (ret && !(regs->pstate & PSR_ZERO_MASK)) ? 
								TRUE : FALSE;
		break;
	case 5:
		if (regs->pstate & PSR_NEGATIVE_MASK) {
			ret = (regs->pstate & PSR_OVERFLOW_MASK) ? 
								TRUE : FALSE;
		} else {
			ret = (regs->pstate & PSR_OVERFLOW_MASK) ? 
								FALSE : TRUE;
		}
		break;
	case 6:
		if (regs->pstate & PSR_NEGATIVE_MASK) {
			ret = (regs->pstate & PSR_OVERFLOW_MASK) ? 
								TRUE : FALSE;
		} else {
			ret = (regs->pstate & PSR_OVERFLOW_MASK) ? 
								FALSE : TRUE;
		}
		ret = (ret && !(regs->pstate & PSR_ZERO_MASK)) ? 
								TRUE : FALSE;
		break;
	case 7:
		ret = TRUE;
		break;
	default:
		break;
	};

	if ((cond & 0x1) && (cond != 0xF)) {
		ret = !ret;
	}

	return ret;
}

/**
 * Update ITSTATE when emulating instructions inside an IT-block
 *
 * When IO abort occurs from Thumb IF-THEN blocks, the ITSTATE field 
 * of the CPSR is not updated, so we do this manually.
 */
static void cpu_vcpu_update_itstate(struct vmm_vcpu *vcpu, 
				    arch_regs_t *regs)
{
	u32 itbits, cond;

	if (!(regs->pstate & PSR_IT_MASK)) {
		return;
	}

	cond = (regs->pstate & 0xe000) >> 13;
	itbits = (regs->pstate & 0x1c00) >> (10 - 2);
	itbits |= (regs->pstate & (0x3 << 25)) >> 25;

	/* Perform ITAdvance (see page A-52 in ARM DDI 0406C) */
	if ((itbits & 0x7) == 0)
		itbits = cond = 0;
	else
		itbits = (itbits << 1) & 0x1f;

	regs->pstate &= ~PSR_IT_MASK;
	regs->pstate |= cond << 13;
	regs->pstate |= (itbits & 0x1c) << (10 - 2);
	regs->pstate |= (itbits & 0x3) << 25;
}

int cpu_vcpu_emulate_wfi_wfe(struct vmm_vcpu *vcpu, 
			     arch_regs_t *regs,
			     u32 il, u32 iss)
{
	u64 timeout_nsecs = 0;

	/* Check instruction condition */
	if (!cpu_vcpu_condition_check(vcpu, regs, iss)) {
		/* Skip this instruction */
		goto done;
	}

	if (iss & ISS_WFI_WFE_TRAPPED_MASK) { /* WFE */
		/* Yield current timeslice */
		vmm_scheduler_yield();
	} else { /* WFI */
		/* Estimate wakeup timeout if possible */
		if (arm_feature(vcpu, ARM_FEATURE_GENERIC_TIMER)) {
			timeout_nsecs = generic_timer_wakeup_timeout();
		}

		/* Wait for irq on this vcpu */
		vmm_vcpu_irq_wait_timeout(vcpu, timeout_nsecs);
	}

done:
	/* Next instruction */
	regs->pc += (il) ? 4 : 2;

	/* Update ITSTATE for Thumb mode */
	if (regs->pstate & PSR_THUMB_ENABLED) {
		cpu_vcpu_update_itstate(vcpu, regs);
	}

	return VMM_OK;
}

int cpu_vcpu_emulate_mcr_mrc_cp15(struct vmm_vcpu *vcpu, 
				  arch_regs_t *regs, 
				  u32 il, u32 iss)
{
	int rc = VMM_OK;
	u32 opc2, opc1, CRn, Rt, CRm;
	u64 t;

	/* Check instruction condition */
	if (!cpu_vcpu_condition_check(vcpu, regs, iss)) {
		/* Skip this instruction */
		goto done;
	}

	/* More MCR/MRC parameters */
	opc2 = (iss & ISS_MCR_MRC_OPC2_MASK) >> ISS_MCR_MRC_OPC2_SHIFT;
	opc1 = (iss & ISS_MCR_MRC_OPC1_MASK) >> ISS_MCR_MRC_OPC1_SHIFT;
	CRn  = (iss & ISS_MCR_MRC_CRN_MASK) >> ISS_MCR_MRC_CRN_SHIFT;
	Rt   = (iss & ISS_MCR_MRC_RT_MASK) >> ISS_MCR_MRC_RT_SHIFT;
	CRm  = (iss & ISS_MCR_MRC_CRM_MASK) >> ISS_MCR_MRC_CRM_SHIFT;

	if (iss & ISS_MCR_MRC_DIR_MASK) {
		/* MRC CP15 */
		if (!cpu_vcpu_cp15_read(vcpu, regs, opc1, opc2, CRn, CRm, &t)) {
			rc = VMM_EFAIL;
		}
		if (!rc) {
			cpu_vcpu_reg64_write(vcpu, regs, Rt, t);
		}
	} else {
		/* MCR CP15 */
		t = cpu_vcpu_reg64_read(vcpu, regs, Rt);
		if (!cpu_vcpu_cp15_write(vcpu, regs, opc1, opc2, CRn, CRm, t)) {
			rc = VMM_EFAIL;
		}
	}

done:
	if (!rc) {
		/* Next instruction */
		regs->pc += (il) ? 4 : 2;
		/* Update ITSTATE for Thumb mode */
		if (regs->pstate & PSR_THUMB_ENABLED) {
			cpu_vcpu_update_itstate(vcpu, regs);
		}
	}

	return rc;
}

/* TODO: To be implemented later */
int cpu_vcpu_emulate_mcrr_mrrc_cp15(struct vmm_vcpu *vcpu, 
				    arch_regs_t *regs, 
				    u32 il, u32 iss)
{
	return VMM_EFAIL;
}

/* TODO: To be implemented later */
int cpu_vcpu_emulate_mcr_mrc_cp14(struct vmm_vcpu *vcpu, 
				  arch_regs_t * regs, 
				  u32 il, u32 iss)
{
	u32 Rt;

	/* Check instruction condition */
	if (!cpu_vcpu_condition_check(vcpu, regs, iss)) {
		/* Skip this instruction */
		goto done;
	}

	/* More MCR/MRC parameters */
	Rt   = (iss & ISS_MCR_MRC_RT_MASK) >> ISS_MCR_MRC_RT_SHIFT;

	if (iss & ISS_MCR_MRC_DIR_MASK) {
		/* MRC CP14 */
		/* Read always zero. */
		cpu_vcpu_reg64_write(vcpu, regs, Rt, 0x0);
	} else {
		/* MCR CP14 */
		/* Ignore it. */
	}

done:
	/* Next instruction */
	regs->pc += (il) ? 4 : 2;
	/* Update ITSTATE for Thumb mode */
	if (regs->pstate & PSR_THUMB_ENABLED) {
		cpu_vcpu_update_itstate(vcpu, regs);
	}

	return VMM_OK;
}

/* TODO: To be implemented later */
int cpu_vcpu_emulate_ldc_stc_cp14(struct vmm_vcpu *vcpu, 
				  arch_regs_t *regs, 
				  u32 il, u32 iss)
{
	return VMM_EFAIL;
}

int cpu_vcpu_emulate_simd_fp_regs(struct vmm_vcpu *vcpu, 
				  arch_regs_t *regs, 
				  u32 il, u32 iss)
{
	msr_sync(cptr_el2, CPTR_TTA_MASK | CPTR_RES1_MASK);
	/* TFA bit should be removed before we try to restore the FP-state */
	vfp_simd_restore_regs(arm_priv(vcpu)->fpregs);
	msr(fpexc32_el2, arm_priv(vcpu)->fpexc);
	msr(fpcr, arm_priv(vcpu)->fpcr);
	msr(fpsr, arm_priv(vcpu)->fpsr);
	return VMM_OK;
}

/* TODO: Can we optimize this by determining the source and target registers
 * and avoid copying all the FPU-regs  */
int cpu_vcpu_emulate_vmrs(struct vmm_vcpu *vcpu, 
			  arch_regs_t *regs, 
			  u32 il, u32 iss)
{
	vmm_printf("\n\n%s\n", __func__);
	msr(cptr_el2, CPTR_TTA_MASK | CPTR_RES1_MASK);
	/* TFA bit should be removed before we try to restore the FP-state */
	vfp_simd_restore_regs(arm_priv(vcpu)->fpregs);
	msr(fpexc32_el2, arm_priv(vcpu)->fpexc);
	msr(fpcr, arm_priv(vcpu)->fpcr);
	msr(fpsr, arm_priv(vcpu)->fpsr);
	return VMM_OK;
}

/* TODO: To be implemented later */
int cpu_vcpu_emulate_mcrr_mrrc_cp14(struct vmm_vcpu *vcpu, 
				    arch_regs_t *regs, 
				    u32 il, u32 iss)
{
	return VMM_EFAIL;
}

static int do_psci_call(struct vmm_vcpu *vcpu, 
			arch_regs_t *regs, 
			u32 il, u32 iss,
			bool is_smc)
{
	int rc;

	/* Treat this as PSCI call and emulate it */
	rc = emulate_psci_call(vcpu, regs, is_smc);
	if (rc) {
		/* Inject undefined exception */
		cpu_vcpu_inject_undef(vcpu, regs);
	}

	return rc;
}

int cpu_vcpu_emulate_hvc32(struct vmm_vcpu *vcpu, 
			   arch_regs_t *regs, 
			   u32 il, u32 iss)
{
	return do_psci_call(vcpu, regs, il, iss, FALSE);
}

int cpu_vcpu_emulate_hvc64(struct vmm_vcpu *vcpu, 
			   arch_regs_t *regs, 
			   u32 il, u32 iss)
{
	return do_psci_call(vcpu, regs, il, iss, FALSE);
}

int cpu_vcpu_emulate_smc32(struct vmm_vcpu *vcpu, 
			   arch_regs_t *regs, 
			   u32 il, u32 iss)
{
	return do_psci_call(vcpu, regs, il, iss, TRUE);
}

int cpu_vcpu_emulate_smc64(struct vmm_vcpu *vcpu, 
			   arch_regs_t *regs, 
			   u32 il, u32 iss)
{
	return do_psci_call(vcpu, regs, il, iss, TRUE);
}

/* TODO: To be implemented later */
int cpu_vcpu_emulate_msr_mrs_system(struct vmm_vcpu *vcpu, 
				    arch_regs_t *regs, 
				    u32 il, u32 iss)
{
	bool read = (!!(iss & ISS_SYSREG_READ));
	int rt = ((iss & ISS_RT_MASK) >> ISS_RT_SHIFT);

	switch (iss & ISS_SYSREG_MASK) {
		case ISS_CPACR_EL1:
			vmm_printf("Trying to access CPACR_EL1\n");
			break;
		case ISS_CNTFRQ_EL0:
			vmm_printf("Trying to access CNTFRQ_EL0  \n");
			break;
		case ISS_CNTPCT_EL0:
			vmm_printf("Trying to access CNTPCT_EL0  \n");
			break;
		case ISS_CNTKCTL_EL1:
			vmm_printf("Trying to access CNTKCTL_EL1  \n");
			break;
		case ISS_CNTP_TVAL_EL0:
			vmm_printf("Trying to access CNTP_TVAL_EL0  \n");
			break;
		case ISS_CNTP_CTL_EL0:
			vmm_printf("Trying to access CNTP_CTL_EL0  \n");
			break;
		case ISS_CNTP_CVAL_EL0:
			vmm_printf("Trying to access CNTP_CVAL_EL0  \n");
			break;
		case ISS_ACTLR_EL1:
			if (read) {
				cpu_vcpu_reg64_write(vcpu, regs, rt, arm_priv(vcpu)->actlr);
			} else {
				arm_priv(vcpu)->actlr = cpu_vcpu_reg64_read(vcpu, regs, rt);
			}
			regs->pc += 4;
			return VMM_OK;
			break;
		default:
			vmm_printf("Guest MSR/MRS Emulation @ PC:0x%X\n", regs->pc);
			break;
	}
	return VMM_EFAIL;
}

static inline u32 arm_sign_extend(u32 imm, u32 len, u32 bits)
{
	if (imm & (1 << (len - 1))) {
		imm = imm | (~((1 << len) - 1));
	}
	return imm & ((1 << bits) - 1);
}

int cpu_vcpu_emulate_load(struct vmm_vcpu *vcpu, 
			  arch_regs_t *regs,
			  u32 il, u32 iss,
			  physical_addr_t ipa)
{
	int rc;
	u8 data8;
	u16 data16;
	u32 data32, sas, sse, srt;

	sas = (iss & ISS_ABORT_SAS_MASK) >> ISS_ABORT_SAS_SHIFT;
	sse = (iss & ISS_ABORT_SSE_MASK) >> ISS_ABORT_SSE_SHIFT;
	srt = (iss & ISS_ABORT_SRT_MASK) >> ISS_ABORT_SRT_SHIFT;

	sse = (sas == 2) ? 0 : sse;

	switch (sas) {
	case 0:
		rc = vmm_devemu_emulate_read(vcpu, ipa, 
					     &data8, sizeof(data8));
		if (!rc) {
			if (sse) {
				cpu_vcpu_reg64_write(vcpu, regs, srt, 
					arm_sign_extend(data8, 8, 32));
			} else {
				cpu_vcpu_reg64_write(vcpu, regs, srt, data8);
			}
		}
		break;
	case 1:
		rc = vmm_devemu_emulate_read(vcpu, ipa, 
					     &data16, sizeof(data16));
		if (!rc) {
			if (sse) {
				cpu_vcpu_reg64_write(vcpu, regs, srt, 
					arm_sign_extend(data16, 16, 32));
			} else {
				cpu_vcpu_reg64_write(vcpu, regs, srt, data16);
			}
		}
		break;
	case 2:
		rc = vmm_devemu_emulate_read(vcpu, ipa, 
					     &data32, sizeof(data32));
		if (!rc) {
			cpu_vcpu_reg64_write(vcpu, regs, srt, data32);
		}
		break;
	default:
		rc = VMM_EFAIL;
		break;
	};

	if (!rc) {
		/* Next instruction */
		regs->pc += (il) ? 4 : 2;
		/* Update ITSTATE for Thumb mode */
		if (regs->pstate & PSR_THUMB_ENABLED) {
			cpu_vcpu_update_itstate(vcpu, regs);
		}
	}

	return rc;
}

int cpu_vcpu_emulate_store(struct vmm_vcpu *vcpu, 
			   arch_regs_t *regs,
			   u32 il, u32 iss,
			   physical_addr_t ipa)
{
	int rc;
	u8 data8;
	u16 data16;
	u32 data32, sas, srt;

	sas = (iss & ISS_ABORT_SAS_MASK) >> ISS_ABORT_SAS_SHIFT;
	srt = (iss & ISS_ABORT_SRT_MASK) >> ISS_ABORT_SRT_SHIFT;

	switch (sas) {
	case 0:
		data8 = cpu_vcpu_reg64_read(vcpu, regs, srt);
		rc = vmm_devemu_emulate_write(vcpu, ipa, 
					      &data8, sizeof(data8));
		break;
	case 1:
		data16 = cpu_vcpu_reg64_read(vcpu, regs, srt);
		rc = vmm_devemu_emulate_write(vcpu, ipa, 
					      &data16, sizeof(data16));
		break;
	case 2:
		data32 = cpu_vcpu_reg64_read(vcpu, regs, srt);
		rc = vmm_devemu_emulate_write(vcpu, ipa, 
					      &data32, sizeof(data32));
		break;
	default:
		rc = VMM_EFAIL;
		break;
	};

	if (!rc) {
		/* Next instruction */
		regs->pc += (il) ? 4 : 2;
		/* Update ITSTATE for Thumb mode */
		if (regs->pstate & PSR_THUMB_ENABLED) {
			cpu_vcpu_update_itstate(vcpu, regs);
		}
	}

	return rc;
}

