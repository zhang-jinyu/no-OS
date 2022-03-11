/***************************************************************************//**
 *   @file   maxim/maxim_gpio.c
 *   @brief  Implementation of gpio driver.
 *   @author Ciprian Regus (ciprian.regus@analog.com)
********************************************************************************
 * Copyright 2022(c) Analog Devices, Inc.
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

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include "no_os_gpio.h"
#include "no_os_irq.h"
#include "no_os_util.h"
#include "gpio.h"
#include "gpio_regs.h"
#include "gpio_extra.h"
#include "irq_extra.h"
#include "max32660.h"
#include "mxc_errors.h"

static struct no_os_callback_desc *gpio_callback[N_PORTS][N_PINS];

/******************************************************************************/
/************************ Functions Definitions *******************************/
/******************************************************************************/

static void _gpio_irq(uint8_t port)
{
	uint32_t pin;
	mxc_gpio_regs_t *gpio_regs = MXC_GPIO_GET_GPIO(port);
	uint32_t stat_reg = gpio_regs->int_stat;
	/** Clear interrupt flags for the current port*/
	gpio_regs->int_clr = stat_reg;
	while(stat_reg) {
		pin = find_first_set_bit(stat_reg);
		if (!gpio_callback[port][pin]) {
			stat_reg >>= pin + 1;
			continue;
		}
		void *ctx = gpio_callback[port][pin]->ctx;
		gpio_callback[port][pin]->callback(ctx, pin, NULL);

		stat_reg >>= pin + 1;
	}
}

void GPIO0_IRQHandler()
{
	_gpio_irq(0);
}

/**
 * @brief Obtain the GPIO decriptor.
 * @param desc - The GPIO descriptor.
 * @param param - GPIO initialization parameters
 * @return 0 in case of success, errno error codes otherwise.
 */
int32_t max_gpio_get(struct no_os_gpio_desc **desc,
		     const struct no_os_gpio_init_param *param)
{
	int32_t ret;
	mxc_gpio_cfg_t *g_cfg;
	struct max_gpio_init_param *pextra;
	uint32_t m_pad, m_func;
	struct no_os_gpio_desc *descriptor;

	if (!param || !param->extra || param->number >= N_PINS)
		return -EINVAL;

	descriptor = calloc(1, sizeof(*descriptor));
	if (!descriptor)
		return -ENOMEM;

	g_cfg = calloc(1, sizeof(*g_cfg));
	if (!g_cfg) {
		ret = -ENOMEM;
		goto free_descriptor;
	}

	pextra = param->extra;
	m_pad = (pextra->pull == 0) ? MXC_GPIO_PAD_PULL_UP :  MXC_GPIO_PAD_PULL_DOWN;
	m_func = (pextra->mode == 0) ? MXC_GPIO_FUNC_IN : MXC_GPIO_FUNC_OUT;

	if (pextra->port >= N_PORTS) {
		ret = -EINVAL;
		goto free_g_cfg;
	}

	g_cfg->port = MXC_GPIO_GET_GPIO(pextra->port);
	g_cfg->mask = BIT(param->number);
	g_cfg->pad = m_pad;
	g_cfg->func = m_func;

	descriptor->number = param->number;
	descriptor->platform_ops = param->platform_ops;
	descriptor->extra = g_cfg;

	MXC_GPIO_Init(pextra->port);
	MXC_GPIO_Config(descriptor->extra);

	*desc = descriptor;

	return 0;

free_g_cfg:
	free(g_cfg);
free_descriptor:
	free(descriptor);

	return ret;
}

/**
 * @brief Get the value of an optional GPIO.
 * @param desc - The GPIO descriptor.
 * @param param - GPIO initialization parameters
 * @return 0 in case of success, errno error codes otherwise.
 */
int32_t max_gpio_get_optional(struct no_os_gpio_desc **desc,
			      const struct no_os_gpio_init_param *param)
{
	if (param == NULL) {
		*desc = NULL;
		return 0;
	}

	return max_gpio_get(desc, param);
}

/**
 * @brief Free the resources allocated by no_os_gpio_get().
 * @param desc - The GPIO descriptor.
 * @return 0 in case of success, errno error codes otherwise.
 */
int32_t max_gpio_remove(struct no_os_gpio_desc *desc)
{
	if (!desc)
		return -EINVAL;

	free(desc->extra);
	free(desc);

	return 0;
}

/**
 * @brief Enable the input direction of the specified GPIO.
 * @param desc - The GPIO descriptor.
 * @return 0 in case of success, errno error codes otherwise.
 */
int32_t max_gpio_direction_input(struct no_os_gpio_desc *desc)
{
	int32_t ret;
	mxc_gpio_cfg_t *maxim_extra;

	if (!desc || desc->number >= N_PINS)
		return -EINVAL;

	maxim_extra = desc->extra;
	maxim_extra->func = MXC_GPIO_FUNC_IN;
	MXC_GPIO_Config(maxim_extra);

	return 0;
}

/**
 * @brief Enable the output direction of the specified GPIO.
 * @param desc - The GPIO descriptor.
 * @param value - The value.
 *                Example: NO_OS_GPIO_HIGH
 *                         NO_OS_GPIO_LOW
 * @return 0 in case of success, errno error codes otherwise.
 */
int32_t max_gpio_direction_output(struct no_os_gpio_desc *desc, uint8_t value)
{
	mxc_gpio_regs_t *gpio_regs;
	mxc_gpio_cfg_t *maxim_extra;

	if (!desc || desc->number >= N_PINS || value > NO_OS_GPIO_HIGH_Z)
		return -EINVAL;

	gpio_regs = MXC_GPIO_GET_GPIO(desc->number);
	maxim_extra = desc->extra;
	maxim_extra->func = MXC_GPIO_FUNC_OUT;
	MXC_GPIO_Config(maxim_extra);

	switch(value) {
	case NO_OS_GPIO_LOW:
		MXC_GPIO_OutClr(gpio_regs, BIT(desc->number));
		/** Enable gpio if it was previously set to HIGH_Z */
		if ((gpio_regs->en0 & BIT(desc->number)) == 0)
			gpio_regs->en0 |= BIT(desc->number);
		break;
	case NO_OS_GPIO_HIGH:
		MXC_GPIO_OutSet(gpio_regs, BIT(desc->number));
		if ((gpio_regs->en0 & BIT(desc->number)) == 0)
			gpio_regs->en0 |= BIT(desc->number);
		break;
	case NO_OS_GPIO_HIGH_Z:
		gpio_regs->en0 &= ~BIT(desc->number);
		break;
	}

	return 0;
}

/**
 * @brief Get the direction of the specified GPIO.
 * @param desc - The GPIO descriptor.
 * @param direction - The direction.
 *                    Example: NO_OS_GPIO_OUT
 *                             NO_OS_GPIO_IN
 * @return 0 in case of success, errno error codes otherwise.
 */
int32_t max_gpio_get_direction(struct no_os_gpio_desc *desc,
			       uint8_t *direction)
{
	mxc_gpio_cfg_t *maxim_extra;

	if (!desc || desc->number >= N_PINS)
		return -EINVAL;

	maxim_extra = desc->extra;

	if (maxim_extra->func == MXC_GPIO_FUNC_OUT)
		*direction = NO_OS_GPIO_OUT;
	else
		*direction = NO_OS_GPIO_IN;

	return 0;
}

/**
 * @brief Set the value of the specified GPIO.
 * @param desc - The GPIO descriptor.
 * @param value - The value.
 *                Example: NO_OS_GPIO_HIGH
 *                         NO_OS_GPIO_LOW
 * @return 0 in case of success, errno error codes otherwise.
 */
int32_t max_gpio_set_value(struct no_os_gpio_desc *desc, uint8_t value)
{
	mxc_gpio_cfg_t *max_gpio_cfg;
	mxc_gpio_regs_t *gpio_regs;

	if (!desc || desc->number >= N_PINS)
		return -EINVAL;

	max_gpio_cfg = desc->extra;
	gpio_regs = MXC_GPIO_GET_GPIO(max_gpio_cfg->port);

	switch(value) {
	case NO_OS_GPIO_LOW:
		MXC_GPIO_OutClr(gpio_regs, BIT(desc->number));
		if (gpio_regs->en0 & BIT(desc->number) == 0)
			gpio_regs->en0 |= BIT(desc->number);
		break;
	case NO_OS_GPIO_HIGH:
		MXC_GPIO_OutSet(gpio_regs, BIT(desc->number));
		if (gpio_regs->en0 & BIT(desc->number) == 0)
			gpio_regs->en0 |= BIT(desc->number);
		break;
	case NO_OS_GPIO_HIGH_Z:
		gpio_regs->en0 &= ~BIT(desc->number);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/**
 * @brief Get the value of the specified GPIO.
 * @param desc - The GPIO descriptor.
 * @param value - The value.
 *                Example: NO_OS_GPIO_HIGH
 *                         NO_OS_GPIO_LOW
 * 			   NO_OS_GPIO_HIGH_Z
 * @return 0 in case of success, errno error codes otherwise.
 */
int32_t max_gpio_get_value(struct no_os_gpio_desc *desc, uint8_t *value)
{
	mxc_gpio_cfg_t *max_gpio_cfg;
	mxc_gpio_regs_t *gpio_regs;

	if (!desc || desc->number >= N_PINS)
		return -EINVAL;

	max_gpio_cfg = desc->extra;
	gpio_regs = MXC_GPIO_GET_GPIO(max_gpio_cfg->port);

	if (!max_gpio_cfg)
		return -EINVAL;

	if (!(gpio_regs->en0 & BIT(desc->number)))
		*value = NO_OS_GPIO_HIGH_Z;
	else if (max_gpio_cfg->func == MXC_GPIO_FUNC_IN)
		*value = MXC_GPIO_InGet(gpio_regs, BIT(desc->number));
	else
		*value = MXC_GPIO_OutGet(gpio_regs, BIT(desc->number));

	return 0;
}

/**
 * @brief Init the gpio irq descriptor.
 * @param desc - The gpio irq descriptor.
 * @param param - The init param for the gpio irq descriptor.
 * @return 0 in case of success, -EINVAL otherwise.
 */
static int32_t max_gpio_irq_ctrl_init(struct no_os_irq_ctrl_desc **desc,
				      const struct no_os_irq_init_param *param)
{
	struct no_os_irq_ctrl_desc *descriptor;

	if (!param)
		return -EINVAL;

	descriptor = calloc(1, sizeof(*descriptor));
	if (!descriptor)
		return -ENOMEM;

	descriptor->irq_ctrl_id = param->irq_ctrl_id;
	descriptor->platform_ops = param->platform_ops;
	descriptor->extra = param->extra;

	*desc = descriptor;

	return 0;
}

/**
 * @brief Remove gpio interrupt descriptor.
 * @param desc - The GPIO descriptor.
 * @return 0 in case of success, -EINVAL otherwise.
 */
static int32_t max_gpio_irq_ctrl_remove(struct no_os_irq_ctrl_desc *desc)
{
	if (!desc)
		return -EINVAL;

	free(desc);

	return 0;
}

/**
 * @brief Set the trigger condition for an interrupt.
 * @param desc - gpio irq descriptor.
 * @param irq_id - the pin number
 * @param trig_l - the trigger condition.
 * @return 0 in case of success, -EINVAL otherwise.
 */
static int32_t max_gpio_irq_set_trigger_level(struct no_os_irq_ctrl_desc *desc,
		uint32_t irq_id,
		enum no_os_irq_trig_level trig_l)
{
	struct no_os_gpio_desc *g_desc;
	mxc_gpio_cfg_t *max_gpio_cfg;
	mxc_gpio_regs_t *gpio_regs;
	uint32_t is_enabled;

	if (!desc || !desc->extra || irq_id > N_PINS)
		return -EINVAL;

	g_desc = desc->extra;
	max_gpio_cfg = g_desc->extra;
	gpio_regs = max_gpio_cfg->port;
	is_enabled = gpio_regs->int_en & BIT(irq_id);

	/** Disable interrupts for pin desc->number */
	gpio_regs->int_en &= ~(BIT(irq_id));
	/** Clear pending interrupts for pin desc->number */
	gpio_regs->int_clr |= BIT(irq_id);
	/** Disable dual edge interrupts for pin desc->number */
	gpio_regs->int_dual_edge &= ~BIT(irq_id);

	switch (trig_l) {
	case NO_OS_IRQ_EDGE_RISING:
		/** Select edge triggered interrupt mode */
		gpio_regs->int_mod |= BIT(irq_id);
		/** Select rising edge trigger condition */
		gpio_regs->int_pol |= BIT(irq_id);
		break;
	case NO_OS_IRQ_EDGE_FALLING:
		/** Select edge triggered interrupt mode */
		gpio_regs->int_mod |= BIT(irq_id);
		/** Select falling edge trigger condition */
		gpio_regs->int_pol &= ~(BIT(irq_id));
		break;
	case NO_OS_IRQ_LEVEL_HIGH:
		/** Select level triggered interrupt mode */
		gpio_regs->int_mod &= ~(BIT(irq_id));
		/** Select level high trigger condition */
		//gpio_regs->int_pol |= BIT(irq_id);
		break;
	case NO_OS_IRQ_LEVEL_LOW:
		/** Select level triggered interrupt mode */
		gpio_regs->int_mod &= ~(BIT(irq_id));
		/** Select level low trigger condition */
		gpio_regs->int_pol &= ~(BIT(irq_id));
		break;
	case NO_OS_IRQ_EDGE_BOTH:
		/** Edge triggered on both rising and falling */
		gpio_regs->int_dual_edge |= BIT(irq_id);
		break;
	default:
		if (is_enabled)
			gpio_regs->int_en |= BIT(irq_id);
		return -EINVAL;
	}
	/** Enable interupts for pin desc->number if they were already enabled*/
	if (is_enabled)
		gpio_regs->int_en |= BIT(irq_id);

	return 0;
}

/**
 * @brief Register a function to be called when an interrupt occurs.
 * @param desc - The gpio irq descriptor
 * @param irq_id - The pin number.
 * @param callback_desc - The callback descriptor
 * @return 0 in case of success, errno error codes otherwise.
 */
static int32_t max_gpio_register_callback(struct no_os_irq_ctrl_desc *desc,
		uint32_t irq_id, struct no_os_callback_desc *callback_desc)
{
	int32_t ret;
	uint32_t port_id;
	struct no_os_callback_desc *descriptor;
	struct gpio_irq_config *g_irq;
	struct no_os_gpio_desc *g_desc;
	mxc_gpio_cfg_t *max_gpio_cfg;
	enum no_os_irq_trig_level trig_level;

	if (!desc || !desc->extra || !callback_desc || !callback_desc->config
	    || irq_id >= N_PINS)
		return -EINVAL;

	g_irq = callback_desc->config;
	g_desc = desc->extra;
	max_gpio_cfg = g_desc->extra;
	trig_level = g_irq->mode;
	port_id = MXC_GPIO_GET_IDX(max_gpio_cfg->port);

	descriptor = calloc(1, sizeof(*descriptor));
	if (!descriptor)
		return -ENOMEM;

	ret = max_gpio_direction_input(g_desc);
	if (ret) {
		free(descriptor);
		return ret;
	}

	ret = no_os_irq_trigger_level_set(desc, irq_id, trig_level);
	if (ret) {
		free(descriptor);
		return ret;
	}

	descriptor->ctx = callback_desc->ctx;
	descriptor->callback = callback_desc->callback;
	descriptor->config = callback_desc->config;

	gpio_callback[port_id][irq_id] = descriptor;

	return 0;
}

/**
 * @brief Unregister a callback function.
 * @param desc - The IRQ descriptor
 * @param irq_id - The pin number
 * @return 0 in case of success, errno error codes otherwise.
 */
static int32_t max_gpio_unregister_callback(struct no_os_irq_ctrl_desc *desc,
		uint32_t irq_id)
{
	uint32_t port_id;
	struct no_os_gpio_desc *g_desc;
	mxc_gpio_cfg_t *max_gpio_cfg;

	if (!desc || !desc->extra)
		return -EINVAL;

	g_desc = desc->extra;
	max_gpio_cfg = g_desc->extra;
	port_id = MXC_GPIO_GET_IDX(max_gpio_cfg->port);

	free(gpio_callback[port_id][irq_id]);
	gpio_callback[port_id][irq_id] = NULL;

	return 0;
}

/**
 * @brief Enable interrupts on a GPIO pin.
 * @param desc - The IRQ descriptor
 * @param irq_id - The pin number
 * @return 0 in case of success, errno error codes otherwise.
 */
static int32_t max_gpio_enable_irq(struct no_os_irq_ctrl_desc *desc,
				   uint32_t irq_id)
{
	mxc_gpio_cfg_t *max_gpio_cfg;
	mxc_gpio_regs_t *gpio_regs;
	struct no_os_gpio_desc *g_desc;

	if (!desc || !desc->extra)
		return -EINVAL;

	g_desc = desc->extra;
	max_gpio_cfg = g_desc->extra;
	gpio_regs = max_gpio_cfg->port;

	MXC_GPIO_EnableInt(gpio_regs, BIT(irq_id));

	return 0;
}

/**
 * @brief Disable interrupts on a GPIO pin.
 * @param desc - The IRQ descriptor
 * @param irq_id - The pin on which the interrupt will be disabled
 * @return 0 in case of success, errno error codes otherwise.
 */
static int32_t max_gpio_disable_irq(struct no_os_irq_ctrl_desc *desc,
				    uint32_t irq_id)
{
	mxc_gpio_cfg_t *max_gpio_cfg;
	mxc_gpio_regs_t *gpio_regs;
	struct no_os_gpio_desc *g_desc;

	if (!desc || !desc->extra)
		return -EINVAL;

	g_desc = desc->extra;
	gpio_regs = MXC_GPIO_GET_GPIO(g_desc->number);

	MXC_GPIO_DisableInt(gpio_regs, BIT(irq_id));

	return 0;
}

/**
 * @brief maxim platform specific GPIO platform ops structure
 */
const struct no_os_gpio_platform_ops max_gpio_ops = {
	.gpio_ops_get = &max_gpio_get,
	.gpio_ops_get_optional = &max_gpio_get_optional,
	.gpio_ops_remove = &max_gpio_remove,
	.gpio_ops_direction_input = &max_gpio_direction_input,
	.gpio_ops_direction_output = &max_gpio_direction_output,
	.gpio_ops_get_direction = &max_gpio_get_direction,
	.gpio_ops_set_value = &max_gpio_set_value,
	.gpio_ops_get_value = &max_gpio_get_value
};

/**
 * @brief maxim platform specific GPIO IRQ platform ops structure
 */
const struct no_os_irq_platform_ops max_gpio_irq_ops = {
	.init = &max_gpio_irq_ctrl_init,
	.register_callback = &max_gpio_register_callback,
	.unregister = &max_gpio_unregister_callback,
	.trigger_level_set = &max_gpio_irq_set_trigger_level,
	.enable = &max_gpio_enable_irq,
	.disable = &max_gpio_disable_irq,
	.remove = &max_gpio_irq_ctrl_remove,
};
