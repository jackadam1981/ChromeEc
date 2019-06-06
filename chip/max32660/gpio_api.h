/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* MAX32660 GPIO API */

#ifndef _GPIO_API_
#define _GPIO_API_

/* **** Includes **** */
#include "gpio_regs.h"

/**
 * @defgroup gpio General-Purpose Input/Output (GPIO)
 * @ingroup periphlibs
 * @{
 */

/* **** Definitions **** */
/**
 * @defgroup gpio_port_pin Port and Pin Definitions
 * @ingroup gpio
 * @{
 * @defgroup gpio_port Port Definitions
 * @ingroup gpio_port_pin
 * @{
 */
#define PORT_0 ((uint32_t)(0UL)) /**< Port 0  Define*/
#define PORT_1 ((uint32_t)(1UL)) /**< Port 1  Define*/
#define PORT_2 ((uint32_t)(2UL)) /**< Port 2  Define*/
#define PORT_3 ((uint32_t)(3UL)) /**< Port 3  Define*/
#define PORT_4 ((uint32_t)(4UL)) /**< Port 4  Define*/
/**@} end of gpio_port group*/
/**
 * @defgroup gpio_pin Pin Definitions
 * @ingroup gpio_port_pin
 * @{
 */
#define PIN_0 ((uint32_t)(1UL << 0))   /**< Pin 0 Define */
#define PIN_1 ((uint32_t)(1UL << 1))   /**< Pin 1 Define */
#define PIN_2 ((uint32_t)(1UL << 2))   /**< Pin 2 Define */
#define PIN_3 ((uint32_t)(1UL << 3))   /**< Pin 3 Define */
#define PIN_4 ((uint32_t)(1UL << 4))   /**< Pin 4 Define */
#define PIN_5 ((uint32_t)(1UL << 5))   /**< Pin 5 Define */
#define PIN_6 ((uint32_t)(1UL << 6))   /**< Pin 6 Define */
#define PIN_7 ((uint32_t)(1UL << 7))   /**< Pin 7 Define */
#define PIN_8 ((uint32_t)(1UL << 8))   /**< Pin 8 Define */
#define PIN_9 ((uint32_t)(1UL << 9))   /**< Pin 9 Define */
#define PIN_10 ((uint32_t)(1UL << 10)) /**< Pin 10 Define */
#define PIN_11 ((uint32_t)(1UL << 11)) /**< Pin 11 Define */
#define PIN_12 ((uint32_t)(1UL << 12)) /**< Pin 12 Define */
#define PIN_13 ((uint32_t)(1UL << 13)) /**< Pin 13 Define */
#define PIN_14 ((uint32_t)(1UL << 14)) /**< Pin 14 Define */
#define PIN_15 ((uint32_t)(1UL << 15)) /**< Pin 15 Define */
#define PIN_16 ((uint32_t)(1UL << 16)) /**< Pin 16 Define */
#define PIN_17 ((uint32_t)(1UL << 17)) /**< Pin 17 Define */
#define PIN_18 ((uint32_t)(1UL << 18)) /**< Pin 18 Define */
#define PIN_19 ((uint32_t)(1UL << 19)) /**< Pin 19 Define */
#define PIN_20 ((uint32_t)(1UL << 20)) /**< Pin 20 Define */
#define PIN_21 ((uint32_t)(1UL << 21)) /**< Pin 21 Define */
#define PIN_22 ((uint32_t)(1UL << 22)) /**< Pin 22 Define */
#define PIN_23 ((uint32_t)(1UL << 23)) /**< Pin 23 Define */
#define PIN_24 ((uint32_t)(1UL << 24)) /**< Pin 24 Define */
#define PIN_25 ((uint32_t)(1UL << 25)) /**< Pin 25 Define */
#define PIN_26 ((uint32_t)(1UL << 26)) /**< Pin 26 Define */
#define PIN_27 ((uint32_t)(1UL << 27)) /**< Pin 27 Define */
#define PIN_28 ((uint32_t)(1UL << 28)) /**< Pin 28 Define */
#define PIN_29 ((uint32_t)(1UL << 29)) /**< Pin 29 Define */
#define PIN_30 ((uint32_t)(1UL << 30)) /**< Pin 30 Define */
#define PIN_31 ((uint32_t)(1UL << 31)) /**< Pin 31 Define */
/**@} end of gpio_pin group */
/**@} end of gpio_port_pin group */

/**
 * Enumeration type for the GPIO Function Type
 */
typedef enum {
	GPIO_FUNC_IN,   /**< GPIO Input */
	GPIO_FUNC_OUT,  /**< GPIO Output */
	GPIO_FUNC_ALT1, /**< Alternate Function Selection */
	GPIO_FUNC_ALT2, /**< Alternate Function Selection */
	GPIO_FUNC_ALT3, /**< Alternate Function Selection */
	GPIO_FUNC_ALT4, /**< Alternate Function Selection */
} gpio_func_t;

/**
 * Enumeration type for the type of GPIO pad on a given pin.
 */
typedef enum {
	GPIO_PAD_NONE,      /**< No pull-up or pull-down */
	GPIO_PAD_PULL_UP,   /**< Set pad to weak pull-up */
	GPIO_PAD_PULL_DOWN, /**< Set pad to weak pull-down */
} gpio_pad_t;

/**
 * Structure type for configuring a GPIO port.
 */
typedef struct {
	uint32_t port;    /**< Index of GPIO port */
	uint32_t mask;    /**< Pin mask (multiple pins may be set) */
	gpio_func_t func; /**< Function type */
	gpio_pad_t pad;   /**< Pad type */
} gpio_cfg_t;

typedef enum { GPIO_INTERRUPT_LEVEL, GPIO_INTERRUPT_EDGE } gpio_int_mode_t;

typedef enum {
	GPIO_INTERRUPT_FALLING = 0, /**< Interrupt triggers on falling edge */
	GPIO_INTERRUPT_HIGH = GPIO_INTERRUPT_FALLING, /**< Interrupt triggers
							 when level is high */
	GPIO_INTERRUPT_RISING, /**< Interrupt triggers on rising edge */
	GPIO_INTERRUPT_LOW = GPIO_INTERRUPT_RISING, /**< Interrupt triggers when
						       level is low */
	GPIO_INTERRUPT_BOTH /**< Interrupt triggers on either edge */
} gpio_int_pol_t;

/* **** Function Prototypes **** */

/**
 * @brief      Configure GPIO pin(s).
 * @param      cfg   Pointer to configuration structure describing the pin.
 * @return     #E_NO_ERROR if everything is successful.
 */
int GPIO_Config(const gpio_cfg_t *cfg);

/**
 * @brief      Gets the pin(s) input state.
 * @param      cfg   Pointer to configuration structure describing the pin.
 * @return     The requested pin state.
 */
uint32_t GPIO_InGet(const gpio_cfg_t *cfg);

/**
 * @brief      Sets the pin(s) to a high level output.
 * @param      cfg   Pointer to configuration structure describing the pin.
 *
 */
void GPIO_OutSet(const gpio_cfg_t *cfg);

/**
 * @brief      Clears the pin(s) to a low level output.
 * @param      cfg   Pointer to configuration structure describing the pin.
 *
 */
void GPIO_OutClr(const gpio_cfg_t *cfg);

/**
 * @brief      Gets the pin(s) output state.
 * @param      cfg   Pointer to configuration structure describing the pin.
 *
 * @return     The state of the requested pin.
 *
 */
uint32_t GPIO_OutGet(const gpio_cfg_t *cfg);

/**
 * @brief      Write the pin(s) to a desired output level.
 * @param      cfg   Pointer to configuration structure describing the pin.
 * @param      val   Desired output level of the pin(s). This will be masked
 *                   with the configuration mask.
 */
void GPIO_OutPut(const gpio_cfg_t *cfg, uint32_t val);

/**
 * @brief      Toggles the the pin(s) output level.
 * @param      cfg   Pointer to configuration structure describing the pin.
 *
 */
void GPIO_OutToggle(const gpio_cfg_t *cfg);

/**
 * @brief      Configure GPIO interrupt(s)
 * @param      cfg   Pointer to configuration structure describing the pin.
 * @param      mode  Requested interrupt mode.
 * @param      pol   Requested interrupt polarity.
 * @return     #E_NO_ERROR if everything is successful.
 */
int GPIO_IntConfig(const gpio_cfg_t *cfg, gpio_int_mode_t mode,
		   gpio_int_pol_t pol);

/**
 * @brief      Enables the specified GPIO interrupt
 * @param      cfg   Pointer to configuration structure describing the pin.
 *
 */
void GPIO_IntEnable(const gpio_cfg_t *cfg);

/**
 * @brief      Disables the specified GPIO interrupt.
 * @param      cfg   Pointer to configuration structure describing the pin.
 */
void GPIO_IntDisable(const gpio_cfg_t *cfg);

/**
 * @brief      Gets the interrupt(s) status on a GPIO pin.
 * @param      cfg   Pointer to configuration structure describing the pin
 *                   for which the status is being requested.
 * @return     The requested interrupt status.
 */
uint32_t GPIO_IntStatus(const gpio_cfg_t *cfg);

/**
 * @brief      Clears the interrupt(s) status on a GPIO pin.
 * @param      cfg   Pointer to configuration structure describing the pin
 *                   to clear the interrupt state of.
 */
void GPIO_IntClr(const gpio_cfg_t *cfg);

/**
 * @brief      Type alias for a GPIO callback function with prototype:
 * @code
    void callback_fn(void *cbdata);
 * @endcode
 * @param      cbdata  A void pointer to the data type as registered when
 *                     GPIO_RegisterCallback() was called.
 */
typedef void (*gpio_callback_fn)(void *cbdata);

/**
 * @brief      Registers a callback for the interrupt on a given port and pin.
 * @param      cfg       Pointer to configuration structure describing the pin
 * @param      callback  A pointer to a function of type #callback_fn.
 * @param      cbdata    The parameter to be passed to the callback function,
 * #callback_fn, when an interrupt occurs.
 *
 */
void GPIO_RegisterCallback(const gpio_cfg_t *cfg, gpio_callback_fn callback,
			   void *cbdata);

/**
 * @brief      GPIO IRQ Handler. @note If a callback is registered for a given
 *             interrupt, the callback function will be called.
 *
 * @param      port number of the port that generated the interrupt service
 * routine.
 *
 */
void GPIO_Handler(unsigned int port);

#endif /* _GPIO_API_ */
