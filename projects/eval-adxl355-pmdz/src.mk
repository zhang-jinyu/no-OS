SRC_DIRS += $(PROJECT)/src

ifeq (y,$(strip $(TINYIIOD)))
SRC_DIRS += $(NO-OS)/iio/iio_app
endif

INCS += $(INCLUDE)/no-os/uart.h \
	$(INCLUDE)/no-os/spi.h \
	$(INCLUDE)/no-os/i2c.h \
	$(INCLUDE)/no-os/util.h \
	$(INCLUDE)/no-os/error.h \
	$(INCLUDE)/no-os/delay.h \
	$(INCLUDE)/no-os/timer.h \
	$(INCLUDE)/no-os/irq.h \
	$(INCLUDE)/no-os/rtc.h \
	$(INCLUDE)/no-os/print_log.h \
	$(INCLUDE)/no-os/gpio.h

INCS += $(PLATFORM_DRIVERS)/stm32_uart_stdio.h \
	$(PLATFORM_DRIVERS)/stm32_uart.h \
	$(PLATFORM_DRIVERS)/stm32_delay.h \
	$(PLATFORM_DRIVERS)/stm32_spi.h \
	$(PLATFORM_DRIVERS)/stm32_gpio.h \
	$(PLATFORM_DRIVERS)/stm32_hal.h

INCS += $(DRIVERS)/accel/adxl355/adxl355.h

ifeq (y,$(strip $(TINYIIOD)))
INCS += $(DRIVERS)/accel/adxl355/iio_adxl355.h

INCS += $(INCLUDE)/no-os/fifo.h \
	$(INCLUDE)/no-os/list.h \
	$(PLATFORM_DRIVERS)/uart_extra.h

INCS += $(PROJECT)/src/app_config.h \
	$(PROJECT)/src/parameters.h
endif

SRCS += $(NO-OS)/util/util.c \
	$(DRIVERS)/api/spi.c \
	$(DRIVERS)/api/i2c.c \
	$(DRIVERS)/api/gpio.c

SRCS += $(PLATFORM_DRIVERS)/stm32_delay.c \
	$(PLATFORM_DRIVERS)/stm32_gpio.c \
	$(PLATFORM_DRIVERS)/stm32_spi.c \
	$(PLATFORM_DRIVERS)/stm32_uart.c \
	$(PLATFORM_DRIVERS)/stm32_uart_stdio.c

SRCS += $(DRIVERS)/accel/adxl355/adxl355.c

ifeq (y,$(strip $(TINYIIOD)))
SRCS += $(DRIVERS)/accel/adxl355/iio_adxl355.c

SRCS += $(NO-OS)/util/fifo.c \
	$(NO-OS)/util/list.c
endif
