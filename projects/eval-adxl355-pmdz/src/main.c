#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include "no-os/uart.h"
#include "no-os/gpio.h"
#include "no-os/spi.h"
#include "no-os/delay.h"
#include "no-os/util.h"
#include "no-os/print_log.h"
#include "stm32_spi.h"
#include "stm32_gpio.h"
#include "stm32_uart.h"
#include "stm32_uart_stdio.h"
#include "adxl355.h"
#ifdef IIO_SUPPORT
#include "app_config.h"
#include "parameters.h"
#include "iio_app.h"
#include "iio_adxl355.h"
#endif

#define SPI_DEVICE_ID	 1
#define SPI_CS			15
#define SPI_CS_PORT		GPIOA

#ifndef IIO_SUPPORT
extern UART_HandleTypeDef huart5;
#else
#define DATA_BUFFER_SIZE 400

// For output data you will need DATA_BUFFER_SIZE*4*sizeof(int32_t)
uint8_t iio_data_buffer[DATA_BUFFER_SIZE*4*sizeof(int)];
#endif

int main ()
{
#ifdef IIO_SUPPORT
	struct adxl355_iio_dev *adxl355_iio_desc;
	struct adxl355_iio_init_param adxl355_init_par;
	struct iio_data_buffer accel_buff = {
		.buff = (void *)iio_data_buffer,
		.size = DATA_BUFFER_SIZE*4*sizeof(int)
	};
#else
	struct uart_desc *uart;
	struct adxl355_dev *adxl355;
#endif

	int ret;
	struct stm32_spi_init_param xsip  = {
		.chip_select_port = SPI_CS_PORT,
		.get_input_clock = HAL_RCC_GetPCLK1Freq,
	};

	struct spi_init_param sip = {
		.device_id = SPI_DEVICE_ID,
		.max_speed_hz = 4000000,
		.bit_order = SPI_BIT_ORDER_MSB_FIRST,
		.mode = NO_OS_SPI_MODE_0,
		.extra = &xsip,
		.platform_ops = &stm32_spi_ops,
		.chip_select = SPI_CS,
	};

	struct adxl355_init_param init_data_adxl355 = {
		.comm_init.spi_init = sip,
		.comm_type = ADXL355_SPI_COMM,
	};

#ifdef IIO_SUPPORT
	adxl355_init_par.adxl355_initial = &init_data_adxl355;
#else
	struct stm32_uart_init_param xuip = {
		.huart = &huart5,
	};
	struct uart_init_param uip = {
		.device_id = 5,
		.baud_rate = 115200,
		.size = UART_CS_8,
		.parity = UART_PAR_NO,
		.stop = UART_STOP_1_BIT,
		.extra = &xuip,
	};
#endif

	stm32_init();

#ifdef IIO_SUPPORT
	ret = adxl355_iio_init(&adxl355_iio_desc, &adxl355_init_par);
	if (ret != SUCCESS)
		return ret;

	struct iio_app_device iio_devices[] = {
		{
			.name = "adxl355",
			.dev = adxl355_iio_desc,
			.dev_descriptor = adxl355_iio_desc->iio_dev,
			.read_buff = &accel_buff,
			.write_buff = NULL
		}
	};

	return iio_app_run(iio_devices, ARRAY_SIZE(iio_devices));
#else
	ret = uart_init(&uart, &uip);
	if (ret < 0)
		goto error;

	stm32_uart_stdio(uart);

	ret = adxl355_init(&adxl355, init_data_adxl355);
	if (ret < 0)
		goto error;
	ret = adxl355_soft_reset(adxl355);
	if (ret < 0)
		goto error;
	ret = adxl355_set_odr_lpf(adxl355, ADXL355_ODR_3_906HZ);
	if (ret < 0)
		goto error;
	ret = adxl355_set_op_mode(adxl355, ADXL355_MEAS_TEMP_ON_DRDY_OFF);
	if (ret < 0)
		goto error;

	struct adxl355_frac_repr x[32] = {0};
	struct adxl355_frac_repr y[32] = {0};
	struct adxl355_frac_repr z[32] = {0};
	struct adxl355_frac_repr temp;
	union adxl355_sts_reg_flags status_flags = {0};
	uint8_t fifo_entries = 0;

	while(1) {

		pr_info("Single read \n");
		ret = adxl355_get_xyz(adxl355,&x[0], &y[0], &z[0]);
		if (ret < 0)
			goto error;
		pr_info(" x=%d"".%09u", (int)x[0].integer, (abs)(x[0].fractional));
		pr_info(" y=%d"".%09u", (int)y[0].integer, (abs)(y[0].fractional));
		pr_info(" z=%d"".%09u \n", (int)z[0].integer,(abs)(z[0].fractional));

		ret = adxl355_get_fifo_data(adxl355,
					    &fifo_entries,
					    &x[0],
					    &y[0],
					    &z[0]);
		if (ret < 0)
			goto error;
		pr_info("Number of read entries from the FIFO %d \n", fifo_entries);
		pr_info("Number of read data sets from the FIFO %d \n", fifo_entries/3);
		for (uint8_t idx = 0; idx < 32; idx ++) {
			if (idx < fifo_entries/3) {
				pr_info(" x=%d"".%09u m/s^2", (int)x[idx].integer, (abs)(x[idx].fractional));
				pr_info(" y=%d"".%09u m/s^2", (int)y[idx].integer, (abs)(y[idx].fractional));
				pr_info(" z=%d"".%09u m/s^2", (int)z[idx].integer, (abs)(z[idx].fractional));
				pr_info("\n");
			}
		}

		pr_info("==========================================================\n");
		ret = adxl355_get_sts_reg(adxl355, &status_flags);
		if (ret < 0)
			goto error;
		pr_info("Activity flag = %d \n", (uint8_t)(status_flags.fields.Activity));
		pr_info("DATA_RDY flag = %d \n", (uint8_t)(status_flags.fields.DATA_RDY));
		pr_info("FIFO_FULL flag = %d \n", (uint8_t)(status_flags.fields.FIFO_FULL));
		pr_info("FIFO_OVR flag = %d \n", (uint8_t)(status_flags.fields.FIFO_OVR));
		pr_info("NVM_BUSY flag = %d \n", (uint8_t)(status_flags.fields.NVM_BUSY));
		pr_info("===========================================================\n");

		ret = adxl355_get_temp(adxl355, &temp);
		if (ret < 0)
			goto error;
		pr_info(" Temp =%d"".%09u millidegress Celsius \n", (int)temp.integer,
			(abs)(temp.fractional));

		mdelay(1000);
	}

error:
	pr_info("Error!\n");
	return 0;
#endif
}

