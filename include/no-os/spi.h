/***************************************************************************//**
 *   @file   spi.h
 *   @brief  Header file of SPI Interface
 *   @author DBogdan (dragos.bogdan@analog.com)
********************************************************************************
 * Copyright 2019(c) Analog Devices, Inc.
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

#ifndef SPI_H_
#define SPI_H_

/******************************************************************************/
/***************************** Include Files **********************************/
/******************************************************************************/

#include <stdint.h>

/******************************************************************************/
/********************** Macros and Constants Definitions **********************/
/******************************************************************************/

#define	SPI_CPHA	0x01
#define	SPI_CPOL	0x02

/******************************************************************************/
/*************************** Types Declarations *******************************/
/******************************************************************************/

/**
 * @enum spi_mode
 * @brief SPI configuration for clock phase and polarity.
 */
typedef enum spi_mode {
	/** Data on rising, shift out on falling */
	NO_OS_SPI_MODE_0 = (0 | 0),
	/** Data on falling, shift out on rising */
	NO_OS_SPI_MODE_1 = (0 | SPI_CPHA),
	/** Data on falling, shift out on rising */
	NO_OS_SPI_MODE_2 = (SPI_CPOL | 0),
	/** Data on rising, shift out on falling */
	NO_OS_SPI_MODE_3 = (SPI_CPOL | SPI_CPHA)
} spi_mode;

/**
 * @enum spi_bit_order
 * @brief SPI configuration for bit order (MSB/LSB).
 */
typedef enum spi_bit_order {
	/** Most-significant bit (MSB) first */
	SPI_BIT_ORDER_MSB_FIRST = 0,
	/** Least-significant bit (LSB) first */
	SPI_BIT_ORDER_LSB_FIRST = 1,
} spi_bit_order;

/**
 * @struct spi_msg_list
 * @brief List item describing a SPI transfer
 */
struct spi_msg {
	/** Buffer with data to send. If NULL, 0x00 will be sent */
	uint8_t			*tx_buff;
	/** Buffer where to store data. If NULL, incoming data won't be saved */
	uint8_t			*rx_buff;
	/** Length of buffers. Must have equal size. */
	uint32_t		bytes_number;
	/** If set, CS will be deasserted after the transfer */
	uint8_t			cs_change;
};

/**
 * @struct spi_platform_ops
 * @brief Structure holding SPI function pointers that point to the platform
 * specific function
 */
struct spi_platform_ops ;

/**
 * @struct spi_init_param
 * @brief Structure holding the parameters for SPI initialization
 */
typedef struct spi_init_param {
	/** Device ID */
	uint32_t	device_id;
	/** maximum transfer speed */
	uint32_t	max_speed_hz;
	/** SPI chip select */
	uint8_t		chip_select;
	/** SPI mode */
	enum spi_mode	mode;
	/** SPI bit order */
	enum spi_bit_order	bit_order;
	const struct spi_platform_ops *platform_ops;
	/**  SPI extra parameters (device specific) */
	void		*extra;
} spi_init_param;

/**
 * @struct spi_desc
 * @brief Structure holding SPI descriptor.
 */
typedef struct spi_desc {
	/** Device ID */
	uint32_t	device_id;
	/** maximum transfer speed */
	uint32_t	max_speed_hz;
	/** SPI chip select */
	uint8_t		chip_select;
	/** SPI mode */
	enum spi_mode	mode;
	/** SPI bit order */
	enum spi_bit_order	bit_order;
	const struct spi_platform_ops *platform_ops;
	/**  SPI extra parameters (device specific) */
	void		*extra;
} spi_desc;

/**
 * @struct spi_platform_ops
 * @brief Structure holding SPI function pointers that point to the platform
 * specific function
 */
struct spi_platform_ops {
	/** SPI initialization function pointer */
	int32_t (*init)(struct spi_desc **, const struct spi_init_param *);
	/** SPI write/read function pointer */
	int32_t (*write_and_read)(struct spi_desc *, uint8_t *, uint16_t);
	/** Iterate over the spi_msg array and send all messages at once */
	int32_t (*transfer)(struct spi_desc *, struct spi_msg *, uint32_t);
	/** SPI remove function pointer */
	int32_t (*remove)(struct spi_desc *);
};

/******************************************************************************/
/************************ Functions Declarations ******************************/
/******************************************************************************/

/* Initialize the SPI communication peripheral. */
int32_t spi_init(struct spi_desc **desc,
		 const struct spi_init_param *param);

/* Free the resources allocated by spi_init(). */
int32_t spi_remove(struct spi_desc *desc);

/* Write and read data to/from SPI. */
int32_t spi_write_and_read(struct spi_desc *desc,
			   uint8_t *data,
			   uint16_t bytes_number);

/* Iterate over the spi_msg array and send all messages at once */
int32_t spi_transfer(struct spi_desc *desc, struct spi_msg *msgs, uint32_t len);


#endif // SPI_H_
