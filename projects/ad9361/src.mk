################################################################################
#									       #
#     Shared variables:							       #
#	- PROJECT							       #
#	- DRIVERS							       #
#	- INCLUDE							       #
#	- PLATFORM_DRIVERS						       #
#	- NO-OS								       #
#									       #
################################################################################

SRCS += $(PROJECT)/src/main.c
SRCS += $(DRIVERS)/rf-transceiver/ad9361/ad9361_api.c \
	$(DRIVERS)/rf-transceiver/ad9361/ad9361.c \
	$(DRIVERS)/rf-transceiver/ad9361/ad9361_conv.c \
	$(DRIVERS)/rf-transceiver/ad9361/ad9361_util.c
SRCS += $(DRIVERS)/axi_core/axi_adc_core/axi_adc_core.c \
	$(DRIVERS)/axi_core/axi_dac_core/axi_dac_core.c \
	$(DRIVERS)/axi_core/axi_dmac/axi_dmac.c \
	$(DRIVERS)/api/spi.c \
	$(DRIVERS)/api/gpio.c \
	$(NO-OS)/util/util.c
SRCS +=	$(PLATFORM_DRIVERS)/axi_io.c
SRCS +=	$(PLATFORM_DRIVERS)/$(PLATFORM)_spi.c \
	$(PLATFORM_DRIVERS)/$(PLATFORM)_gpio.c
ifeq (linux,$(strip $(PLATFORM)))
SRCS +=	$(PLATFORM_DRIVERS)/linux_delay.c
else
SRCS +=	$(PLATFORM_DRIVERS)/delay.c
endif
ifeq (y,$(strip $(TINYIIOD)))
LIBRARIES += iio

ifeq (linux,$(strip $(PLATFORM)))
CFLAGS += -DENABLE_IIO_NETWORK \
		-DDISABLE_SECURE_SOCKET
SRCS += $(NO-OS)/network/linux_socket/linux_socket.c \
		$(NO-OS)/network/tcp_socket.c \
	$(PLATFORM_DRIVERS)/linux_uart.c
else
SRCS += $(PLATFORM_DRIVERS)/uart.c
endif

SRCS += $(NO-OS)/util/fifo.c \
	$(NO-OS)/util/list.c \
	$(DRIVERS)/rf-transceiver/ad9361/iio_ad9361.c \
	$(DRIVERS)/axi_core/iio_axi_adc/iio_axi_adc.c \
	$(DRIVERS)/axi_core/iio_axi_dac/iio_axi_dac.c \
	$(NO-OS)/iio/iio_app/iio_app.c \
	$(NO-OS)/util/circular_buffer.c
endif
INCS += $(DRIVERS)/rf-transceiver/ad9361/common.h \
	$(PROJECT)/src/app_config.h
INCS += $(DRIVERS)/rf-transceiver/ad9361/ad9361.h \
	$(PROJECT)/src/parameters.h \
	$(DRIVERS)/rf-transceiver/ad9361/ad9361_util.h \
	$(DRIVERS)/rf-transceiver/ad9361/ad9361_api.h
INCS += $(DRIVERS)/axi_core/axi_adc_core/axi_adc_core.h \
	$(DRIVERS)/axi_core/axi_dac_core/axi_dac_core.h \
	$(INCLUDE)/no-os/irq.h \
	$(DRIVERS)/axi_core/axi_dmac/axi_dmac.h
ifeq (linux,$(strip $(PLATFORM)))
CFLAGS += -DPLATFORM_MB
INCS +=	$(PLATFORM_DRIVERS)/linux_spi.h \
	$(PLATFORM_DRIVERS)/linux_gpio.h \
	$(PLATFORM_DRIVERS)/linux_uart.h
endif
INCS +=	$(INCLUDE)/no-os/axi_io.h \
	$(INCLUDE)/no-os/spi.h \
	$(INCLUDE)/no-os/gpio.h \
	$(INCLUDE)/no-os/error.h \
	$(INCLUDE)/no-os/delay.h \
	$(INCLUDE)/no-os/util.h
ifeq (y,$(strip $(TINYIIOD)))

ifeq (linux,$(strip $(PLATFORM)))
INCS += $(NO-OS)/network/tcp_socket.h \
	$(NO-OS)/network/linux_socket/linux_socket.h \
	$(NO-OS)/network/network_interface.h \
	$(NO-OS)/network/noos_mbedtls_config.h
endif

INCS += $(INCLUDE)/no-os/fifo.h \
	$(INCLUDE)/no-os/uart.h \
	$(INCLUDE)/no-os/list.h \
	$(DRIVERS)/rf-transceiver/ad9361/iio_ad9361.h \
	$(DRIVERS)/axi_core/iio_axi_adc/iio_axi_adc.h \
	$(DRIVERS)/axi_core/iio_axi_dac/iio_axi_dac.h \
	$(NO-OS)/iio/iio_app/iio_app.h \
	$(INCLUDE)/no-os/circular_buffer.h
endif
ifeq (xilinx,$(strip $(PLATFORM)))
SRCS += $(PLATFORM_DRIVERS)/$(PLATFORM)_irq.c \
	$(DRIVERS)/api/irq.c
	
INCS += $(PLATFORM_DRIVERS)/irq_extra.h \
	$(PLATFORM_DRIVERS)/spi_extra.h \
	$(PLATFORM_DRIVERS)/gpio_extra.h \
	$(PLATFORM_DRIVERS)/uart_extra.h	
endif
