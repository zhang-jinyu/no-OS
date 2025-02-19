/***************************************************************************//**
 *   @file   fmcadc5.c
 *   @brief  Implementation of Main Function.
 *   @author DBogdan (dragos.bogdan@analog.com)
 *   @author AntoniuMiclaus (antoniu.miclaus@analog.com)
********************************************************************************
 * Copyright 2020(c) Analog Devices, Inc.
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

#include <stdio.h>
#include <inttypes.h>
#include "parameters.h"
#include <xparameters.h>
#include <xil_printf.h>
#include <xil_cache.h>
#include "axi_adxcvr.h"
#include "no-os/spi.h"
#include "no-os/gpio.h"
#include "spi_extra.h"
#include "gpio_extra.h"
#include "no-os/delay.h"
#include "no-os/error.h"
#include "ad9625.h"
#include "axi_adc_core.h"
#include "axi_dmac.h"
#include "axi_jesd204_rx.h"
#include "axi_fmcadc5_sync.h"

#ifdef IIO_SUPPORT
#include "iio_app.h"
#include "iio_axi_adc.h"
#endif

static uint16_t adc_buff[ADC_MAX_SAMPLES] __attribute__((aligned));

int main(void)
{
	int32_t status;
	uint8_t pwr_good;

	// SPI configuration
	struct spi_init_param ad9625_0_spi_param = {
		.device_id = SPI_DEVICE_ID,
		.max_speed_hz = 2000000u,
		.chip_select = 0,
		.mode = NO_OS_SPI_MODE_0,
		.platform_ops = &xil_spi_ops
	};

	struct spi_init_param ad9625_1_spi_param = {
		.device_id = SPI_DEVICE_ID,
		.max_speed_hz = 2000000u,
		.chip_select = 1,
		.mode = NO_OS_SPI_MODE_0,
		.platform_ops = &xil_spi_ops,
	};

	struct xil_spi_init_param xil_spi_param = {
		.type = SPI_PL,
	};
	ad9625_0_spi_param.extra = &xil_spi_param;
	ad9625_1_spi_param.extra = &xil_spi_param;

	// GPIO configuration
	struct gpio_init_param gpio_sysref_param = {
		.number = GPIO_JESD204_SYSREF,
		.platform_ops = &xil_gpio_ops
	};
	struct gpio_init_param gpio_rst_0_param = {
		.number = GPIO_RST_0,
		.platform_ops = &xil_gpio_ops
	};
	struct gpio_init_param gpio_rst_1_param = {
		.number = GPIO_RST_1,
		.platform_ops = &xil_gpio_ops
	};
	struct gpio_init_param gpio_pwdn_0_param = {
		.number = GPIO_PWDN_0,
		.platform_ops = &xil_gpio_ops
	};
	struct gpio_init_param gpio_pwdn_1_param = {
		.number = GPIO_PWDN_1,
		.platform_ops = &xil_gpio_ops
	};
	struct gpio_init_param gpio_pwr_good_param = {
		.number = GPIO_PWR_GOOD,
		.platform_ops = &xil_gpio_ops
	};

	struct xil_gpio_init_param xil_gpio_param = {
		.type = GPIO_PL,
		.device_id = GPIO_DEVICE_ID
	};
	gpio_sysref_param.extra = &xil_gpio_param;
	gpio_rst_0_param.extra = &xil_gpio_param;
	gpio_rst_1_param.extra = &xil_gpio_param;
	gpio_pwdn_0_param.extra = &xil_gpio_param;
	gpio_pwdn_1_param.extra = &xil_gpio_param;
	gpio_pwr_good_param.extra = &xil_gpio_param;

	gpio_desc *gpio_sysref;
	gpio_desc *gpio_rst_0;
	gpio_desc *gpio_rst_1;
	gpio_desc *gpio_pwdn_0;
	gpio_desc *gpio_pwdn_1;
	gpio_desc *gpio_pwr_good;

	struct adxcvr_init ad9625_0_xcvr_param = {
		.name = "ad9152_0_xcvr",
		.base = XPAR_AXI_AD9625_0_XCVR_BASEADDR,
		.sys_clk_sel = 0,
		.out_clk_sel = 2,
		.lpm_enable = 1,
		.cpll_enable = 1,
		.ref_rate_khz = 625000,
		.lane_rate_khz = 6250000,
	};
	struct adxcvr	*ad9625_0_xcvr;

	struct adxcvr_init ad9625_1_xcvr_param = {
		.name = "ad9152_1_xcvr",
		.base = XPAR_AXI_AD9625_1_XCVR_BASEADDR,
		.sys_clk_sel = 0,
		.out_clk_sel = 2,
		.lpm_enable = 1,
		.cpll_enable = 1,
		.ref_rate_khz = 625000,
		.lane_rate_khz = 6250000,
	};
	struct adxcvr	*ad9625_1_xcvr;

	struct jesd204_rx_init  ad9625_0_jesd_param = {
		.name = "ad9625_0_jesd",
		.base = RX_0_JESD_BASEADDR,
		.octets_per_frame = 1,
		.frames_per_multiframe = 32,
		.subclass = 1,
		.device_clk_khz = 6250000 / 40,
		.lane_clk_khz = 6250000
	};
	struct axi_jesd204_rx *ad9625_0_jesd;

	struct jesd204_rx_init  ad9625_1_jesd_param = {
		.name = "ad9625_1_jesd",
		.base = RX_1_JESD_BASEADDR,
		.octets_per_frame = 1,
		.frames_per_multiframe = 32,
		.subclass = 1,
		.device_clk_khz = 6250000 / 40,
		.lane_clk_khz = 6250000
	};
	struct axi_jesd204_rx *ad9625_1_jesd;

	struct axi_adc_init ad9625_0_core_param = {
		.name = "ad9625_0_adc",
		.base = RX_0_CORE_BASEADDR,
		.num_channels = 1
	};
	struct axi_adc	*ad9625_0_core;

	struct axi_adc_init ad9625_1_core_param = {
		.name = "ad9625_1_adc",
		.base = RX_1_CORE_BASEADDR,
		.num_channels = 1
	};
	struct axi_adc	*ad9625_1_core;

	struct axi_dmac_init ad9625_dmac_param = {
		.name = "ad9625_dmac",
		.base = RX_DMA_BASEADDR,
		.direction = DMA_DEV_TO_MEM,
		.flags = 0
	};
	struct axi_dmac *ad9625_dmac;

	struct ad9625_init_param	ad9625_0_param;
	struct ad9625_init_param	ad9625_1_param;
	ad9625_0_param.spi_init = ad9625_0_spi_param;
	ad9625_1_param.spi_init = ad9625_1_spi_param;

	struct ad9625_dev		*ad9625_0_device;
	struct ad9625_dev		*ad9625_1_device;

	// ADC configuration
	ad9625_0_param.lane_rate_kbps = 6250000;
	ad9625_0_param.test_samples[0] = 0x5A5;
	ad9625_0_param.test_samples[1] = 0x1E1;
	ad9625_0_param.test_samples[2] = 0x777;
	ad9625_0_param.test_samples[3] = 0x444;

	ad9625_1_param.lane_rate_kbps = 6250000;
	ad9625_1_param.test_samples[0] = 0x5A5;
	ad9625_1_param.test_samples[1] = 0x1E1;
	ad9625_1_param.test_samples[2] = 0x777;
	ad9625_1_param.test_samples[3] = 0x444;

	// setup GPIOs
	gpio_get(&gpio_sysref,  &gpio_sysref_param);
	gpio_get(&gpio_rst_0,  &gpio_rst_0_param);
	gpio_get(&gpio_rst_1,  &gpio_rst_1_param);
	gpio_get(&gpio_pwdn_0,  &gpio_pwdn_0_param);
	gpio_get(&gpio_pwdn_1,  &gpio_pwdn_1_param);
	gpio_get(&gpio_pwr_good,  &gpio_pwr_good_param);

	gpio_direction_output(gpio_rst_0,  0);
	gpio_direction_output(gpio_rst_1,  0);
	gpio_direction_output(gpio_pwdn_0,  0);
	gpio_direction_output(gpio_pwdn_1,  0);
	mdelay(10);

	gpio_get_value(gpio_pwr_good, &pwr_good);
	if (!pwr_good) {
		xil_printf("Error: GPIO Power Good NOT set.\n\r");
		return FAILURE;
	}

	gpio_direction_output(gpio_rst_0,  1);
	gpio_direction_output(gpio_rst_1,  1);
	gpio_direction_output(gpio_sysref,  1);
	mdelay(100);

	// set up the device
	status = ad9625_setup(&ad9625_0_device, ad9625_0_param);
	if (status != SUCCESS) {
		printf("error: ad9625_0_setup() failed\n");
	}

	status = ad9625_setup(&ad9625_1_device, ad9625_1_param);
	if (status != SUCCESS) {
		printf("error: ad9625_1_setup() failed\n");
	}

	struct s_i5g_init i5g_init_param = {
		.ad9625_0_device = ad9625_0_device,
		.ad9625_1_device = ad9625_1_device,
		.ad9625_cs_0 = 0,
		.ad9625_cs_1 = 1,
		.regs = XPAR_AXI_FMCADC5_SYNC_BASEADDR,
	};
	struct s_i5g *i5g_core;

	// setup JESD core
	status = axi_jesd204_rx_init(&ad9625_0_jesd, &ad9625_0_jesd_param);
	if (status != SUCCESS) {
		printf("error: %s: axi_jesd204_rx_init() failed\n", ad9625_0_jesd->name);
	}
	status = axi_jesd204_rx_lane_clk_enable(ad9625_0_jesd);
	if (status != SUCCESS) {
		printf("error: %s: axi_jesd204_tx_lane_clk_enable() failed\n",
		       ad9625_0_jesd->name);
	}

	status = axi_jesd204_rx_init(&ad9625_1_jesd, &ad9625_1_jesd_param);
	if (status != SUCCESS) {
		printf("error: %s: axi_jesd204_rx_init() failed\n", ad9625_1_jesd->name);
	}
	status = axi_jesd204_rx_lane_clk_enable(ad9625_1_jesd);
	if (status != SUCCESS) {
		printf("error: %s: axi_jesd204_tx_lane_clk_enable() failed\n",
		       ad9625_1_jesd->name);
	}

	// set up the XCVRs
	status = adxcvr_init(&ad9625_0_xcvr, &ad9625_0_xcvr_param);
	if (status != SUCCESS) {
		printf("error: %s: adxcvr_init() failed\n", ad9625_0_xcvr->name);
	}
	status = adxcvr_clk_enable(ad9625_0_xcvr);
	if (status != SUCCESS) {
		printf("error: %s: adxcvr_clk_enable() failed\n", ad9625_0_xcvr->name);
	}

	status = adxcvr_init(&ad9625_1_xcvr, &ad9625_1_xcvr_param);
	if (status != SUCCESS) {
		printf("error: %s: adxcvr_init() failed\n", ad9625_1_xcvr->name);
	}
	status = adxcvr_clk_enable(ad9625_1_xcvr);
	if (status != SUCCESS) {
		printf("error: %s: adxcvr_clk_enable() failed\n", ad9625_1_xcvr->name);
	}

	// interface core setup
	status = axi_adc_init(&ad9625_0_core,  &ad9625_0_core_param);
	if (status != SUCCESS) {
		printf("axi_adc_init() error: %s\n", ad9625_0_core->name);
	}
	status = axi_adc_init(&ad9625_1_core,  &ad9625_1_core_param);
	if (status != SUCCESS) {
		printf("axi_adc_init() error: %s\n", ad9625_1_core->name);
	}

	// JESD core status
	status = axi_jesd204_rx_status_read(ad9625_0_jesd);
	if (status != SUCCESS) {
		printf("axi_jesd204_rx_status_read() error: %"PRIi32"\n", status);
	}
	status = axi_jesd204_rx_status_read(ad9625_1_jesd);
	if (status != SUCCESS) {
		printf("axi_jesd204_rx_status_read() error: %"PRIi32"\n", status);
	}

	i5g_setup(&i5g_core, i5g_init_param);

	status = axi_jesd204_rx_status_read(ad9625_0_jesd);
	if (status != SUCCESS) {
		printf("axi_jesd204_rx_status_read() error: %"PRIi32"\n", status);
	}
	status = axi_jesd204_rx_status_read(ad9625_1_jesd);
	if (status != SUCCESS) {
		printf("axi_jesd204_rx_status_read() error: %"PRIi32"\n", status);
	}

	// PRBS test
	ad9625_test(ad9625_0_device, AD9625_TEST_PNLONG);
	if(axi_adc_pn_mon(ad9625_0_core, AXI_ADC_PN23, 10) == -1) {
		printf("%s PN23 sequence mismatch at ad9625_0!\n", __func__);
	};
	ad9625_test(ad9625_1_device, AD9625_TEST_PNLONG);
	if(axi_adc_pn_mon(ad9625_1_core, AXI_ADC_PN23, 10) == -1) {
		printf("%s PN23 sequence mismatch at ad9625_1!\n", __func__);
	};

	// capture data with DMA
	ad9625_test(ad9625_0_device, AD9625_TEST_OFF);
	ad9625_test(ad9625_1_device, AD9625_TEST_OFF);

	axi_dmac_init(&ad9625_dmac, &ad9625_dmac_param);

	axi_dmac_transfer(ad9625_dmac, (uint32_t)adc_buff, sizeof(adc_buff));

#ifdef IIO_SUPPORT
	struct iio_device *adc_dev_0_desc;
	struct iio_axi_adc_desc *iio_axi_adc_0_desc;
	struct iio_axi_adc_init_param iio_axi_adc_0_init_par;
	iio_axi_adc_0_init_par = (struct iio_axi_adc_init_param) {
		.rx_adc = ad9625_0_core,
		.rx_dmac = ad9625_dmac,
	};

	struct iio_data_buffer read_buff0 = {
		.buff = (void *)adc_buff,
		.size = sizeof(adc_buff),
	};

	status = iio_axi_adc_init(&iio_axi_adc_0_desc, &iio_axi_adc_0_init_par);
	if (IS_ERR_VALUE(status))
		return FAILURE;
	iio_axi_adc_get_dev_descriptor(iio_axi_adc_0_desc, &adc_dev_0_desc);
	if (status < 0)
		return status;

	struct iio_app_device devices[] = {
		IIO_APP_DEVICE("axi-ad9625-0", iio_axi_adc_0_desc, adc_dev_0_desc,
			       &read_buff0, NULL),
	};

	return iio_app_run(devices, ARRAY_SIZE(devices));
#endif

	printf("adc5: setup and configuration is done\n");

	ad9625_remove(ad9625_0_device);
	ad9625_remove(ad9625_1_device);

	gpio_remove(gpio_sysref);
	gpio_remove(gpio_rst_0);
	gpio_remove(gpio_rst_1);
	gpio_remove(gpio_pwdn_0);
	gpio_remove(gpio_pwdn_1);
	gpio_remove(gpio_pwr_good);

	return(0);
}
