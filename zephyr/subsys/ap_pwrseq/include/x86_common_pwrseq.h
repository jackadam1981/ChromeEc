/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __X86_COMMON_H__
#define __X86_COMMON_H__

#include <drivers/espi.h>
#include <drivers/gpio.h>

/* Power signal GPIO configuration */
struct gpio_config {
	/* GPIO net name */
	const char *net_name;
	/* GPIO pin port name */
	const char *port_name;
	/* GPIO pin index */
	const gpio_pin_t pin;
	/* GPIO configuration flags */
	const gpio_flags_t flags;
	/* Device structure for the driver instance */
	const struct device *port;
};

struct gpio_interrupt_config {
	/* GPIO net name */
	const char *net_name;
	/* GPIO configuration */
	const struct gpio_config *config;
	/* GPIO callback */
	struct gpio_callback intr_cb;
	/* GPIO interrupt flags */
	const gpio_flags_t intr_flags;
	/* Disable at boot up */
	const bool disable_at_boot;
};

/* Power signals list */
enum power_signal {
	X86_SLP_S0_DEASSERTED,
	X86_SLP_S3_DEASSERTED,
	X86_SLP_S4_DEASSERTED,
	X86_SLP_S5_DEASSERTED,
	X86_SLP_SUS_DEASSERTED,
	X86_RSMRST_L_PGOOD,
	X86_DSW_PWROK,
	X86_ALL_SYS_PGOOD,
	/* X86 signals count, GPIO and VW */
	POWER_SIGNAL_COUNT
};

/* Information of a GPIO power signal */
struct power_signal_gpio_info {
	const char *net_name;   /* GPIO net name of signal */
	enum power_signal power_sig;        /* Power signal*/
	uint32_t flags;		/* See POWER_SIGNAL_* macros */
	const char *name;
};

/* Information of a virtual wire power signal */
struct power_signal_vw_info {
	enum espi_vwire_signal vw_signal; /* ESPI VW signal */
	enum power_signal power_sig;      /* Power signal */
	uint32_t flags;	        /* See POWER_SIGNAL_* macros */
	const char *name;
};

/**
 * @brief System power states for Non Deep Sleep Well
 * EC is an always on device in a Non Deep Sx system except when EC
 * is hibernated or all the VRs are turned off.
 */
enum power_states_ndsx {
	/*
	 * Actual power states
	 */
	/* AP is off & EC is on */
	SYS_POWER_STATE_G3,
	/* AP is in soft off state */
	SYS_POWER_STATE_S5,
	/* AP is suspended to Non-volatile disk */
	SYS_POWER_STATE_S4,
	/* AP is suspended to RAM */
	SYS_POWER_STATE_S3,
	/* AP is in active state */
	SYS_POWER_STATE_S0,
#ifdef CONFIG_PLATFORM_EC_POWERSEQ_S0IX
	/* AP is in standby; cache is flushed to RAM */
	SYS_POWER_STATE_S0ix,
#endif

	/*
	 * Intermediate power up states
	 */
	/* Determine if the AP's power rails are turned on */
	SYS_POWER_STATE_G3S5,
	/* Determine if AP is suspended from sleep */
	SYS_POWER_STATE_S5S4,
	/* Determine if Suspend to Disk is de-asserted */
	SYS_POWER_STATE_S4S3,
	/* Determine if Suspend to RAM is de-asserted */
	SYS_POWER_STATE_S3S0,
#ifdef CONFIG_PLATFORM_EC_POWERSEQ_S0IX
	SYS_POWER_STATE_S0ixS0,
#endif

	/*
	 * Intermediate power down states
	 */
	/* Determine if the AP's power rails are turned off */
	SYS_POWER_STATE_S5G3,
	/* Determine if AP is suspended to sleep */
	SYS_POWER_STATE_S4S5,
	/* Determine if Suspend to Disk is asserted */
	SYS_POWER_STATE_S3S4,
	/* Determine if Suspend to RAM is asserted */
	SYS_POWER_STATE_S0S3,
#ifdef CONFIG_PLATFORM_EC_POWERSEQ_S0IX
	SYS_POWER_STATE_S0S0ix,
#endif
};

/*
 * AP hard shutdowns are logged on the same path as resets.
 */
enum chipset_shutdown_reason {
	CHIPSET_SHUTDOWN_BEGIN = BIT(15),
	CHIPSET_SHUTDOWN_POWERFAIL = CHIPSET_SHUTDOWN_BEGIN,
	/* Forcing a shutdown as part of EC initialization */
	CHIPSET_SHUTDOWN_INIT,
	/* Custom reason on a per-board basis. */
	CHIPSET_SHUTDOWN_BOARD_CUSTOM,
	/* This is a reason to inhibit startup, not cause shut down. */
	CHIPSET_SHUTDOWN_BATTERY_INHIBIT,
	/* A power_wait_signal is being asserted */
	CHIPSET_SHUTDOWN_WAIT,
	/* Critical battery level. */
	CHIPSET_SHUTDOWN_BATTERY_CRIT,
	/* Because you told me to. */
	CHIPSET_SHUTDOWN_CONSOLE_CMD,
	/* Forcing a shutdown to effect entry to G3. */
	CHIPSET_SHUTDOWN_G3,
	/* Force shutdown due to over-temperature. */
	CHIPSET_SHUTDOWN_THERMAL,
	/* Force a chipset shutdown from the power button through EC */
	CHIPSET_SHUTDOWN_BUTTON,

	CHIPSET_SHUTDOWN_COUNT,
};

/* Common device tree configurable attributes */
struct common_pwrseq_config {
	int pch_dsw_pwrok_delay_ms;
	int pch_pm_pwrbtn_delay_ms;
	int pch_rsmrst_delay_ms;
	int vr_en_vccin_delay_ms;
	/* Default timeout to wait for power signal */
	int wait_signal_timeout_ms;
};

/* This encapsulates the attributes of the state machine */
struct power_seq_context {
	/* On power-on start boot up sequence */
	enum power_states_ndsx power_state;

	/*
	 * Current input power signal states. Each bit represents an input
	 * power signal that is defined by enum power_signal in same order.
	 * 1 - signal state is asserted.
	 * 0 - signal state is de-asserted.
	 */
	uint32_t in_signals;
	/* Input signal state we're waiting for */
	uint32_t in_want;
	/* Signal values which print debug output */
	uint32_t in_debug;

	/* S5 inactive time in seconds before power state change */
	int s5_timeout_s;
};

enum power_states_ndsx pwr_sm_get_state(void);
void pwr_sm_set_state(enum power_states_ndsx new_state);
int chipset_in_state(int state_mask);
int chipset_in_or_transitioning_to_state(int state_mask);


/* Below code are pulled from headers defined in shim code */
/********************************************************************/
/* /ec_commands.h */
/*
 * Host event codes. ACPI query EC command uses code 0 to mean "no event
 * pending".  We explicitly specify each value in the enum listing so they won't
 * change if we delete/insert an item or rearrange the list (it needs to be
 * stable across platforms, not just within a single compiled instance).
 */
enum host_event_code {
	EC_HOST_EVENT_NONE = 0,
	EC_HOST_EVENT_LID_CLOSED = 1,
	EC_HOST_EVENT_LID_OPEN = 2,
	EC_HOST_EVENT_POWER_BUTTON = 3,
	EC_HOST_EVENT_AC_CONNECTED = 4,
	EC_HOST_EVENT_AC_DISCONNECTED = 5,
	EC_HOST_EVENT_BATTERY_LOW = 6,
	EC_HOST_EVENT_BATTERY_CRITICAL = 7,
	EC_HOST_EVENT_BATTERY = 8,
	EC_HOST_EVENT_THERMAL_THRESHOLD = 9,
	/* Event generated by a device attached to the EC */
	EC_HOST_EVENT_DEVICE = 10,
	EC_HOST_EVENT_THERMAL = 11,
	EC_HOST_EVENT_USB_CHARGER = 12,
	EC_HOST_EVENT_KEY_PRESSED = 13,
	/*
	 * EC has finished initializing the host interface.  The host can check
	 * for this event following sending a EC_CMD_REBOOT_EC command to
	 * determine when the EC is ready to accept subsequent commands.
	 */
	EC_HOST_EVENT_INTERFACE_READY = 14,
	/* Keyboard recovery combo has been pressed */
	EC_HOST_EVENT_KEYBOARD_RECOVERY = 15,
	/* Shutdown due to thermal overload */
	EC_HOST_EVENT_THERMAL_SHUTDOWN = 16,
	/* Shutdown due to battery level too low */
	EC_HOST_EVENT_BATTERY_SHUTDOWN = 17,
	/* Suggest that the AP throttle itself */
	EC_HOST_EVENT_THROTTLE_START = 18,
	/* Suggest that the AP resume normal speed */
	EC_HOST_EVENT_THROTTLE_STOP = 19,
	/* Hang detect logic detected a hang and host event timeout expired */
	EC_HOST_EVENT_HANG_DETECT = 20,
	/* Hang detect logic detected a hang and warm rebooted the AP */
	EC_HOST_EVENT_HANG_REBOOT = 21,
	/* PD MCU triggering host event */
	EC_HOST_EVENT_PD_MCU = 22,
	/* Battery Status flags have changed */
	EC_HOST_EVENT_BATTERY_STATUS = 23,
	/* EC encountered a panic, triggering a reset */
	EC_HOST_EVENT_PANIC = 24,
	/* Keyboard fastboot combo has been pressed */
	EC_HOST_EVENT_KEYBOARD_FASTBOOT = 25,
	/* EC RTC event occurred */
	EC_HOST_EVENT_RTC = 26,
	/* Emulate MKBP event */
	EC_HOST_EVENT_MKBP = 27,
	/* EC desires to change state of host-controlled USB mux */
	EC_HOST_EVENT_USB_MUX = 28,
	/*
	 * The device has changed "modes". This can be one of the following:
	 *
	 * - TABLET/LAPTOP mode
	 * - detachable base attach/detach event
	 */
	EC_HOST_EVENT_MODE_CHANGE = 29,
	/* Keyboard recovery combo with hardware reinitialization */
	EC_HOST_EVENT_KEYBOARD_RECOVERY_HW_REINIT = 30,
	/* WoV */
	EC_HOST_EVENT_WOV = 31,
	/*
	 * The high bit of the event mask is not used as a host event code.  If
	 * it reads back as set, then the entire event mask should be
	 * considered invalid by the host.  This can happen when reading the
	 * raw event status via EC_MEMMAP_HOST_EVENTS but the LPC interface is
	 * not initialized on the EC, or improperly configured on the host.
	 */
	EC_HOST_EVENT_INVALID = 32
};

/* Host event mask */
#define EC_HOST_EVENT_MASK(event_code) BIT64((event_code) - 1)

enum host_sleep_event {
	HOST_SLEEP_EVENT_DEFAULT_RESET = 0,
	HOST_SLEEP_EVENT_S3_SUSPEND   = 1,
	HOST_SLEEP_EVENT_S3_RESUME    = 2,
	HOST_SLEEP_EVENT_S0IX_SUSPEND = 3,
	HOST_SLEEP_EVENT_S0IX_RESUME  = 4,
	/* S3 suspend with additional enabled wake sources */
	HOST_SLEEP_EVENT_S3_WAKEABLE_SUSPEND = 5,
};


/********************************************************************/
/* hook.h */
enum hook_type {
	HOOK_CHIPSET_RESUME = 6,
	HOOK_CHIPSET_SUSPEND,
};

/********************************************************************/
/* chipset.h */
/* Chipset state mask */
enum chipset_state_mask {
	CHIPSET_STATE_HARD_OFF = 0x01,   /* Hard off (G3) */
	CHIPSET_STATE_SOFT_OFF = 0x02,   /* Soft off (S5) */
	CHIPSET_STATE_SUSPEND  = 0x04,   /* Suspend (S3) */
	CHIPSET_STATE_ON       = 0x08,   /* On (S0) */
	CHIPSET_STATE_STANDBY  = 0x10,   /* Standby (S0ix) */
	/* Common combinations */
	CHIPSET_STATE_ANY_OFF = (CHIPSET_STATE_HARD_OFF |
				 CHIPSET_STATE_SOFT_OFF),  /* Any off state */
	/* This combination covers any kind of suspend i.e. S3 or S0ix. */
	CHIPSET_STATE_ANY_SUSPEND = (CHIPSET_STATE_SUSPEND |
				     CHIPSET_STATE_STANDBY),
};

/********************************************************************/
/* power.h */
enum power_state {
	/* Steady states */
	POWER_G3 = 0,	/*
			 * System is off (not technically all the way into G3,
			 * which means totally unpowered...)
			 */
	POWER_S5,		/* System is soft-off */
	POWER_S4,		/* System is suspended to disk */
	POWER_S3,		/* Suspend; RAM on, processor is asleep */
	POWER_S0,		/* System is on */
#ifdef CONFIG_PLATFORM_EC_POWERSEQ_S0IX
	POWER_S0ix,
#endif
	/* Transitions */
	POWER_G3S5,	/* G3 -> S5 (at system init time) */
	POWER_S5S3,	/* S5 -> S3 (skips S4 on non-Intel systems) */
	POWER_S3S0,	/* S3 -> S0 */
	POWER_S0S3,	/* S0 -> S3 */
	POWER_S3S5,	/* S3 -> S5 (skips S4 on non-Intel systems) */
	POWER_S5G3,	/* S5 -> G3 */
	POWER_S3S4,	/* S3 -> S4 */
	POWER_S4S3,	/* S4 -> S3 */
	POWER_S4S5,	/* S4 -> S5 */
	POWER_S5S4,	/* S5 -> S4 */
#ifdef CONFIG_PLATFORM_EC_POWERSEQ_S0IX
	POWER_S0ixS0,   /* S0ix -> S0 */
	POWER_S0S0ix,   /* S0 -> S0ix */
#endif
};

static inline enum power_state convert_native_power_state_to_shim
	(enum power_states_ndsx native_state )
{
	switch (native_state) {
	case SYS_POWER_STATE_S5:
		return POWER_S5;
	case SYS_POWER_STATE_S3:
			return POWER_S3;
#ifdef CONFIG_PLATFORM_EC_POWERSEQ_S0IX
	case SYS_POWER_STATE_S0ix:
		return POWER_S0ix;
#endif
	default:
		return 0;
	}
}
#if 0
enum host_sleep_event {
	HOST_SLEEP_EVENT_DEFAULT_RESET = 0,
	HOST_SLEEP_EVENT_S3_SUSPEND   = 1,
	HOST_SLEEP_EVENT_S3_RESUME    = 2,
	HOST_SLEEP_EVENT_S0IX_SUSPEND = 3,
	HOST_SLEEP_EVENT_S0IX_RESUME  = 4,
	/* S3 suspend with additional enabled wake sources */
	HOST_SLEEP_EVENT_S3_WAKEABLE_SUSPEND = 5,
};
#endif
/* Host sleep */
enum sleep_notify_type {
	SLEEP_NOTIFY_NONE,
	SLEEP_NOTIFY_SUSPEND,
	SLEEP_NOTIFY_RESUME,
};

#ifdef CONFIG_PLATFORM_EC_POWERSEQ_HOST_SLEEP
/* Context to pass to a host sleep command handler. */
struct host_sleep_event_context {
	uint32_t sleep_transitions; /* Number of sleep transitions observed */
	uint16_t sleep_timeout_ms;  /* Timeout in milliseconds */
};

void power_chipset_handle_host_sleep_event(
		enum host_sleep_event state,
		struct host_sleep_event_context *ctx);

void power_board_handle_host_sleep_event(
		enum host_sleep_event state);

enum host_sleep_event power_get_host_sleep_state(void);
void power_set_host_sleep_state(enum host_sleep_event state);
#ifdef CONFIG_PLATFORM_EC_POWERSEQ_S0IX
void power_reset_host_sleep_state(void);
#endif /* CONFIG_PLATFORM_EC_POWERSEQ_S0IX */
void sleep_set_notify(enum sleep_notify_type notify);
void sleep_notify_transition(int check_state, int hook_id);
void sleep_suspend_transition(void);
void sleep_resume_transition(void);
void sleep_start_suspend(struct host_sleep_event_context *ctx,
			 void (*callback)(void));
void sleep_complete_resume(struct host_sleep_event_context *ctx);
void sleep_reset_tracking(void);
#endif /* CONFIG_PLATFORM_EC_POWERSEQ_HOST_SLEEP */

/********************************************************************/
/* system.h */
/* Low power modes for idle API */
enum {
	/*
	 * Sleep masks to prevent going in to deep sleep.
	 */
	SLEEP_MASK_AP_RUN     = BIT(0), /* the main CPU is running */
	SLEEP_MASK_UART       = BIT(1), /* UART communication ongoing */
	SLEEP_MASK_I2C_CONTROLLER = BIT(2), /* I2C controller comms ongoing */
	SLEEP_MASK_CHARGING   = BIT(3), /* Charging loop ongoing */
	SLEEP_MASK_USB_PWR    = BIT(4), /* USB power loop ongoing */
	SLEEP_MASK_USB_PD     = BIT(5), /* USB PD device connected */
	SLEEP_MASK_SPI        = BIT(6), /* SPI communications ongoing */
	SLEEP_MASK_I2C_PERIPHERAL = BIT(7), /* I2C peripheral comms ongoing */
	SLEEP_MASK_FAN        = BIT(8), /* Fan control loop ongoing */
	SLEEP_MASK_USB_DEVICE = BIT(9), /* Generic USB device in use */
	SLEEP_MASK_PWM        = BIT(10), /* PWM output is enabled */
	SLEEP_MASK_PHYSICAL_PRESENCE  = BIT(11), /* Physical presence
						    * detection ongoing */
	SLEEP_MASK_PLL        = BIT(12), /* High-speed PLL in-use */
	SLEEP_MASK_ADC        = BIT(13), /* ADC conversion ongoing */
	SLEEP_MASK_EMMC       = BIT(14), /* eMMC emulation ongoing */
	SLEEP_MASK_FORCE_NO_DSLEEP    = BIT(15), /* Force disable. */
	/*
	 * Sleep masks to prevent using slow speed clock in deep sleep.
	 */
	SLEEP_MASK_JTAG     = BIT(16), /* JTAG is in use. */
	SLEEP_MASK_CONSOLE  = BIT(17), /* Console is in use. */
	SLEEP_MASK_FORCE_NO_LOW_SPEED = BIT(31)  /* Force disable. */
};
//static inline void enable_sleep(uint32_t mask);
//static inline void disable_sleep(uint32_t mask);

/********************************************************************/
/* host_command.h */
typedef uint64_t host_event_t;
extern void host_set_single_event(enum host_event_code event);
extern uint8_t lpc_is_active_wm_set_by_host(void);
extern int get_lazy_wake_mask(enum power_state state, host_event_t *mask);

/********************************************************************/
/* lpc.h */
/* Types of host events */
enum lpc_host_event_type {
	LPC_HOST_EVENT_SMI = 0,
	LPC_HOST_EVENT_SCI,
	LPC_HOST_EVENT_WAKE,
	LPC_HOST_EVENT_ALWAYS_REPORT,
	LPC_HOST_EVENT_COUNT,
};
extern host_event_t lpc_get_host_event_mask(enum lpc_host_event_type type);
extern void lpc_set_host_event_mask(enum lpc_host_event_type type, host_event_t mask);

/********************************************************************/
/* wireless.h */
/* Wireless power state for wireless_set_state() */
enum wireless_power_state {
	WIRELESS_OFF,
	WIRELESS_SUSPEND,
	WIRELESS_ON
};
static inline void wireless_set_state(enum wireless_power_state state) { }

#endif /* __X86_COMMON_H__ */
