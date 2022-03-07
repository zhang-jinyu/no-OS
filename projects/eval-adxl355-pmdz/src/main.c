#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include "no-os/uart.h"
#include "no-os/gpio.h"
#include "no-os/spi.h"
#include "no-os/delay.h"
#include "no-os/util.h"
#include "no-os/print_log.h"

#ifdef STM32_PLATFORM
#include "stm32_spi.h"
#include "stm32_gpio.h"
#include "stm32_uart.h"
#include "stm32_uart_stdio.h"
#endif

#ifdef MAXIM_PLATFORM
#include "spi_extra.h"
#include "gpio_extra.h"
#include "maxim_uart.h"
#include "maxim_stdio.h"
#endif

#include "adxl355.h"

#ifdef STM32_PLATFORM
#define UART_ID			5
#define SPI_DEVICE_ID	 	1
#define SPI_CS			15
#define SPI_CS_PORT		GPIOA
#endif

#ifdef MAXIM_PLATFORM
#define UART_ID			1
#define SPI_DEVICE_ID		0
#define SPI_CS			0
#endif

#ifdef STM32_PLATFORM
extern UART_HandleTypeDef huart5;
#endif

int main ()
{
	struct uart_desc *uart;
	struct adxl355_dev *adxl355;
	struct spi_platform_ops *spi_ops;
	int ret;

#ifdef STM32_PLATFORM
	stm32_init();
	HAL_Init();
	SystemClock_Config();
	struct stm32_spi_init_param xsip  = {
		.chip_select_port = SPI_CS_PORT,
		.get_input_clock = HAL_RCC_GetPCLK1Freq,
	};
	struct stm32_uart_init_param xuip = {
		.huart = &huart5,
	};
	spi_ops = &stm32_spi_ops;
#elif MAXIM_PLATFORM
	struct max_uart_init_param xuip = {
		.flow = UART_FLOW_DIS
	};
	struct max_spi_init_param xsip = {
		.numSlaves = 1,
		.polarity = SPI_SS_POL_LOW
	};
	spi_ops = &max_spi_ops;
#endif

	struct spi_init_param sip = {
		.device_id = SPI_DEVICE_ID,
		.max_speed_hz = 4000000,
		.bit_order = SPI_BIT_ORDER_MSB_FIRST,
		.mode = NO_OS_SPI_MODE_0,
		.extra = &xsip,
		.platform_ops = spi_ops,
		.chip_select = SPI_CS,
	};

	struct adxl355_init_param init_data_adxl355 = {
		.comm_init.spi_init = sip,
		.comm_type = ADXL355_SPI_COMM,
	};

	struct uart_init_param uip = {
		.device_id = UART_ID,
		.baud_rate = 115200,
		.size = UART_CS_8,
		.parity = UART_PAR_NO,
		.stop = UART_STOP_1_BIT,
		.extra = &xuip,
	};

	ret = uart_init(&uart, &uip);
	if (ret < 0)
		goto error;

#ifdef STM32_PLATFORM
	stm32_uart_stdio(uart);
#elif MAXIM_PLATFORM
	maxim_uart_stdio(uart);
#endif

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
}
