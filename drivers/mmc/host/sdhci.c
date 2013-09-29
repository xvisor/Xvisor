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
 * @file sdhci.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Secure Digital Host Controller Interface driver framework
 *
 * The source has been largely adapted from u-boot:
 * drivers/mmc/sdhci.c
 *
 * Copyright 2011, Marvell Semiconductor Inc.
 * Lei Wen <leiwen@marvell.com>
 *
 * The original code is licensed under the GPL.
 */

#include <vmm_error.h>
#include <vmm_cache.h>
#include <vmm_delay.h>
#include <vmm_heap.h>
#include <vmm_host_aspace.h>
#include <vmm_host_irq.h>
#include <vmm_modules.h>
#include <libs/bitops.h>
#include <libs/stringlib.h>
#include <libs/mathlib.h>
#include <drv/mmc/sdhci.h>

#define MODULE_DESC			"SDHCI Driver"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(SDHCI_IPRIORITY)
#define	MODULE_INIT			sdhci_module_init
#define	MODULE_EXIT			sdhci_module_exit

static void sdhci_clear_set_irqs(struct sdhci_host *host, u32 clear, u32 set)
{
	u32 ier;

	ier = sdhci_readl(host, SDHCI_INT_ENABLE);
	ier &= ~clear;
	ier |= set;
	sdhci_writel(host, ier, SDHCI_INT_ENABLE);

	ier = sdhci_readl(host, SDHCI_SIGNAL_ENABLE);
	ier &= ~clear;
	ier |= set;
	sdhci_writel(host, ier, SDHCI_SIGNAL_ENABLE);
}

static void sdhci_unmask_irqs(struct sdhci_host *host, u32 irqs)
{
	sdhci_clear_set_irqs(host, 0, irqs);
}

static void sdhci_mask_irqs(struct sdhci_host *host, u32 irqs)
{
	sdhci_clear_set_irqs(host, irqs, 0);
}

static void sdhci_set_card_detection(struct sdhci_host *host, bool enable)
{
	u32 present, irqs;

	if (host->quirks & SDHCI_QUIRK_BROKEN_CARD_DETECTION) {
		return;
	}

	present = sdhci_readl(host, SDHCI_PRESENT_STATE) & SDHCI_CARD_PRESENT;
	irqs = present ? SDHCI_INT_CARD_REMOVE : SDHCI_INT_CARD_INSERT;

	if (enable) {
		sdhci_unmask_irqs(host, irqs);
	} else {
		sdhci_mask_irqs(host, irqs);
	}
}

static void sdhci_enable_card_detection(struct sdhci_host *host)
{
	sdhci_set_card_detection(host, TRUE);
}

#if 0
static void sdhci_disable_card_detection(struct sdhci_host *host)
{
	sdhci_set_card_detection(host, FALSE);
}
#endif

static void sdhci_reset(struct sdhci_host *host, u8 mask)
{
	u32 timeout;

	if (host->quirks & SDHCI_QUIRK_NO_CARD_NO_RESET) {
		if (!(sdhci_readl(host, SDHCI_PRESENT_STATE) &
			SDHCI_CARD_PRESENT))
			return;
	}

	/* Wait max 100 ms */
	timeout = 100;
	sdhci_writeb(host, mask, SDHCI_SOFTWARE_RESET);
	while (sdhci_readb(host, SDHCI_SOFTWARE_RESET) & mask) {
		if (timeout == 0) {
			vmm_printf("%s: Reset 0x%x never completed.\n", 
				   __func__, mask);
			return;
		}
		timeout--;
		vmm_udelay(1000);
	}
}

static void sdhci_init(struct sdhci_host *host, int soft)
{
	if (soft) {
		sdhci_reset(host, SDHCI_RESET_CMD|SDHCI_RESET_DATA);
	} else {
		sdhci_reset(host, SDHCI_RESET_ALL);
	}

	/* Enable only interrupts served by the SD controller */
	sdhci_writel(host, SDHCI_INT_DATA_MASK | SDHCI_INT_CMD_MASK,
		     SDHCI_INT_ENABLE);

	/* Mask all sdhci interrupt sources */
	sdhci_writel(host, 0x0, SDHCI_SIGNAL_ENABLE);
}

static void sdhci_cmd_done(struct sdhci_host *host, struct mmc_cmd *cmd)
{
	int i;
	if (cmd->resp_type & MMC_RSP_136) {
		/* CRC is stripped so we need to do some shifting. */
		for (i = 0; i < 4; i++) {
			cmd->response[i] = sdhci_readl(host,
					SDHCI_RESPONSE + (3-i)*4) << 8;
			if (i != 3)
				cmd->response[i] |= sdhci_readb(host,
						SDHCI_RESPONSE + (3-i)*4-1);
		}
	} else {
		cmd->response[0] = sdhci_readl(host, SDHCI_RESPONSE);
	}
}

static void sdhci_transfer_pio(struct sdhci_host *host, struct mmc_data *data)
{
	int i;
	char *offs;
	for (i = 0; i < data->blocksize; i += 4) {
		offs = data->dest + i;
		if (data->flags == MMC_DATA_READ) {
			*(u32 *)offs = sdhci_readl(host, SDHCI_BUFFER);
		} else {
			sdhci_writel(host, *(u32 *)offs, SDHCI_BUFFER);
		}
	}
}

static int sdhci_transfer_data(struct sdhci_host *host, 
				struct mmc_data *data,
				u32 start_addr)
{
	u32 ctrl, stat, rdy, mask, timeout, block = 0;

	if (host->sdhci_caps & SDHCI_CAN_DO_SDMA) {
		ctrl = sdhci_readl(host, SDHCI_HOST_CONTROL);
		ctrl &= ~SDHCI_CTRL_DMA_MASK;
		ctrl |= SDHCI_CTRL_SDMA;
		sdhci_writel(host, ctrl, SDHCI_HOST_CONTROL);
	}

	timeout = 1000000;
	rdy = SDHCI_INT_SPACE_AVAIL | SDHCI_INT_DATA_AVAIL;
	mask = SDHCI_DATA_AVAILABLE | SDHCI_SPACE_AVAILABLE;
	do {
		stat = sdhci_readl(host, SDHCI_INT_STATUS);
		if (stat & SDHCI_INT_ERROR) {
			vmm_printf("%s: Error detected in status(0x%X)!\n", 
				   __func__, stat);
			return VMM_EFAIL;
		}

		if (stat & rdy) {
			if (!(sdhci_readl(host, SDHCI_PRESENT_STATE) & mask)) {
				continue;
			}
			sdhci_writel(host, rdy, SDHCI_INT_STATUS);
			sdhci_transfer_pio(host, data);
			data->dest += data->blocksize;
			if (++block >= data->blocks) {
				break;
			}
		}

		if (host->sdhci_caps & SDHCI_CAN_DO_SDMA) {
			if (stat & SDHCI_INT_DMA_END) {
				sdhci_writel(host, SDHCI_INT_DMA_END, 
							SDHCI_INT_STATUS);
				start_addr &= 
					~(SDHCI_DEFAULT_BOUNDARY_SIZE - 1);
				start_addr += SDHCI_DEFAULT_BOUNDARY_SIZE;
				sdhci_writel(host, start_addr, 
							SDHCI_DMA_ADDRESS);
			}
		}

		if (timeout-- > 0) {
			vmm_udelay(10);
		} else {
			vmm_printf("%s: Transfer data timeout\n", __func__);
			return VMM_ETIMEDOUT;
		}
	} while (!(stat & SDHCI_INT_DATA_END));

	return VMM_OK;
}

int sdhci_send_command(struct mmc_host *mmc,
			struct mmc_cmd *cmd, 
			struct mmc_data *data)
{
	bool present;
	u32 mask, flags, mode;
	int ret = 0, trans_bytes = 0, is_aligned = 1;
	u32 timeout, retry = 10000, stat = 0, start_addr = 0;
	struct sdhci_host *host = mmc_priv(mmc);

	/* If polling, assume that the card is always present. */
	if (host->quirks & SDHCI_QUIRK_BROKEN_CARD_DETECTION) {
		present = TRUE;
	} else {
		present = sdhci_readl(host, SDHCI_PRESENT_STATE) &
				SDHCI_CARD_PRESENT;
	}

	/* If card not present then return error */
	if (!present) {
		return VMM_EIO;
	}

	/* Wait max 10 ms */
	timeout = 10;

	sdhci_writel(host, SDHCI_INT_ALL_MASK, SDHCI_INT_STATUS);
	mask = SDHCI_CMD_INHIBIT | SDHCI_DATA_INHIBIT;

	/* We shouldn't wait for data inihibit for stop commands, even
	   though they might use busy signaling */
	if (cmd->cmdidx == MMC_CMD_STOP_TRANSMISSION) {
		mask &= ~SDHCI_DATA_INHIBIT;
	}

	while (sdhci_readl(host, SDHCI_PRESENT_STATE) & mask) {
		if (timeout == 0) {
			vmm_printf("%s: Controller never released "
				   "inhibit bit(s).\n", __func__);
			return VMM_EIO;
		}
		timeout--;
		vmm_udelay(1000);
	}

	mask = SDHCI_INT_RESPONSE;
	if (!(cmd->resp_type & MMC_RSP_PRESENT)) {
		flags = SDHCI_CMD_RESP_NONE;
	} else if (cmd->resp_type & MMC_RSP_136) {
		flags = SDHCI_CMD_RESP_LONG;
	} else if (cmd->resp_type & MMC_RSP_BUSY) {
		flags = SDHCI_CMD_RESP_SHORT_BUSY;
		mask |= SDHCI_INT_DATA_END;
	} else {
		flags = SDHCI_CMD_RESP_SHORT;
	}

	if (cmd->resp_type & MMC_RSP_CRC) {
		flags |= SDHCI_CMD_CRC;
	}
	if (cmd->resp_type & MMC_RSP_OPCODE) {
		flags |= SDHCI_CMD_INDEX;
	}
	if (data) {
		flags |= SDHCI_CMD_DATA;
	}

	/* Set Transfer mode regarding to data flag */
	if (data != 0) {
		sdhci_writeb(host, 0xe, SDHCI_TIMEOUT_CONTROL);
		mode = SDHCI_TRNS_BLK_CNT_EN;
		trans_bytes = data->blocks * data->blocksize;
		if (data->blocks > 1) {
			mode |= SDHCI_TRNS_MULTI;
		}

		if (data->flags == MMC_DATA_READ) {
			mode |= SDHCI_TRNS_READ;
		}

		if (host->sdhci_caps & SDHCI_CAN_DO_SDMA) {
			if (data->flags == MMC_DATA_READ) {
				start_addr = (u32)data->dest;
			} else {
				start_addr = (u32)data->src;
			}

			if ((host->quirks & SDHCI_QUIRK_32BIT_DMA_ADDR) &&
			    (start_addr & 0x7) != 0x0) {
				is_aligned = 0;
				start_addr = (u32)host->aligned_buffer;
				if (data->flags != MMC_DATA_READ) {
					memcpy(host->aligned_buffer, 
						data->src, trans_bytes);
				}
			}

			sdhci_writel(host, start_addr, SDHCI_DMA_ADDRESS);
			mode |= SDHCI_TRNS_DMA;

			vmm_flush_cache_range(start_addr, 
						start_addr + trans_bytes);
		}

		sdhci_writew(host, SDHCI_MAKE_BLKSZ(SDHCI_DEFAULT_BOUNDARY_ARG,
				data->blocksize),
				SDHCI_BLOCK_SIZE);
		sdhci_writew(host, data->blocks, SDHCI_BLOCK_COUNT);
		sdhci_writew(host, mode, SDHCI_TRANSFER_MODE);
	}

	sdhci_writel(host, cmd->cmdarg, SDHCI_ARGUMENT);

	sdhci_writew(host, SDHCI_MAKE_CMD(cmd->cmdidx, flags), SDHCI_COMMAND);
	do {
		stat = sdhci_readl(host, SDHCI_INT_STATUS);
		if (stat & SDHCI_INT_ERROR) {
			break;
		}
		if (--retry == 0) {
			break;
		}
	} while ((stat & mask) != mask);

	if (retry == 0) {
		if (host->quirks & SDHCI_QUIRK_BROKEN_R1B)
			return VMM_OK;
		else {
			vmm_printf("%s: Status update timeout!\n", __func__);
			return VMM_ETIMEDOUT;
		}
	}

	if ((stat & (SDHCI_INT_ERROR | mask)) == mask) {
		sdhci_cmd_done(host, cmd);
		sdhci_writel(host, mask, SDHCI_INT_STATUS);
	} else {
		ret = VMM_EFAIL;
	}

	if (!ret && data) {
		ret = sdhci_transfer_data(host, data, start_addr);
	}

	if (host->quirks & SDHCI_QUIRK_WAIT_SEND_CMD) {
		vmm_udelay(1000);
	}

	stat = sdhci_readl(host, SDHCI_INT_STATUS);
	sdhci_writel(host, SDHCI_INT_ALL_MASK, SDHCI_INT_STATUS);
	if (!ret) {
		if ((host->quirks & SDHCI_QUIRK_32BIT_DMA_ADDR) &&
		     !is_aligned && (data->flags == MMC_DATA_READ)) {
			memcpy(data->dest, host->aligned_buffer, trans_bytes);
		}
		return VMM_OK;
	}

	sdhci_reset(host, SDHCI_RESET_CMD);
	sdhci_reset(host, SDHCI_RESET_DATA);

	if (stat & SDHCI_INT_TIMEOUT) {
		return VMM_ETIMEDOUT;
	} else {
		return VMM_EIO;
	}
}

static int sdhci_set_clock(struct mmc_host *mmc, u32 clock)
{
	struct sdhci_host *host = (struct sdhci_host *)mmc->priv;
	u32 div, clk, timeout;

	sdhci_writew(host, 0, SDHCI_CLOCK_CONTROL);

	if (clock == 0) {
		return VMM_OK;
	}

	if ((host->sdhci_version & SDHCI_SPEC_VER_MASK) >= SDHCI_SPEC_300) {
		/* Version 3.00 divisors must be a multiple of 2. */
		if (mmc->f_max <= clock)
			div = 1;
		else {
			for (div = 2; div < SDHCI_MAX_DIV_SPEC_300; div += 2) {
				if (udiv32(mmc->f_max, div) <= clock) {
					break;
				}
			}
		}
	} else {
		/* Version 2.00 divisors must be a power of 2. */
		for (div = 1; div < SDHCI_MAX_DIV_SPEC_200; div *= 2) {
			if (udiv32(mmc->f_max, div) <= clock) {
				break;
			}
		}
	}
	div >>= 1;

	if (host->ops.set_clock) {
		host->ops.set_clock(host, div);
	}

	clk = (div & SDHCI_DIV_MASK) << SDHCI_DIVIDER_SHIFT;
	clk |= ((div & SDHCI_DIV_HI_MASK) >> SDHCI_DIV_MASK_LEN)
		<< SDHCI_DIVIDER_HI_SHIFT;
	clk |= SDHCI_CLOCK_INT_EN;
	sdhci_writew(host, clk, SDHCI_CLOCK_CONTROL);

	/* Wait max 20 ms */
	timeout = 20;
	while (!((clk = sdhci_readw(host, SDHCI_CLOCK_CONTROL))
		& SDHCI_CLOCK_INT_STABLE)) {
		if (timeout == 0) {
			vmm_printf("%s: Internal clock never stabilised.\n", 
				   __func__);
			return VMM_EFAIL;
		}
		timeout--;
		vmm_udelay(1000);
	}

	clk |= SDHCI_CLOCK_CARD_EN;
	sdhci_writew(host, clk, SDHCI_CLOCK_CONTROL);

	return VMM_OK;
}

static void sdhci_set_power(struct sdhci_host *host, u16 power)
{
	u8 pwr = 0;

	if (power != 0xFFFF) {
		switch (1 << power) {
		case MMC_VDD_165_195:
			pwr = SDHCI_POWER_180;
			break;
		case MMC_VDD_29_30:
		case MMC_VDD_30_31:
			pwr = SDHCI_POWER_300;
			break;
		case MMC_VDD_32_33:
		case MMC_VDD_33_34:
			pwr = SDHCI_POWER_330;
			break;
		}
	}

	if (pwr == 0) {
		sdhci_writeb(host, 0, SDHCI_POWER_CONTROL);
		return;
	}

	if (host->quirks & SDHCI_QUIRK_NO_SIMULT_VDD_AND_POWER)
		sdhci_writeb(host, pwr, SDHCI_POWER_CONTROL);

	pwr |= SDHCI_POWER_ON;

	sdhci_writeb(host, pwr, SDHCI_POWER_CONTROL);
}

static void sdhci_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	u32 ctrl;
	struct sdhci_host *host = mmc_priv(mmc);

	if (host->ops.set_control_reg) {
		host->ops.set_control_reg(host);
	}

	if (ios->clock != host->clock) {
		sdhci_set_clock(mmc, ios->clock);
	}

	/* Set bus width */
	ctrl = sdhci_readb(host, SDHCI_HOST_CONTROL);
	if (ios->bus_width == 8) {
		ctrl &= ~SDHCI_CTRL_4BITBUS;
		if ((host->sdhci_version & SDHCI_SPEC_VER_MASK) >= 
							SDHCI_SPEC_300) {
			ctrl |= SDHCI_CTRL_8BITBUS;
		}
	} else {
		if ((host->sdhci_version & SDHCI_SPEC_VER_MASK) >= 
							SDHCI_SPEC_300) {
			ctrl &= ~SDHCI_CTRL_8BITBUS;
		}
		if (ios->bus_width == 4) {
			ctrl |= SDHCI_CTRL_4BITBUS;
		} else {
			ctrl &= ~SDHCI_CTRL_4BITBUS;
		}
	}

	if (ios->clock > 26000000) {
		ctrl |= SDHCI_CTRL_HISPD;
	} else {
		ctrl &= ~SDHCI_CTRL_HISPD;
	}

	if (host->quirks & SDHCI_QUIRK_NO_HISPD_BIT) {
		ctrl &= ~SDHCI_CTRL_HISPD;
	}

	sdhci_writeb(host, ctrl, SDHCI_HOST_CONTROL);
}

static int sdhci_init_card(struct mmc_host *mmc, struct mmc_card *card)
{
	struct sdhci_host *host = mmc_priv(mmc);

	sdhci_set_power(host, fls(mmc->voltages) - 1);

	if (host->quirks & SDHCI_QUIRK_BROKEN_CARD_DETECTION) {
		u32 status;

		sdhci_writel(host, SDHCI_CTRL_CD_TEST_INS | SDHCI_CTRL_CD_TEST,
			     SDHCI_HOST_CONTROL);

		status = sdhci_readl(host, SDHCI_PRESENT_STATE);
		while ((!(status & SDHCI_CARD_PRESENT)) ||
			(!(status & SDHCI_CARD_STATE_STABLE)) ||
			(!(status & SDHCI_CARD_DETECT_PIN_LEVEL))) {
			status = sdhci_readl(host, SDHCI_PRESENT_STATE);
		}
	}

	return VMM_OK;
}

static void sdhci_cmd_irq(struct sdhci_host *host, u32 intmask)
{
	/* Not used right now. */
}

static void sdhci_data_irq(struct sdhci_host *host, u32 intmask)
{
	/* Not used right now. */
}

static vmm_irq_return_t sdhci_irq_handler(int irq_no, void *dev)
{
	u32 intmask, unexpected = 0;
	vmm_irq_return_t result;
	struct sdhci_host *host = dev;

	intmask = sdhci_readl(host, SDHCI_INT_STATUS);

	if (!intmask || intmask == 0xffffffff) {
		result = VMM_IRQ_NONE;
		goto out;
	}

	if (intmask & (SDHCI_INT_CARD_INSERT | SDHCI_INT_CARD_REMOVE)) {
		u32 present = sdhci_readl(host, SDHCI_PRESENT_STATE) &
			      SDHCI_CARD_PRESENT;

		/*
		 * There is a observation on i.mx esdhc.  INSERT bit will be
		 * immediately set again when it gets cleared, if a card is
		 * inserted.  We have to mask the irq to prevent interrupt
		 * storm which will freeze the system.  And the REMOVE gets
		 * the same situation.
		 *
		 * More testing are needed here to ensure it works for other
		 * platforms though.
		 */
		sdhci_mask_irqs(host, present ? SDHCI_INT_CARD_INSERT :
						SDHCI_INT_CARD_REMOVE);
		sdhci_unmask_irqs(host, present ? SDHCI_INT_CARD_REMOVE :
						  SDHCI_INT_CARD_INSERT);

		sdhci_writel(host, intmask & (SDHCI_INT_CARD_INSERT |
			     SDHCI_INT_CARD_REMOVE), SDHCI_INT_STATUS);
		intmask &= ~(SDHCI_INT_CARD_INSERT | SDHCI_INT_CARD_REMOVE);

		mmc_detect_card_change(host->mmc, 200);
	}

	if (intmask & SDHCI_INT_CMD_MASK) {
		sdhci_writel(host, intmask & SDHCI_INT_CMD_MASK,
			SDHCI_INT_STATUS);
		sdhci_cmd_irq(host, intmask & SDHCI_INT_CMD_MASK);
	}

	if (intmask & SDHCI_INT_DATA_MASK) {
		sdhci_writel(host, intmask & SDHCI_INT_DATA_MASK,
			SDHCI_INT_STATUS);
		sdhci_data_irq(host, intmask & SDHCI_INT_DATA_MASK);
	}

	intmask &= ~(SDHCI_INT_CMD_MASK | SDHCI_INT_DATA_MASK);

	intmask &= ~SDHCI_INT_ERROR;

	if (intmask & SDHCI_INT_BUS_POWER) {
		vmm_printf("%s: Card is consuming too much power!\n",
			   mmc_hostname(host->mmc));
		sdhci_writel(host, SDHCI_INT_BUS_POWER, SDHCI_INT_STATUS);
	}

	intmask &= ~SDHCI_INT_BUS_POWER;

	if (intmask) {
		unexpected |= intmask;
		sdhci_writel(host, intmask, SDHCI_INT_STATUS);
	}

	result = VMM_IRQ_HANDLED;

out:
	return result;
}

struct sdhci_host *sdhci_alloc_host(struct vmm_device *dev, int extra)
{
	struct mmc_host *mmc;
	struct sdhci_host *host;

	mmc = mmc_alloc_host(sizeof(struct sdhci_host) + extra, dev);
	if (!mmc) {
		return NULL;
	}

	host = mmc_priv(mmc);
	host->mmc = mmc;
	host->dev = dev;

	return host;
}
VMM_EXPORT_SYMBOL(sdhci_alloc_host);

int sdhci_add_host(struct sdhci_host *host)
{
	int rc;
	const char *ver;
	physical_addr_t iopaddr;
	struct mmc_host *mmc = host->mmc;

	if (host->quirks & SDHCI_QUIRK_REG32_RW) {
		host->sdhci_version = 
			sdhci_readl(host, SDHCI_HOST_VERSION - 2) >> 16;
	} else {
		host->sdhci_version = sdhci_readw(host, SDHCI_HOST_VERSION);
	}

	host->sdhci_caps = sdhci_readl(host, SDHCI_CAPABILITIES);

	mmc->ops.send_cmd = sdhci_send_command;
	mmc->ops.set_ios = sdhci_set_ios;
	mmc->ops.init_card = sdhci_init_card;
	mmc->ops.get_cd = NULL;
	mmc->ops.get_wp = NULL;

	if (host->max_clk) {
		mmc->f_max = host->max_clk;
	} else {
		if ((host->sdhci_version & SDHCI_SPEC_VER_MASK) >= SDHCI_SPEC_300) {
			mmc->f_max = (host->sdhci_caps & SDHCI_CLOCK_V3_BASE_MASK)
					>> SDHCI_CLOCK_BASE_SHIFT;
		} else {
			mmc->f_max = (host->sdhci_caps & SDHCI_CLOCK_BASE_MASK)
					>> SDHCI_CLOCK_BASE_SHIFT;
		}
		mmc->f_max *= 1000000;
	}
	if (mmc->f_max == 0) {
		vmm_printf("%s: No base clock frequency\n", __func__);
		rc = VMM_EINVALID;
		goto free_nothing;
	}
	if (host->min_clk) {
		mmc->f_min = host->min_clk;
	} else {
		if ((host->sdhci_version & SDHCI_SPEC_VER_MASK) >= SDHCI_SPEC_300) {
			mmc->f_min = mmc->f_max / SDHCI_MAX_DIV_SPEC_300;
		} else {
			mmc->f_min = mmc->f_max / SDHCI_MAX_DIV_SPEC_200;
		}
	}

	mmc->voltages = 0;
	if (host->sdhci_caps & SDHCI_CAN_VDD_330) {
		mmc->voltages |= MMC_VDD_32_33 | MMC_VDD_33_34;
	}
	if (host->sdhci_caps & SDHCI_CAN_VDD_300) {
		mmc->voltages |= MMC_VDD_29_30 | MMC_VDD_30_31;
	}
	if (host->sdhci_caps & SDHCI_CAN_VDD_180) {
		mmc->voltages |= MMC_VDD_165_195;
	}

	if (host->quirks & SDHCI_QUIRK_BROKEN_VOLTAGE) {
		mmc->voltages |= host->voltages;
	}

	mmc->caps = MMC_CAP_MODE_HS | 
		    MMC_CAP_MODE_HS_52MHz | 
		    MMC_CAP_MODE_4BIT;
	if (host->sdhci_caps & SDHCI_CAN_DO_8BIT) {
		mmc->caps |= MMC_CAP_MODE_8BIT;
	}

	if (host->quirks & SDHCI_QUIRK_BROKEN_CARD_DETECTION) {
		mmc->caps |= MMC_CAP_NEEDS_POLL;
	}

	if (host->caps) {
		mmc->caps |= host->caps;
	}

	sdhci_init(host, 0);

	if (host->quirks & SDHCI_QUIRK_32BIT_DMA_ADDR) {
		/* Note: host aligned buffer must be 8-byte aligned */
		host->aligned_buffer = vmm_zalloc(512*1024);
		if (!host->aligned_buffer) {
			vmm_printf("%s: host buffer alloc failed!!!\n", 
				   __func__);
			rc = VMM_ENOMEM;
			goto free_nothing;
		}
		if (((u32)host->aligned_buffer) & 0x7) {
			vmm_printf("%s: host buffer not aligned to "
				   "8-byte boundary!!!\n", __func__);
			rc = VMM_EFAIL;
			goto free_host_buffer;
		}
	}

	if (host->irq > 0) {
		if ((rc = vmm_host_irq_register(host->irq, mmc_hostname(mmc), 
						sdhci_irq_handler, 
						host))) {
			goto free_host_buffer;
		}

	} else {
		host->quirks |= SDHCI_QUIRK_BROKEN_CARD_DETECTION;
	}

	rc = mmc_add_host(mmc);
	if (rc) {
		goto free_host_irq;
	}

	switch (host->sdhci_version & SDHCI_SPEC_VER_MASK) {
	case SDHCI_SPEC_100:
		ver = "v1";
		break;
	case SDHCI_SPEC_200:
		ver = "v2";
		break;
	case SDHCI_SPEC_300:
		ver = "v3";
		break;
	default:
		ver = "unknown version";
		break;
	};

	if ((rc = vmm_host_va2pa((virtual_addr_t)host->ioaddr, &iopaddr))) {
		goto remove_host;
	}

	vmm_printf("%s: SDHCI controller %s at 0x%llx irq %d [%s]\n",
		   mmc_hostname(mmc), ver, 
		   (unsigned long long)iopaddr, host->irq,
		   (host->sdhci_caps & SDHCI_CAN_DO_SDMA) ? "DMA" : "PIO");

	sdhci_enable_card_detection(host);

	return VMM_OK;

remove_host:
	mmc_remove_host(mmc);

free_host_irq:
	if (host->irq > 0) {
		vmm_host_irq_unregister(host->irq, mmc);
	}
free_host_buffer:
	if (host->quirks & SDHCI_QUIRK_32BIT_DMA_ADDR) {
		vmm_free(host->aligned_buffer);
		host->aligned_buffer = NULL;
	}
free_nothing:
	return rc;
}
VMM_EXPORT_SYMBOL(sdhci_add_host);

void sdhci_remove_host(struct sdhci_host *host, int dead)
{
	struct mmc_host *mmc = host->mmc;

	mmc_remove_host(mmc);

	if (host->irq > 0) {
		vmm_host_irq_unregister(host->irq, mmc);
	}

	if (host->quirks & SDHCI_QUIRK_32BIT_DMA_ADDR) {
		vmm_free(host->aligned_buffer);
		host->aligned_buffer = NULL;
	}
}
VMM_EXPORT_SYMBOL(sdhci_remove_host);

void sdhci_free_host(struct sdhci_host *host)
{
	mmc_free_host(host->mmc);
}
VMM_EXPORT_SYMBOL(sdhci_free_host);

static int __init sdhci_module_init(void)
{
	/* Nothing to do here. */
	return VMM_OK;
}

static void __exit sdhci_module_exit(void)
{
	/* Nothing to do here. */
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
