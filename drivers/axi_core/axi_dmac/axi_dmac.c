/***************************************************************************//**
 *   @file   axi_dmac.c
 *   @brief  Driver for the Analog Devices AXI-DMAC core.
 *   @author DBogdan (dragos.bogdan@analog.com)
********************************************************************************
 * Copyright 2018(c) Analog Devices, Inc.
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *  - Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  - Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  - Neither the name of Analog Devices, Inc. nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *  - The use of this software may or may not infringe the patent rights
 *    of one or more patent holders.  This license does not release you
 *    from the requirement that you obtain separate licenses from these
 *    patent holders to use this software.
 *  - Use of the software either in source or binary form, must be run
 *    on or directly connected to an Analog Devices Inc. component.
 *
 * THIS SOFTWARE IS PROVIDED BY ANALOG DEVICES "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, NON-INFRINGEMENT,
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL ANALOG DEVICES BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, INTELLECTUAL PROPERTY RIGHTS, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*******************************************************************************/

/******************************************************************************/
/***************************** Include Files **********************************/
/******************************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "no-os/axi_io.h"
#include "no-os/error.h"
#include "no-os/delay.h"
#include "axi_dmac.h"

/*******************************************************************************
 * @brief isr for dev to mem DMA transfer
*******************************************************************************/
void axi_dmac_dev_to_mem_isr(void *instance)
{
	struct axi_dmac *dmac = (struct axi_dmac *)instance;
	uint32_t burst_size;
	uint32_t reg_val;

	/* Get interrupt sources and clear interrupts. */
	axi_dmac_read(dmac, AXI_DMAC_REG_IRQ_PENDING, &reg_val);
	axi_dmac_write(dmac, AXI_DMAC_REG_IRQ_PENDING, reg_val);

	if (reg_val & AXI_DMAC_IRQ_SOT) {
		if (dmac->remaining_size) {
			/* See if remaining size is bigger than max transfer size and
			 * set burst size. */
			if (dmac->remaining_size > dmac->max_length) {
				burst_size = dmac->max_length;
			} else {
				burst_size = dmac->remaining_size - 1;
			}

			/* The current transfer was started; set up the new one. */
			axi_dmac_write(dmac, AXI_DMAC_REG_DEST_ADDRESS, dmac->next_dest_addr);
			axi_dmac_write(dmac, AXI_DMAC_REG_DEST_STRIDE, 0x0);
			axi_dmac_write(dmac, AXI_DMAC_REG_X_LENGTH, burst_size);
			axi_dmac_write(dmac, AXI_DMAC_REG_Y_LENGTH, 0x0);

			/* Compute size of the next transfer. */
			dmac->remaining_size = dmac->remaining_size - (burst_size + 1);
			/* Address must advance with +1 since the DMAC writes X_LENGTH+1. */
			dmac->next_dest_addr = dmac->next_dest_addr + (burst_size + 1);

			/* Trigger the next transfer. */
			axi_dmac_write(dmac, AXI_DMAC_REG_TRANSFER_SUBMIT, AXI_DMAC_TRANSFER_SUBMIT);
		}
//		} else {
//			dmac->transfer.transfer_done = true;
//			dmac->next_dest_addr = 0;
//		}
	}
	if (reg_val & AXI_DMAC_IRQ_EOT) {
		if (!dmac->remaining_size) {
			dmac->transfer.transfer_done = true;
			dmac->next_dest_addr = 0;
		}
	}
}

/*******************************************************************************
 * @brief isr for mem to dev DMA transfer
*******************************************************************************/
void axi_dmac_mem_to_dev_isr(void *instance)
{
	struct axi_dmac *dmac = (struct axi_dmac *)instance;
	uint32_t burst_size;
	uint32_t reg_val;

	/* Get interrupt sources and clear interrupts. */
	axi_dmac_read(dmac, AXI_DMAC_REG_IRQ_PENDING, &reg_val);
	axi_dmac_write(dmac, AXI_DMAC_REG_IRQ_PENDING, reg_val);

	if (reg_val & AXI_DMAC_IRQ_SOT) {
		if ((dmac->transfer.cyclic == CYCLIC) &&
			(dmac->next_src_addr >= (dmac->init_addr + dmac->transfer.size - 1))) {
			dmac->remaining_size = dmac->transfer.size;
			dmac->next_src_addr = dmac->init_addr;
		}

		if (dmac->remaining_size) {
			/** See if remaining size is bigger than max transfer size and
			 * set burst size. */
			if (dmac->remaining_size > dmac->max_length) {
				burst_size = dmac->max_length;
			} else {
				burst_size = dmac->remaining_size - 1;
			}

			/* The current transfer was started; set up the new one. */
			axi_dmac_write(dmac, AXI_DMAC_REG_SRC_ADDRESS, dmac->next_src_addr);
			axi_dmac_write(dmac, AXI_DMAC_REG_SRC_STRIDE, 0x0);
			axi_dmac_write(dmac, AXI_DMAC_REG_X_LENGTH, burst_size);
			axi_dmac_write(dmac, AXI_DMAC_REG_Y_LENGTH, 0x0);

			/* Compute parameters for the next transfer. */
			dmac->remaining_size = dmac->remaining_size - (burst_size + 1);
			/* Address must advance with +1 since the DMAC writes X_LENGTH+1. */
			dmac->next_src_addr = dmac->next_src_addr + (burst_size + 1);

			/* Trigger the current transfer */
			axi_dmac_write(dmac, AXI_DMAC_REG_TRANSFER_SUBMIT, AXI_DMAC_TRANSFER_SUBMIT);
		}
	}
	if (reg_val & AXI_DMAC_IRQ_EOT) {
		if ((!dmac->remaining_size) && (dmac->transfer.cyclic != CYCLIC)) {
			dmac->transfer.transfer_done = true;
			dmac->next_src_addr = 0;
		}
	}
}

/*******************************************************************************
 * @brief isr for mem to mem DMA transfer
*******************************************************************************/
void axi_dmac_mem_to_mem_isr(void *instance)
{
	struct axi_dmac *dmac = (struct axi_dmac *)instance;
	uint32_t burst_size;
	uint32_t reg_val;

	/* Get interrupt sources and clear interrupts. */
	axi_dmac_read(dmac, AXI_DMAC_REG_IRQ_PENDING, &reg_val);
	axi_dmac_write(dmac, AXI_DMAC_REG_IRQ_PENDING, reg_val);

	if (reg_val & AXI_DMAC_IRQ_SOT) {
		if (dmac->remaining_size) {
			/** See if remaining size is bigger than max transfer size and
			 * set burst size. */
			if (dmac->remaining_size > dmac->max_length) {
				burst_size = dmac->max_length;
			} else {
				burst_size = dmac->remaining_size - 1;
			}

			/* The current transfer was started; set up the new one. */
			axi_dmac_write(dmac, AXI_DMAC_REG_SRC_ADDRESS, dmac->next_src_addr);
			axi_dmac_write(dmac, AXI_DMAC_REG_SRC_STRIDE, 0x0);
			axi_dmac_write(dmac, AXI_DMAC_REG_DEST_ADDRESS, dmac->next_dest_addr);
			axi_dmac_write(dmac, AXI_DMAC_REG_DEST_STRIDE, 0x0);
			axi_dmac_write(dmac, AXI_DMAC_REG_X_LENGTH, burst_size);
			axi_dmac_write(dmac, AXI_DMAC_REG_Y_LENGTH, 0x0);

			/* Compute parameters for the next transfer. */
			dmac->remaining_size = dmac->remaining_size - (burst_size + 1);
			/* Address must advance with +1 since the DMAC writes X_LENGTH+1. */
			dmac->next_src_addr = dmac->next_src_addr + (burst_size + 1);
			dmac->next_dest_addr = dmac->next_dest_addr + (burst_size + 1);

			/* If transfer finished advance src address so that transfer_done can be set*/
			if (!dmac->remaining_size) {
				dmac->next_src_addr = dmac->next_src_addr + (burst_size + 1);
			}

			/* Trigger the current transfer */
			axi_dmac_write(dmac, AXI_DMAC_REG_TRANSFER_SUBMIT, AXI_DMAC_TRANSFER_SUBMIT);
		}
	}
	if (reg_val & AXI_DMAC_IRQ_EOT) {
		if (!dmac->remaining_size) {
			if(dmac->next_src_addr > (dmac->init_addr + dmac->transfer.size)) {
				dmac->transfer.transfer_done = true;
				dmac->next_src_addr = 0;
				dmac->next_dest_addr = 0;
			}
		}
	}
}

/*******************************************************************************
 * @brief axi_dmac_read
 *******************************************************************************/
int32_t axi_dmac_read(struct axi_dmac *dmac,
		      uint32_t reg_addr,
		      uint32_t *reg_data)
{
	axi_io_read(dmac->base, reg_addr, reg_data);

	return 0;
}

/*******************************************************************************
 * @brief axi_dmac_write
 *******************************************************************************/
int32_t axi_dmac_write(struct axi_dmac *dmac,
		       uint32_t reg_addr,
		       uint32_t reg_data)
{
	axi_io_write(dmac->base, reg_addr, reg_data);

	return 0;
}

/*******************************************************************************
 * @brief axi_dmac_is_transfer_ready
 *******************************************************************************/
int32_t axi_dmac_is_transfer_ready(struct axi_dmac *dmac, bool *rdy)
{
	*rdy = dmac->transfer.transfer_done;

	return 0;
}

/*******************************************************************************
 * @brief detect DMA capabilities
 *******************************************************************************/
static int32_t axi_dmac_detect_caps(struct axi_dmac *dmac)
{
	uint32_t reg_val, initial_reg_val = 0;
	uint32_t src_mem_mapped = 0;
	uint32_t dest_mem_mapped = 0;

	dmac->max_length = -1;
	dmac->direction = DMA_MEM_TO_MEM;

	/* Check if HW cyclic possible */
	axi_dmac_read(dmac, AXI_DMAC_REG_FLAGS, &initial_reg_val);
	axi_dmac_write(dmac, AXI_DMAC_REG_FLAGS, DMA_CYCLIC);
	axi_dmac_read(dmac, AXI_DMAC_REG_FLAGS, &reg_val);
	if (reg_val == DMA_CYCLIC)
		dmac->hw_cyclic = true;
	/* Restore initial value for AXI_DMAC_REG_FLAGS register */
	axi_dmac_write(dmac, AXI_DMAC_REG_FLAGS, initial_reg_val);

	/* Get maximum burst size and set value. */
	axi_dmac_write(dmac, AXI_DMAC_REG_X_LENGTH, dmac->max_length);
	axi_dmac_read(dmac, AXI_DMAC_REG_X_LENGTH, &dmac->max_length);

	/* Get transfer direction and set value. */
	axi_dmac_write(dmac, AXI_DMAC_REG_DEST_ADDRESS, 0xffffffff);
	axi_dmac_read(dmac, AXI_DMAC_REG_DEST_ADDRESS, &dest_mem_mapped);
	axi_dmac_write(dmac, AXI_DMAC_REG_SRC_ADDRESS, 0xffffffff);
	axi_dmac_read(dmac, AXI_DMAC_REG_SRC_ADDRESS, &src_mem_mapped);

	if (dest_mem_mapped && !src_mem_mapped) {
		dmac->direction = DMA_DEV_TO_MEM;
	} else if (!dest_mem_mapped && src_mem_mapped) {
		dmac->direction = DMA_MEM_TO_DEV;
	} else if (dest_mem_mapped && src_mem_mapped) {
		dmac->direction = DMA_MEM_TO_MEM;
	} else {
		printf("Destination and source memory-mapped interfaces not supported.\n");
		return FAILURE;
	}
	return 0;
}

/*******************************************************************************
 * @brief axi_dmac_init
 *******************************************************************************/
int32_t axi_dmac_init(struct axi_dmac **dmac_core,
		      const struct axi_dmac_init *init)
{
	struct axi_dmac *dmac;

	dmac = (struct axi_dmac *)calloc(1, sizeof(*dmac));
	if (!dmac)
		return FAILURE;

	dmac->name = init->name;
	dmac->base = init->base;

	*dmac_core = dmac;

	int32_t status = axi_dmac_detect_caps(*dmac_core);
	if (status < 0)
		return FAILURE;

	return 0;
}

/*******************************************************************************
 * @brief axi_dmac_remove
 *******************************************************************************/
int32_t axi_dmac_remove(struct axi_dmac *dmac)
{
	if(!dmac)
		return FAILURE;

	free(dmac);

	return 0;
}

/*******************************************************************************
 * @brief starts a DMA transfer
 *******************************************************************************/
int32_t axi_dmac_transfer_start(struct axi_dmac *dmac, struct axi_dma_transfer *dma_transfer)
{
	uint32_t reg_val, burst_size;

	if (dma_transfer->size == 0)
		return 0; /* Nothing to do. */

	/* Set current transfer parameters. */
	dmac->transfer.size = dma_transfer->size;
	dmac->transfer.cyclic = dma_transfer->cyclic;
	dmac->transfer.dest_addr = dma_transfer->dest_addr;
	dmac->transfer.src_addr = dma_transfer->src_addr;

	dmac->remaining_size = dma_transfer->size;
	dmac->next_dest_addr = dma_transfer->dest_addr;
	dmac->next_src_addr = dma_transfer->src_addr;

	/* If HW cyclic transfer selected and not available, show error */
	if ((!dmac->hw_cyclic) && (dma_transfer->cyclic == CYCLIC)) {
		printf("Transfer mode not supported!\n");
		return FAILURE;
	}

	/* Cyclic transfers not possible for DEV_TO_MEM and MEM_TO_MEM transmissions. */
	if ((dmac->direction == DMA_DEV_TO_MEM) || (dmac->direction == DMA_MEM_TO_MEM)) {
		if (dma_transfer->cyclic == CYCLIC) {
			printf("Transfer mode not supported!\n");
			return FAILURE;
		}
	}

	/* Cyclic transfers set  to HW or SW depending on size */
	if ((dmac->direction == DMA_MEM_TO_DEV) && (dmac->transfer.cyclic == CYCLIC)) {
		if ((dmac->remaining_size -1) <= dmac->max_length) {
			if (dmac->hw_cyclic)
				axi_dmac_write(dmac, AXI_DMAC_REG_FLAGS, DMA_CYCLIC);
		} else {
			axi_dmac_read(dmac, AXI_DMAC_REG_FLAGS, &reg_val);
			reg_val = reg_val & ~DMA_CYCLIC;
			axi_dmac_write(dmac, AXI_DMAC_REG_FLAGS, reg_val);
		}
	}


	/* Enable DMA if not already enabled. */
	axi_dmac_read(dmac, AXI_DMAC_REG_CTRL, &reg_val);
	if (!(reg_val & AXI_DMAC_CTRL_ENABLE)) {
		axi_dmac_write(dmac, AXI_DMAC_REG_CTRL, 0x0);
		axi_dmac_write(dmac, AXI_DMAC_REG_CTRL, AXI_DMAC_CTRL_ENABLE);
		axi_dmac_write(dmac, AXI_DMAC_REG_IRQ_MASK, 0x0);
	}

	axi_dmac_read(dmac, AXI_DMAC_REG_TRANSFER_SUBMIT, &reg_val);
	/* If we don't have a start of transfer then start compute
	 * values and trigger next transfer. */
	if (!(reg_val & AXI_DMAC_QUEUE_FULL)) {
		switch (dmac->direction) {
		case DMA_DEV_TO_MEM:
			dmac->init_addr = dmac->next_dest_addr;
			axi_dmac_write(dmac, AXI_DMAC_REG_DEST_ADDRESS, dmac->next_dest_addr);
			axi_dmac_write(dmac, AXI_DMAC_REG_DEST_STRIDE, 0x0);
			break;
		case DMA_MEM_TO_DEV:
			dmac->init_addr = dmac->next_src_addr;
			axi_dmac_write(dmac, AXI_DMAC_REG_SRC_ADDRESS, dmac->next_src_addr);
			axi_dmac_write(dmac, AXI_DMAC_REG_SRC_STRIDE, 0x0);
			break;
		case DMA_MEM_TO_MEM:
			dmac->init_addr = dmac->next_src_addr;
			axi_dmac_write(dmac, AXI_DMAC_REG_DEST_ADDRESS, dmac->next_dest_addr);
			axi_dmac_write(dmac, AXI_DMAC_REG_DEST_STRIDE, 0x0);
			axi_dmac_write(dmac, AXI_DMAC_REG_SRC_ADDRESS, dmac->next_src_addr);
			axi_dmac_write(dmac, AXI_DMAC_REG_SRC_STRIDE, 0x0);
			break;
		default:
			return FAILURE; /* Other directions are not supported yet. */
		}

		/* Compute the burst size. */
		if (dmac->remaining_size > dmac->max_length) {
			burst_size = dmac->max_length;
		} else {
			burst_size = dmac->remaining_size - 1;
		}

		/* Compute next address (src or dest, or both, depending on type of transfer). */
		switch (dmac->direction) {
		case DMA_DEV_TO_MEM:
			dmac->next_dest_addr = dmac->next_dest_addr + (burst_size + 1);
			break;
		case DMA_MEM_TO_DEV:
			dmac->next_src_addr = dmac->next_src_addr + (burst_size + 1);
			break;
		case DMA_MEM_TO_MEM:
			dmac->next_dest_addr = dmac->next_dest_addr + (burst_size + 1);
			dmac->next_src_addr = dmac->next_src_addr + (burst_size + 1);
			break;
		default:
			return FAILURE;
		}

		/* Compute remaining size. */
		dmac->remaining_size = dmac->remaining_size - (burst_size + 1);

		/* Specify the length of the transfer and trigger transfer. */
		axi_dmac_write(dmac, AXI_DMAC_REG_X_LENGTH, burst_size);
		axi_dmac_write(dmac, AXI_DMAC_REG_Y_LENGTH, 0x0);
		axi_dmac_write(dmac, AXI_DMAC_REG_TRANSFER_SUBMIT, AXI_DMAC_TRANSFER_SUBMIT);
	} else {
		return FAILURE;
	}

	return 0;
}

/*******************************************************************************
 * @brief wait for DMA transfer to be completed
 *******************************************************************************/
int32_t axi_dmac_transfer_wait_completion(struct axi_dmac *dmac)
{
	uint32_t timeout = 0;

	while(!dmac->transfer.transfer_done) {
		timeout++;
		if (timeout == UINT32_MAX){
			printf("Error transferring data using DMA.\n");
			return FAILURE;
		}
	}

	return 0;
}

/*******************************************************************************
 * @brief stop the DMA transfer
 *******************************************************************************/
void axi_dmac_transfer_stop(struct axi_dmac *dmac)
{
	axi_dmac_write(dmac, AXI_DMAC_REG_CTRL, AXI_DMAC_CTRL_DISABLE);
}
