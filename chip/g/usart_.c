# 1 "chip/g/usart.c"
# 1 "/home/dnojiri/trunk/src/platform/ec//"
# 1 "<built-in>"
# 1 "<command-line>"
# 1 "chip/g/usart.c"

# 1 "include/queue.h" 1
# 10 "include/queue.h"
# 1 "include/common.h" 1
# 11 "include/common.h"
# 1 "builtin/stdint.h" 1
# 9 "builtin/stdint.h"
typedef unsigned char uint8_t;
typedef signed char int8_t;

typedef unsigned short uint16_t;
typedef signed short int16_t;

typedef unsigned int uint32_t;
typedef signed int int32_t;

typedef unsigned long long uint64_t;
typedef signed long long int64_t;

typedef unsigned int uintptr_t;
typedef int intptr_t;

typedef uint8_t uint_least8_t;
typedef uint16_t uint_least16_t;
typedef uint32_t uint_least32_t;
typedef uint64_t uint_least64_t;

typedef int8_t int_least8_t;
typedef int16_t int_least16_t;
typedef int32_t int_least32_t;
typedef int64_t int_least64_t;

typedef uint8_t uint_fast8_t;
typedef uint16_t uint_fast16_t;
typedef uint32_t uint_fast32_t;
typedef uint64_t uint_fast64_t;

typedef int8_t int_fast8_t;
typedef int16_t int_fast16_t;
typedef int32_t int_fast32_t;
typedef int64_t int_fast64_t;
# 12 "include/common.h" 2
# 131 "include/common.h"
# 1 "include/config.h" 1
# 3788 "include/config.h"
# 1 "chip/g/config_chip.h" 1
# 10 "chip/g/config_chip.h"
# 1 "./core/cortex-m/config_core.h" 1
# 11 "chip/g/config_chip.h" 2
# 1 "chip/g/hw_regdefs.h" 1
# 12 "chip/g/config_chip.h" 2
# 3789 "include/config.h" 2
# 1 "board/cr50/board.h" 1
# 154 "board/cr50/board.h"
# 1 "include/gpio_signal.h" 1
# 13 "include/gpio_signal.h"
enum gpio_signal
{
# 1 "include/gpio.wrap" 1
# 117 "include/gpio.wrap"
# 1 "board/cr50/gpio.inc" 1
# 58 "board/cr50/gpio.inc"
	GPIO_TPM_RST_L, GPIO_DETECT_AP, GPIO_DETECT_EC,

	GPIO_DETECT_SERVO,

	GPIO_CCD_MODE_L,

	GPIO_EC_TX_CR50_RX,

	GPIO_INT_AP_L,

	GPIO_EC_FLASH_SELECT, GPIO_AP_FLASH_SELECT,

	GPIO_SYS_RST_L_OUT,
# 95 "board/cr50/gpio.inc"
	GPIO_BATT_PRES_L,

	GPIO_SPI_MOSI, GPIO_SPI_CLK, GPIO_SPI_CS_L,

	GPIO_DIOB4,

	GPIO_STRAP_A0, GPIO_STRAP_A1, GPIO_STRAP_B0, GPIO_STRAP_B1,

	GPIO_EN_PP3300_INA_L,

	GPIO_I2C_SCL_INA, GPIO_I2C_SDA_INA,
# 130 "board/cr50/gpio.inc"
	GPIO_I2CS_SDA,

	GPIO_EC_TX_CR50_RX_OUT,

	GPIO_ENTERING_RW,
# 152 "board/cr50/gpio.inc"

# 202 "board/cr50/gpio.inc"

# 238 "board/cr50/gpio.inc"

# 118 "include/gpio.wrap" 2
# 15 "include/gpio_signal.h" 2
	GPIO_COUNT
};
# 155 "board/cr50/board.h" 2

enum usb_strings
{
	USB_STR_DESC = 0,
	USB_STR_VENDOR,
	USB_STR_PRODUCT,
	USB_STR_VERSION,
	USB_STR_CONSOLE_NAME,
	USB_STR_BLOB_NAME,
	USB_STR_HID_KEYBOARD_NAME,
	USB_STR_AP_NAME,
	USB_STR_EC_NAME,
	USB_STR_UPGRADE_NAME,
	USB_STR_SPI_NAME,
	USB_STR_SERIALNO,
	USB_STR_I2C_NAME,

	USB_STR_COUNT
};

enum device_state
{

	DEVICE_STATE_INIT = 0,

	DEVICE_STATE_INIT_DEBOUNCING,

	DEVICE_STATE_INIT_RX_ONLY,

	DEVICE_STATE_DISCONNECTED, DEVICE_STATE_OFF,

	DEVICE_STATE_UNDETECTABLE,

	DEVICE_STATE_CONNECTED, DEVICE_STATE_ON,

	DEVICE_STATE_DEBOUNCING,

	DEVICE_STATE_UNKNOWN,

	DEVICE_STATE_COUNT
};

const char *device_state_name(enum device_state state);

enum nvmem_vars
{
	NVMEM_VAR_CONSOLE_LOCKED = 0,
	NVMEM_VAR_TEST_VAR,
	NVMEM_VAR_U2F_SALT,
	NVMEM_VAR_CCD_CONFIG,

	NVMEM_VARS_COUNT
};

void board_configure_deep_sleep_wakepins(void);
void ap_detect_asserted(enum gpio_signal signal);
void ec_detect_asserted(enum gpio_signal signal);
void ccd_mode_asserted(enum gpio_signal signal);
void ec_tx_cr50_rx(enum gpio_signal signal);
void servo_detect_asserted(enum gpio_signal signal);
void tpm_rst_deasserted(enum gpio_signal signal);

void post_reboot_request(void);

void assert_sys_rst(void);
void deassert_sys_rst(void);
int is_sys_rst_asserted(void);
void assert_ec_rst(void);
void deassert_ec_rst(void);
int is_ec_rst_asserted(void);

void ccd_update_state(void);

int board_use_plt_rst(void);

int board_rst_pullup_needed(void);

int board_tpm_uses_i2c(void);

int board_tpm_uses_spi(void);

int board_uses_closed_source_set1(void);
int board_id_is_mismatched(void);

int board_deep_sleep_allowed(void);

void power_button_record(void);

void power_button_release_enable_interrupt(int enable);

int board_battery_is_present(void);
int board_fwmp_allows_unlock(void);
int board_vboot_dev_mode_enabled(void);
void board_reboot_ap(void);
int board_wipe_tpm(void);
int board_is_first_factory_boot(void);

int usb_i2c_board_enable(void);
void usb_i2c_board_disable(void);

void print_ap_state(void);
void print_ap_uart_state(void);
void print_ec_state(void);
void print_servo_state(void);

int ap_is_on(void);
int ap_uart_is_on(void);
int ec_is_on(void);
int ec_is_rx_allowed(void);
int servo_is_connected(void);
int ec_is_speaking(void);

void set_ap_on(void);

int chip_factory_mode(void);

void board_start_ite_sync(void);
# 373 "board/cr50/board.h"
enum nvmem_users
{
	NVMEM_TPM = 0, NVMEM_CR50, NVMEM_NUM_USERS
};
# 3790 "include/config.h" 2
# 4144 "include/config.h"
# 1 "fuzz/fuzz_config.h" 1
# 4145 "include/config.h" 2
# 1 "test/test_config.h" 1
# 4146 "include/config.h" 2
# 132 "include/common.h" 2

# 1 "include/module_id.h" 1
# 11 "include/module_id.h"
# 1 "include/common.h" 1
# 12 "include/module_id.h" 2

enum module_id
{
	MODULE_ADC,
	MODULE_CHARGER,
	MODULE_CHIPSET,
	MODULE_CLOCK,
	MODULE_COMMAND,
	MODULE_DMA,
	MODULE_EXTPOWER,
	MODULE_FAST_CPU,
	MODULE_GPIO,
	MODULE_HOOK,
	MODULE_HOST_COMMAND,
	MODULE_HOST_EVENT,
	MODULE_I2C,
	MODULE_I2C_TIMERS,
	MODULE_KEYBOARD,
	MODULE_KEYBOARD_SCAN,
	MODULE_LIGHTBAR,
	MODULE_LPC,
	MODULE_MCO,
	MODULE_PECI,
	MODULE_PMU,
	MODULE_PORT80,
	MODULE_POWER_LED,
	MODULE_PWM,
	MODULE_RDD,
	MODULE_RBOX,
	MODULE_SPI,
	MODULE_SPI_FLASH,
	MODULE_SPI_MASTER,
	MODULE_SWITCH,
	MODULE_SYSTEM,
	MODULE_TASK,
	MODULE_TFDP,
	MODULE_THERMAL,
	MODULE_UART,
	MODULE_USART,
	MODULE_USB,
	MODULE_USB_DEBUG,
	MODULE_USB_PD,
	MODULE_USB_PORT_POWER,
	MODULE_USB_SWITCH,
	MODULE_VBOOT,
	MODULE_WOV,

	MODULE_COUNT
};
# 135 "include/common.h" 2

enum ec_error_list
{

	EC_SUCCESS = 0,

	EC_ERROR_UNKNOWN = 1,

	EC_ERROR_UNIMPLEMENTED = 2,

	EC_ERROR_OVERFLOW = 3,

	EC_ERROR_TIMEOUT = 4,

	EC_ERROR_INVAL = 5,

	EC_ERROR_BUSY = 6,

	EC_ERROR_ACCESS_DENIED = 7,

	EC_ERROR_NOT_POWERED = 8,

	EC_ERROR_NOT_CALIBRATED = 9,

	EC_ERROR_CRC = 10,

	EC_ERROR_PARAM1 = 11,
	EC_ERROR_PARAM2 = 12,
	EC_ERROR_PARAM3 = 13,
	EC_ERROR_PARAM4 = 14,
	EC_ERROR_PARAM5 = 15,
	EC_ERROR_PARAM6 = 16,
	EC_ERROR_PARAM7 = 17,
	EC_ERROR_PARAM8 = 18,
	EC_ERROR_PARAM9 = 19,

	EC_ERROR_PARAM_COUNT = 20,

	EC_ERROR_NOT_HANDLED = 21,

	EC_ERROR_UNCHANGED = 22,

	EC_ERROR_MEMORY_ALLOCATION = 23,

	EC_ERROR_INVALID_CONFIG = 24,

	EC_ERROR_HW_INTERNAL = 25,

	EC_ERROR_VBOOT_SIGNATURE = 0x1000,
	EC_ERROR_VBOOT_SIG_MAGIC = 0x1001,
	EC_ERROR_VBOOT_SIG_SIZE = 0x1002,
	EC_ERROR_VBOOT_SIG_ALGORITHM = 0x1003,
	EC_ERROR_VBOOT_HASH_ALGORITHM = 0x1004,
	EC_ERROR_VBOOT_SIG_OFFSET = 0x1005,
	EC_ERROR_VBOOT_DATA_SIZE = 0x1006,

	EC_ERROR_VBOOT_KEY = 0x1100,
	EC_ERROR_VBOOT_KEY_MAGIC = 0x1101,
	EC_ERROR_VBOOT_KEY_SIZE = 0x1102,

	EC_ERROR_VBOOT_DATA = 0x1200,
	EC_ERROR_VBOOT_DATA_VERIFY = 0x1201,

	EC_ERROR_INTERNAL_FIRST = 0x10000,
	EC_ERROR_INTERNAL_LAST = 0x1FFFF
};
# 11 "include/queue.h" 2

# 1 "builtin/stddef.h" 1
# 13 "builtin/stddef.h"
typedef unsigned int size_t;

typedef signed int ssize_t;
# 28 "builtin/stddef.h"
typedef short unsigned int wchar_t;
# 13 "include/queue.h" 2
# 32 "include/queue.h"
struct queue_policy
{
	void (*add)(struct queue_policy const *queue_policy, size_t count);
	void (*remove)(struct queue_policy const *queue_policy, size_t count);
};
# 44 "include/queue.h"
extern struct queue_policy const queue_policy_null;

struct queue_state
{
# 65 "include/queue.h"
	size_t head;
	size_t tail;
};

struct queue
{
	struct queue_state volatile *state;

	struct queue_policy const *policy;

	size_t buffer_units;
	size_t unit_bytes;
	uint8_t *buffer;
};
# 97 "include/queue.h"
void queue_init(struct queue const *q);

int queue_is_empty(struct queue const *q);

size_t queue_count(struct queue const *q);

size_t queue_space(struct queue const *q);

int queue_is_full(struct queue const *q);

struct queue_chunk
{
	size_t length;
	uint8_t *buffer;
};
# 132 "include/queue.h"
struct queue_chunk queue_get_write_chunk(struct queue const *q);
# 143 "include/queue.h"
struct queue_chunk queue_get_read_chunk(struct queue const *q);

size_t queue_advance_head(struct queue const *q, size_t count);

size_t queue_advance_tail(struct queue const *q, size_t count);

size_t queue_add_unit(struct queue const *q, const void *src);

size_t queue_add_units(struct queue const *q, const void *src, size_t count);

size_t queue_add_memcpy(struct queue const *q, const void *src, size_t count,
			void *(*memcpy)(void *dest, const void *src, size_t n));

size_t queue_remove_unit(struct queue const *q, void *dest);

size_t queue_remove_units(struct queue const *q, void *dest, size_t count);

size_t queue_remove_memcpy(
		struct queue const *q, void *dest, size_t count,
		void *(*memcpy)(void *dest, const void *src, size_t n));

size_t queue_peek_units(struct queue const *q, void *dest, size_t i,
			size_t count);

size_t queue_peek_memcpy(struct queue const *q, void *dest, size_t i,
			 size_t count,
			 void *(*memcpy)(void *dest, const void *src, size_t n));
# 7 "chip/g/usart.c" 2
# 1 "include/queue_policies.h" 1
# 10 "include/queue_policies.h"
# 1 "include/queue.h" 1
# 11 "include/queue_policies.h" 2
# 1 "include/consumer.h" 1
# 19 "include/consumer.h"
struct consumer;
struct producer;

struct consumer_ops
{

	void (*written)(struct consumer const *consumer, size_t count);
};

struct consumer
{

	struct queue const *queue;

	struct consumer_ops const *ops;
};
# 12 "include/queue_policies.h" 2
# 1 "include/producer.h" 1
# 19 "include/producer.h"
struct consumer;
struct producer;

struct producer_ops
{

	void (*read)(struct producer const *producer, size_t count);
};

struct producer
{

	struct queue const *queue;

	struct producer_ops const *ops;
};
# 13 "include/queue_policies.h" 2

struct queue_policy_direct
{
	struct queue_policy policy;

	struct producer const *producer;
	struct consumer const *consumer;
};

void queue_add_direct(struct queue_policy const *policy, size_t count);
void queue_remove_direct(struct queue_policy const *policy, size_t count);
# 48 "include/queue_policies.h"
extern struct producer const null_producer;
extern struct consumer const null_consumer;
# 8 "chip/g/usart.c" 2

# 1 "chip/g/uart_bitbang.h" 1
# 11 "chip/g/uart_bitbang.h"
# 1 "include/common.h" 1
# 12 "chip/g/uart_bitbang.h" 2
# 1 "include/gpio.h" 1
# 68 "include/gpio.h"
struct gpio_info
{

	const char *name;

	uint32_t port;

	uint32_t mask;

	uint32_t flags;
};

extern const struct gpio_info gpio_list[];

extern void (* const gpio_irq_handlers[])(enum gpio_signal signal);
extern const int gpio_ih_count;

void gpio_pre_init(void);
# 108 "include/gpio.h"
int gpio_config_module(enum module_id id, int enable);
# 118 "include/gpio.h"
int gpio_config_pin(enum module_id id, enum gpio_signal signal, int enable);

int gpio_get_level(enum gpio_signal signal);
# 135 "include/gpio.h"
int gpio_get_ternary(enum gpio_signal signal);

const char *gpio_get_name(enum gpio_signal signal);
# 154 "include/gpio.h"
int gpio_is_implemented(enum gpio_signal signal);

void gpio_set_flags(enum gpio_signal signal, int flags);
# 188 "include/gpio.h"
int gpio_get_default_flags(enum gpio_signal signal);

void gpio_set_level(enum gpio_signal signal, int value);
# 206 "include/gpio.h"
void gpio_reset(enum gpio_signal signal);
# 218 "include/gpio.h"
int gpio_enable_interrupt(enum gpio_signal signal);
# 230 "include/gpio.h"
int gpio_disable_interrupt(enum gpio_signal signal);
# 240 "include/gpio.h"
int gpio_clear_pending_interrupt(enum gpio_signal signal);
# 255 "include/gpio.h"
void gpio_set_flags_by_mask(uint32_t port, uint32_t mask, uint32_t flags);
# 270 "include/gpio.h"
void gpio_set_alternate_function(uint32_t port, uint32_t mask, int func);
# 13 "chip/g/uart_bitbang.h" 2

struct uart_bitbang_properties
{
	enum gpio_signal tx_gpio;
	enum gpio_signal rx_gpio;
	uint32_t tx_pinmux_reg;
	uint32_t tx_pinmux_regval;
	uint32_t rx_pinmux_reg;
	uint32_t rx_pinmux_regval;
	struct queue const *uart_in;
	uint32_t baud_rate;
	uint16_t rx_irq;
	uint8_t uart;
	uint8_t parity;
};

extern struct uart_bitbang_properties bitbang_config;

int uart_bitbang_enable(void);

int uart_bitbang_disable(void);

int uart_bitbang_is_enabled(void);

int uart_bitbang_is_wanted(void);
# 61 "chip/g/uart_bitbang.h"
void uart_bitbang_irq(void);

void uart_bitbang_drain_tx_queue(struct queue const *q);
# 13 "chip/g/usart.c" 2

# 1 "chip/g/uartn.h" 1
# 14 "chip/g/uartn.h"
void uartn_init(int uart);

void uartn_tx_flush(int uart);

int uartn_tx_ready(int uart);

int uartn_tx_in_progress(int uart);

int uartn_rx_available(int uart);
# 43 "chip/g/uartn.h"
void uartn_write_char(int uart, char c);

int uartn_read_char(int uart);

void uartn_disable_interrupt(int uart);

void uartn_enable_interrupt(int uart);

void uartn_tx_start(int uart);

void uartn_tx_stop(int uart);

int uart_tx_is_connected(int uart);

void uartn_tx_connect(int uart);

void uartn_tx_disconnect(int uart);
# 100 "chip/g/uartn.h"
int uartn_is_enabled(int uart);
# 110 "chip/g/uartn.h"
void uartn_enable(int uart);
# 120 "chip/g/uartn.h"
void uartn_disable(int uart);
# 15 "chip/g/usart.c" 2
# 1 "chip/g/usart.h" 1

# 1 "include/consumer.h" 1
# 7 "chip/g/usart.h" 2
# 1 "include/producer.h" 1
# 8 "chip/g/usart.h" 2
# 1 "chip/g/registers.h" 1
# 11 "chip/g/registers.h"
# 1 "include/util.h" 1
# 12 "include/util.h"
# 1 "include/compile_time_macros.h" 1
# 13 "include/util.h" 2
# 1 "include/panic.h" 1
# 12 "include/panic.h"
# 1 "include/software_panic.h" 1
# 13 "include/panic.h" 2

# 1 "builtin/stdarg.h" 1
# 19 "builtin/stdarg.h"
typedef __builtin_va_list va_list;
# 15 "include/panic.h" 2

struct cortex_panic_data
{
	uint32_t regs[12];

	uint32_t frame[8];

	uint32_t mmfs;
	uint32_t bfar;
	uint32_t mfar;
	uint32_t shcsr;
	uint32_t hfsr;
	uint32_t dfsr;
};

struct nds32_n8_panic_data
{
	uint32_t itype;
	uint32_t regs[16];
	uint32_t ipc;
	uint32_t ipsw;
};

struct panic_data
{
	uint8_t arch;
	uint8_t struct_version;
	uint8_t flags;
	uint8_t reserved;

	union
	{
		struct cortex_panic_data cm;
		struct nds32_n8_panic_data nds_n8;
	};

	uint32_t struct_size;
	uint32_t magic;
};

enum panic_arch
{
	PANIC_ARCH_CORTEX_M = 1, PANIC_ARCH_NDS32_N8 = 2,
};
# 97 "include/panic.h"
void panic_puts(const char *s);
# 107 "include/panic.h"
void panic_printf(const char *format, ...);

void panic_data_print(const struct panic_data *pdata);
# 128 "include/panic.h"
void panic_assert_fail(const char *msg, const char *func, const char *fname,
		       int linenum) __attribute__((noreturn));

void panic(const char *msg) __attribute__((noreturn));

void panic_reboot(void) __attribute__((noreturn));

void software_panic(uint32_t reason, uint32_t info) __attribute__((noreturn));

void panic_set_reason(uint32_t reason, uint32_t info, uint8_t exception);

void panic_get_reason(uint32_t *reason, uint32_t *info, uint8_t *exception);

void ignore_bus_fault(int ignored);

struct panic_data *panic_get_data(void);
# 186 "include/panic.h"
void chip_panic_data_backup(void);
# 14 "include/util.h" 2

# 1 "./builtin/assert.h" 1
# 29 "./builtin/assert.h"
extern void panic_assert_fail(const char *msg, const char *func,
			      const char *fname, int linenum) __attribute__((noreturn));
# 16 "include/util.h" 2
# 82 "include/util.h"
int atoi(const char *nptr);
int isdigit(int c);
int isspace(int c);
int isalpha(int c);
int isupper(int c);
int isprint(int c);
int memcmp(const void *s1, const void *s2, size_t len);
void *memcpy(void *dest, const void *src, size_t len);
__attribute__((externally_visible)) void *memset(void *dest,
								  int c,
								  size_t len);
void *memmove(void *dest, const void *src, size_t len);
void *memchr(const void *buffer, int c, size_t n);
int strcasecmp(const char *s1, const char *s2);
int strncasecmp(const char *s1, const char *s2, size_t size);
size_t strlen(const char *s);
size_t strnlen(const char *s, size_t maxlen);
char *strncpy(char *dest, const char *src, size_t n);
int strncmp(const char *s1, const char *s2, size_t n);

int strtoi(const char *nptr, char **endptr, int base);
uint64_t strtoul(const char *nptr, char **endptr, int base);

char *strzcpy(char *dest, const char *src, int len);
# 124 "include/util.h"
int parse_bool(const char *s, int *dest);

int tolower(int c);

int safe_memcmp(const void *s1, const void *s2, size_t len);

int uint64divmod(uint64_t *v, int by);
# 150 "include/util.h"
int get_next_bit(uint32_t *mask);

void reverse(void *dest, size_t len);
# 180 "include/util.h"
typedef uint8_t cond_t;

void cond_init(cond_t *c, int boolean);
static inline void cond_init_false(cond_t *c)
{
	cond_init(c, 0);
}
static inline void cond_init_true(cond_t *c)
{
	cond_init(c, 1);
}

void cond_set(cond_t *c, int boolean);
static inline void cond_set_false(cond_t *c)
{
	cond_set(c, 0);
}
static inline void cond_set_true(cond_t *c)
{
	cond_set(c, 1);
}

int cond_is(cond_t *c, int boolean);
static inline int cond_is_false(cond_t *c)
{
	return cond_is(c, 0);
}
static inline int cond_is_true(cond_t *c)
{
	return cond_is(c, 1);
}

int cond_went(cond_t *c, int boolean);
static inline int cond_went_false(cond_t *c)
{
	return cond_went(c, 0);
}
static inline int cond_went_true(cond_t *c)
{
	return cond_went(c, 1);
}

int parse_offset_size(int argc, char **argv, int shift, int *offset, int *size);
# 221 "include/util.h"
static inline uint64_t mula32(uint32_t a, uint32_t b, uint32_t c)
{
	uint64_t ret = a;

	ret *= b;
	ret += c;

	return ret;
}

static inline uint64_t mulaa32(uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
	uint64_t ret = a;

	ret *= b;
	ret += c;
	ret += d;

	return ret;
}
# 12 "chip/g/registers.h" 2
# 151 "chip/g/registers.h"
extern int __build_assertion_151[1
		- 2 * !(0x40610000 - 0x40600000 == 0x40620000 - 0x40610000)] __attribute__ ((unused));

static inline int x_uart_addr(int ch, int offset)
{
	return offset + 0x40600000 + (0x40610000 - 0x40600000) * ch;
}
# 239 "chip/g/registers.h"
static inline int x_timehs_addr(unsigned int module, unsigned int timer,
				int offset)
{
	return 0x40650000 + (0x40660000 - 0x40650000) * module + 0x0
			+ (0x20 - 0x0) * (timer - 1) + offset;
}
# 552 "chip/g/registers.h"
struct g_usb_desc
{
	uint32_t flags;
	void *addr;
};
# 9 "chip/g/usart.h" 2
# 1 "include/task.h" 1
# 12 "include/task.h"
# 1 "include/task_id.h" 1
# 11 "include/task_id.h"
# 1 "include/task_filter.h" 1
# 12 "include/task_id.h" 2
# 20 "include/task_id.h"
# 1 "board/cr50/ec.tasklist" 1
# 21 "include/task_id.h" 2
# 29 "include/task_id.h"
typedef uint8_t task_id_t;
# 39 "include/task_id.h"
enum
{
	TASK_ID_IDLE,

	TASK_ID_HOOKS, TASK_ID_TPM, TASK_ID_EC_COMMAND, TASK_ID_CONSOLE,

	TASK_ID_COUNT,

	TASK_ID_INVALID = 0xff,
};
# 13 "include/task.h" 2
# 61 "include/task.h"
void interrupt_disable(void);

void interrupt_enable(void);

int in_interrupt_context(void);

uint32_t get_int_mask(void);

void set_int_mask(uint32_t val);
# 100 "include/task.h"
uint32_t task_set_event(task_id_t tskid, uint32_t event, int wait);

static inline void task_wake(task_id_t tskid)
{
	task_set_event(tskid, (1 << 29), 0);
}

task_id_t task_get_current(void);

uint32_t *task_get_event_bitmap(task_id_t tskid);
# 135 "include/task.h"
uint32_t task_wait_event(int timeout_us);
# 153 "include/task.h"
uint32_t task_wait_event_mask(uint32_t event_mask, int timeout_us);

void task_print_list(void);

const char *task_get_name(task_id_t tskid);
# 174 "include/task.h"
void task_start_irq_handler(void *excep_return);
void task_end_irq_handler(void *excep_return);
# 190 "include/task.h"
void task_resched_if_needed(void *excep_return);

void task_pre_init(void);

int task_start(void);

int task_start_called(void);
# 220 "include/task.h"
void task_enable_all_tasks(void);

void task_enable_irq(int irq);

void task_disable_irq(int irq);

void task_trigger_irq(int irq);
# 246 "include/task.h"
void task_clear_pending_irq(int irq);

struct mutex
{
	uint32_t lock;
	uint32_t waiters;
};
# 261 "include/task.h"
void mutex_lock(struct mutex *mtx);

void mutex_unlock(struct mutex *mtx);

struct irq_priority
{
	uint8_t irq;
	uint8_t priority;
};

# 1 "core/cortex-m/irq_handler.h" 1
# 279 "include/task.h" 2
# 10 "chip/g/usart.h" 2

struct usart_config
{
	int uart;

	struct producer const producer;
	struct consumer const consumer;
};

extern struct consumer_ops const uart_consumer_ops;
extern struct producer_ops const uart_producer_ops;
# 69 "chip/g/usart.h"
void send_data_to_usb(struct usart_config const *config);

void get_data_from_usb(struct usart_config const *config);

extern struct usart_config const ec_uart;
# 16 "chip/g/usart.c" 2
# 1 "chip/g/usb-stream.h" 1
# 10 "chip/g/usb-stream.h"
# 1 "include/compile_time_macros.h" 1
# 11 "chip/g/usb-stream.h" 2

# 1 "include/hooks.h" 1
# 13 "include/hooks.h"
enum hook_priority
{

	HOOK_PRIO_FIRST = 1, HOOK_PRIO_DEFAULT = 5000, HOOK_PRIO_LAST = 9999,

	HOOK_PRIO_INIT_DMA = HOOK_PRIO_FIRST + 1,

	HOOK_PRIO_INIT_LPC = HOOK_PRIO_FIRST + 1,

	HOOK_PRIO_INIT_I2C = HOOK_PRIO_FIRST + 2,

	HOOK_PRIO_INIT_CHIPSET = HOOK_PRIO_FIRST + 3,

	HOOK_PRIO_INIT_LID = HOOK_PRIO_FIRST + 4,

	HOOK_PRIO_INIT_POWER_BUTTON = HOOK_PRIO_FIRST + 5,

	HOOK_PRIO_INIT_SWITCH = HOOK_PRIO_FIRST + 6,

	HOOK_PRIO_INIT_FAN = HOOK_PRIO_FIRST + 7,

	HOOK_PRIO_INIT_PWM = HOOK_PRIO_FIRST + 8,

	HOOK_PRIO_INIT_SPI = HOOK_PRIO_FIRST + 9,

	HOOK_PRIO_INIT_EXTPOWER = HOOK_PRIO_FIRST + 10,

	HOOK_PRIO_INIT_VBOOT_HASH = HOOK_PRIO_FIRST + 11,

	HOOK_PRIO_CHARGE_MANAGER_INIT = HOOK_PRIO_FIRST + 12,

	HOOK_PRIO_TEMP_SENSOR = 6000,

	HOOK_PRIO_TEMP_SENSOR_DONE = HOOK_PRIO_TEMP_SENSOR + 1,
};

enum hook_type
{

	HOOK_INIT = 0,
# 72 "include/hooks.h"
	HOOK_PRE_FREQ_CHANGE, HOOK_FREQ_CHANGE,
# 82 "include/hooks.h"
	HOOK_SYSJUMP,

	HOOK_CHIPSET_PRE_INIT,

	HOOK_CHIPSET_STARTUP,

	HOOK_CHIPSET_RESUME,

	HOOK_CHIPSET_SUSPEND,

	HOOK_CHIPSET_SHUTDOWN,

	HOOK_CHIPSET_RESET,

	HOOK_AC_CHANGE,

	HOOK_LID_CHANGE,

	HOOK_TABLET_MODE_CHANGE,

	HOOK_BASE_ATTACHED_CHANGE,

	HOOK_POWER_BUTTON_CHANGE,

	HOOK_BATTERY_SOC_CHANGE,

	HOOK_CCD_CHANGE,
# 196 "include/hooks.h"
	HOOK_TICK,

	HOOK_SECOND,

	HOOK_USB_PD_DISCONNECT,
};

struct hook_data
{

	void (*routine)(void);

	int priority;
};
# 232 "include/hooks.h"
void hook_notify(enum hook_type type);

struct deferred_data
{

	void (*routine)(void);
};
# 253 "include/hooks.h"
int hook_call_deferred(const struct deferred_data *data, int us);
# 13 "chip/g/usb-stream.h" 2

# 1 "include/usb_descriptor.h" 1
# 31 "include/usb_descriptor.h"
struct usb_device_descriptor
{
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint16_t bcdUSB;
	uint8_t bDeviceClass;
	uint8_t bDeviceSubClass;
	uint8_t bDeviceProtocol;
	uint8_t bMaxPacketSize0;
	uint16_t idVendor;
	uint16_t idProduct;
	uint16_t bcdDevice;
	uint8_t iManufacturer;
	uint8_t iProduct;
	uint8_t iSerialNumber;
	uint8_t bNumConfigurations;
}__attribute__((packed));

struct bos_context
{
	void *descp;
	int size;
};

struct usb_bos_hdr_descriptor
{
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint16_t wTotalLength;
	uint8_t bNumDeviceCaps;
}__attribute__((packed));

struct usb_contid_caps_descriptor
{
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bDevCapabilityType;
	uint8_t bReserved;
	uint8_t ContainerID[16];
}__attribute__((packed));
# 90 "include/usb_descriptor.h"
struct usb_platform_descriptor
{
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bDevCapabilityType;
	uint8_t bReserved;
	uint8_t PlatformCapUUID[16];
	uint16_t bcdVersion;
	uint8_t bVendorCode;
	uint8_t iLandingPage;
}__attribute__((packed));
# 108 "include/usb_descriptor.h"
struct usb_qualifier_descriptor
{
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint16_t bcdUSB;
	uint8_t bDeviceClass;
	uint8_t bDeviceSubClass;
	uint8_t bDeviceProtocol;
	uint8_t bMaxPacketSize0;
	uint8_t bNumConfigurations;
	uint8_t bReserved;
}__attribute__((packed));

struct usb_config_descriptor
{
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint16_t wTotalLength;
	uint8_t bNumInterfaces;
	uint8_t bConfigurationValue;
	uint8_t iConfiguration;
	uint8_t bmAttributes;
	uint8_t bMaxPower;
}__attribute__((packed));

struct usb_string_descriptor
{
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint16_t wData[1];
}__attribute__((packed));

struct usb_interface_descriptor
{
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bInterfaceNumber;
	uint8_t bAlternateSetting;
	uint8_t bNumEndpoints;
	uint8_t bInterfaceClass;
	uint8_t bInterfaceSubClass;
	uint8_t bInterfaceProtocol;
	uint8_t iInterface;
}__attribute__((packed));

struct usb_endpoint_descriptor
{
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bEndpointAddress;
	uint8_t bmAttributes;
	uint16_t wMaxPacketSize;
	uint8_t bInterval;
}__attribute__((packed));
# 281 "include/usb_descriptor.h"
struct usb_setup_packet
{
	uint8_t bmRequestType;
	uint8_t bRequest;
	uint16_t wValue;
	uint16_t wIndex;
	uint16_t wLength;
};
# 311 "include/usb_descriptor.h"
struct usb_string_desc
{
	uint8_t _len;
	uint8_t _type;
	wchar_t _data[28];
};

extern struct usb_string_desc *usb_serialno_desc;
# 337 "include/usb_descriptor.h"
extern const uint8_t __usb_desc[];
extern const uint8_t __usb_desc_end[];

extern const void * const usb_strings[];
extern const uint8_t usb_string_desc[];

extern const void * const usb_fw_version;
extern const struct bos_context bos_ctx;
extern const void *webusb_url;
# 17 "chip/g/usb-stream.h" 2
# 1 "chip/g/usb_hw.h" 1
# 24 "chip/g/usb_hw.h"
extern void (*usb_ep_tx[])(void);
extern void (*usb_ep_rx[])(void);
extern void (*usb_ep_reset[])(void);
struct usb_setup_packet;

extern int (*usb_iface_request[])(struct usb_setup_packet *req);
# 48 "chip/g/usb_hw.h"
int load_in_fifo(const void *source, uint32_t len);

int accept_out_fifo(uint32_t len);
# 18 "chip/g/usb-stream.h" 2

struct usb_stream_config
{

	int endpoint;

	int *is_reset;

	const struct deferred_data *deferred_tx;
	const struct deferred_data *deferred_rx;

	int tx_size;
	int rx_size;

	uint8_t *tx_ram;
	uint8_t *rx_ram;

	struct consumer consumer;
	struct producer producer;

	struct g_usb_desc *out_desc;
	struct g_usb_desc *in_desc;
};

extern struct consumer_ops const usb_stream_consumer_ops;
extern struct producer_ops const usb_stream_producer_ops;
# 211 "chip/g/usb-stream.h"
int rx_stream_handler(struct usb_stream_config const *config);
int tx_stream_handler(struct usb_stream_config const *config);

void usb_stream_tx(struct usb_stream_config const *config);
void usb_stream_rx(struct usb_stream_config const *config);
void usb_stream_reset(struct usb_stream_config const *config);
# 17 "chip/g/usart.c" 2
# 42 "chip/g/usart.c"
struct usb_stream_config const ap_usb;
struct usart_config const ap_uart;
# 67 "chip/g/usart.c"
static struct queue const ap_uart_output =
		((struct queue ) {
						.state =
								&((struct queue_state ) { } ),
						.policy =
								&((struct queue_policy_direct const ) {
												.policy =
														{
																.add =
																		queue_add_direct,
																.remove =
																		queue_remove_direct, },
												.producer =
														&ap_uart.producer,
												.consumer =
														&ap_usb.consumer, } ).policy,
						.buffer_units = 512,
						.unit_bytes = sizeof(uint8_t),
						.buffer =
								(uint8_t *) &((uint8_t[512] ) { } ), } );

static struct queue const ap_usb_to_uart =
		((struct queue ) {
						.state =
								&((struct queue_state ) { } ),
						.policy =
								&((struct queue_policy_direct const ) {
												.policy =
														{
																.add =
																		queue_add_direct,
																.remove =
																		queue_remove_direct, },
												.producer =
														&ap_usb.producer,
												.consumer =
														&ap_uart.consumer, } ).policy,
						.buffer_units = 64,
						.unit_bytes = sizeof(uint8_t),
						.buffer =
								(uint8_t *) &((uint8_t[64] ) { } ), } );

struct usart_config const ap_uart = { .uart = 1, .consumer = { .queue =
		&ap_usb_to_uart, .ops = &uart_consumer_ops, }, .producer = {
		.queue = &ap_uart_output, .ops = &uart_producer_ops, }, }

;

static struct g_usb_desc ap_usb_out_desc_;
static struct g_usb_desc ap_usb_in_desc_;
static uint8_t ap_usb_buf_rx_[64];
static uint8_t ap_usb_buf_tx_[64];
static int ap_usb_is_reset_;
static void ap_usb_deferred_tx_(void);
const struct deferred_data __attribute__((used)) __attribute__((externally_visible)) ap_usb_deferred_tx__data __attribute__((section(".rodata.deferred")))
		= { ap_usb_deferred_tx_ };
static void ap_usb_deferred_rx_(void);
const struct deferred_data __attribute__((used)) __attribute__((externally_visible)) ap_usb_deferred_rx__data __attribute__((section(".rodata.deferred")))
		= { ap_usb_deferred_rx_ };
struct usb_stream_config const ap_usb = { .endpoint = 2, .is_reset =
		&ap_usb_is_reset_, .in_desc = &ap_usb_in_desc_, .out_desc =
		&ap_usb_out_desc_, .deferred_tx = &ap_usb_deferred_tx__data,
		.deferred_rx = &ap_usb_deferred_rx__data, .tx_size = 64,
		.rx_size = 64, .tx_ram = ap_usb_buf_tx_, .rx_ram =
				ap_usb_buf_rx_, .consumer =
				{ .queue = &ap_uart_output, .ops =
						&usb_stream_consumer_ops, },
		.producer = { .queue = &ap_usb_to_uart, .ops =
				&usb_stream_producer_ops, }, };
const struct usb_interface_descriptor usb_desc_iface1_0iface __attribute__((used)) __attribute__((externally_visible)) __attribute__((section(".rodata.usb_desc_" "iface1_0iface")))
		= { .bLength = 9, .bDescriptorType = 0x04,
				.bInterfaceNumber = 1, .bAlternateSetting = 0,
				.bNumEndpoints = 2, .bInterfaceClass = 0xff,
				.bInterfaceSubClass = 0x50,
				.bInterfaceProtocol = 0x01, .iInterface =
						USB_STR_AP_NAME, };
const struct usb_endpoint_descriptor usb_desc_iface1_2ep0 __attribute__((used)) __attribute__((externally_visible)) __attribute__((section(".rodata.usb_desc_" "iface1_2ep0")))
		= { .bLength = 7, .bDescriptorType = 0x05, .bEndpointAddress =
				0x80 | 2, .bmAttributes = 0x02,
				.wMaxPacketSize = 64, .bInterval = 10, };
const struct usb_endpoint_descriptor usb_desc_iface1_2ep1 __attribute__((used)) __attribute__((externally_visible)) __attribute__((section(".rodata.usb_desc_" "iface1_2ep1")))
		= { .bLength = 7, .bDescriptorType = 0x05,
				.bEndpointAddress = 2, .bmAttributes = 0x02,
				.wMaxPacketSize = 64, .bInterval = 0, };
static void ap_usb_deferred_tx_(void)
{
	tx_stream_handler(&ap_usb);
}
static void ap_usb_deferred_rx_(void)
{
	rx_stream_handler(&ap_usb);
}
static void ap_usb_ep_tx(void)
{
	usb_stream_tx(&ap_usb);
}
static void ap_usb_ep_rx(void)
{
	usb_stream_rx(&ap_usb);
}
static void ap_usb_ep_reset(void)
{
	usb_stream_reset(&ap_usb);
}
void ep_2_tx(void) __attribute__ ((alias("ap_usb_ep_tx")));
void ep_2_rx(void) __attribute__ ((alias("ap_usb_ep_rx")));
void ep_2_rst(void) __attribute__ ((alias("ap_usb_ep_reset")));
# 108 "chip/g/usart.c"
struct usb_stream_config const ec_usb;
struct usart_config const ec_uart;

static struct queue const ec_uart_to_usb = ((struct queue ) {
	.state = &((struct queue_state ) { } ),
	.policy = &((struct queue_policy_direct const ) {
		.policy = {
			.add = queue_add_direct,
			.remove = queue_remove_direct, },
			.producer = &ec_uart.producer,
			.consumer = &ec_usb.consumer,
		} ).policy,
	.buffer_units = 512,
	.unit_bytes = sizeof(uint8_t),
	.buffer = (uint8_t *) &((uint8_t[512] ) { } ), } );

static struct queue const ec_usb_to_uart = ((struct queue ) {
	.state = &((struct queue_state ) { } ),
	.policy = &((struct queue_policy_direct const ) {
		.policy = {
			.add = queue_add_direct,
			.remove = queue_remove_direct, },
		.producer = &ec_usb.producer,
		.consumer = &ec_uart.consumer, } ).policy,
		.buffer_units = 64,
		.unit_bytes = sizeof(uint8_t),
		.buffer = (uint8_t *) &((uint8_t[64] ) { } ), } );

struct usart_config const ec_uart = {
	.uart = 2,
	.consumer = {
		.queue = &ec_usb_to_uart,
		.ops = &uart_consumer_ops, },
	.producer = {
		.queue = &ec_uart_to_usb,
		.ops = &uart_producer_ops, },
} ;

static struct g_usb_desc ec_usb_out_desc_;
static struct g_usb_desc ec_usb_in_desc_;
static uint8_t ec_usb_buf_rx_[64];
static uint8_t ec_usb_buf_tx_[64];
static int ec_usb_is_reset_;
static void ec_usb_deferred_tx_(void);
const struct deferred_data __attribute__((used)) __attribute__((externally_visible)) ec_usb_deferred_tx__data __attribute__((section(".rodata.deferred")))
		= { ec_usb_deferred_tx_ };
static void ec_usb_deferred_rx_(void);
const struct deferred_data __attribute__((used)) __attribute__((externally_visible)) ec_usb_deferred_rx__data __attribute__((section(".rodata.deferred")))
		= { ec_usb_deferred_rx_ };
struct usb_stream_config const ec_usb = {
		.endpoint = 3,
		.is_reset =
		&ec_usb_is_reset_,
		.in_desc = &ec_usb_in_desc_,
		.out_desc =
		&ec_usb_out_desc_,
		.deferred_tx = &ec_usb_deferred_tx__data,
		.deferred_rx = &ec_usb_deferred_rx__data,
		.tx_size = 64,
		.rx_size = 64,
		.tx_ram = ec_usb_buf_tx_,
		.rx_ram = ec_usb_buf_rx_,
		.consumer = {
			.queue = &ec_uart_to_usb,
			.ops = &usb_stream_consumer_ops, },
		.producer = {
			.queue = &ec_usb_to_uart,
			.ops = &usb_stream_producer_ops, }, };
const struct usb_interface_descriptor usb_desc_iface2_0iface __attribute__((used)) __attribute__((externally_visible)) __attribute__((section(".rodata.usb_desc_" "iface2_0iface")))
		= { .bLength = 9, .bDescriptorType = 0x04,
				.bInterfaceNumber = 2, .bAlternateSetting = 0,
				.bNumEndpoints = 2, .bInterfaceClass = 0xff,
				.bInterfaceSubClass = 0x50,
				.bInterfaceProtocol = 0x01, .iInterface =
						USB_STR_EC_NAME, };
const struct usb_endpoint_descriptor usb_desc_iface2_2ep0 __attribute__((used)) __attribute__((externally_visible)) __attribute__((section(".rodata.usb_desc_" "iface2_2ep0")))
		= { .bLength = 7, .bDescriptorType = 0x05, .bEndpointAddress =
				0x80 | 3, .bmAttributes = 0x02,
				.wMaxPacketSize = 64, .bInterval = 10, };
const struct usb_endpoint_descriptor usb_desc_iface2_2ep1 __attribute__((used)) __attribute__((externally_visible)) __attribute__((section(".rodata.usb_desc_" "iface2_2ep1")))
		= { .bLength = 7, .bDescriptorType = 0x05,
				.bEndpointAddress = 3, .bmAttributes = 0x02,
				.wMaxPacketSize = 64, .bInterval = 0, };
static void ec_usb_deferred_tx_(void)
{
	tx_stream_handler(&ec_usb);
}
static void ec_usb_deferred_rx_(void)
{
	rx_stream_handler(&ec_usb);
}
static void ec_usb_ep_tx(void)
{
	usb_stream_tx(&ec_usb);
}
static void ec_usb_ep_rx(void)
{
	usb_stream_rx(&ec_usb);
}
static void ec_usb_ep_reset(void)
{
	usb_stream_reset(&ec_usb);
}
void ep_3_tx(void) __attribute__ ((alias("ec_usb_ep_tx")));
void ep_3_rx(void) __attribute__ ((alias("ec_usb_ep_rx")));
void ep_3_rst(void) __attribute__ ((alias("ec_usb_ep_reset")));
# 132 "chip/g/usart.c"
void get_data_from_usb(struct usart_config const *config)
{
	struct queue const *uart_out = config->consumer.queue;
	int c;

	while (queue_count(uart_out)
			&& ( {      size_t result; if (1 == 1) result = queue_remove_unit(uart_out, &c); else result = queue_remove_units(uart_out, &c, 1); result;}))
		uartn_write_char(config->uart, c);

	if (!queue_count(uart_out))
		uartn_tx_stop(config->uart);
}

void send_data_to_usb(struct usart_config const *config)
{

	uint8_t buffer[50];
	uint32_t i;
	uint32_t room;
	struct queue const *uart_in = config->producer.queue;
	int uart = config->uart;

	i = 0;
	room = ( {
		__typeof__(sizeof(buffer)) temp_a = (sizeof(buffer));
		__typeof__(queue_space(uart_in)) temp_b = (queue_space(uart_in));
		((temp_a) < (temp_b) ? (temp_a) : (temp_b));
	});

	while ((i < room) && uartn_rx_available(uart))
		buffer[i++] = uartn_read_char(uart);

	if (i) ( {
		size_t result;
		if (i == 1)
			result = queue_add_unit(uart_in, buffer);
		else
			result = queue_add_units(uart_in, buffer, i);
		result;
	});
}

static void uart_read(struct producer const *producer, size_t count)
{
}

static void uart_written(struct consumer const *consumer, size_t count)
{
	struct usart_config const *config =
			((struct usart_config *) (((uint8_t *) consumer)
					- __builtin_offsetof(
							struct usart_config,
							consumer)));

	if (uart_bitbang_is_enabled()
			&& (config->uart == bitbang_config.uart)) {
		uart_bitbang_drain_tx_queue(consumer->queue);
		return;
	}

	if (uartn_tx_ready(config->uart), queue_count(consumer->queue))
		uartn_tx_start(config->uart);
}

struct producer_ops const uart_producer_ops = { .read = uart_read, };

struct consumer_ops const uart_consumer_ops = { .written = uart_written, };

void ap_uart_rx_int_(void);
void ap_uart_tx_int_(void);
void irq_181_handler(void) __attribute__((naked));
typedef struct
{
	int dummy[181 >= (218 - 15) ? -1 : 1];
} irq_num_check_181;
void __attribute__((used)) __attribute__((externally_visible)) ap_uart_rx_int_(
		void);
void irq_181_handler(void)
{
	asm volatile("mov r0, lr\n" "push {r0, lr}\n" "bl task_start_irq_handler\n" "bl ""ap_uart_rx_int_""\n" "pop {r0, lr}\n" "b task_resched_if_needed\n" );
}
const struct irq_priority __attribute__((used)) __attribute__((externally_visible)) prio_181 __attribute__((section(".rodata.irqprio")))
		= { 181, 1 };
void irq_184_handler(void) __attribute__((naked));
typedef struct
{
	int dummy[184 >= (218 - 15) ? -1 : 1];
} irq_num_check_184;
void __attribute__((used)) __attribute__((externally_visible)) ap_uart_tx_int_(
		void);
void irq_184_handler(void)
{
	asm volatile("mov r0, lr\n" "push {r0, lr}\n" "bl task_start_irq_handler\n" "bl ""ap_uart_tx_int_""\n" "pop {r0, lr}\n" "b task_resched_if_needed\n" );
}
const struct irq_priority __attribute__((used)) __attribute__((externally_visible)) prio_184 __attribute__((section(".rodata.irqprio")))
		= { 184, 1 };
void ap_uart_tx_int_(void)
{
	(*((volatile uint32_t *) (x_uart_addr(ap_uart.uart, 0x20)))) = 0x1;
	get_data_from_usb(&ap_uart);
}
void ap_uart_rx_int_(void)
{
	(*((volatile uint32_t *) (x_uart_addr(ap_uart.uart, 0x20)))) = 0x2;
	send_data_to_usb(&ap_uart);
}
# 213 "chip/g/usart.c"
void ec_uart_rx_int_(void);
void ec_uart_tx_int_(void);
void irq_188_handler(void) __attribute__((naked));
typedef struct
{
	int dummy[188 >= (218 - 15) ? -1 : 1];
} irq_num_check_188;
void __attribute__((used)) __attribute__((externally_visible)) ec_uart_rx_int_(
		void);
void irq_188_handler(void)
{
	asm volatile("mov r0, lr\n" "push {r0, lr}\n" "bl task_start_irq_handler\n" "bl ""ec_uart_rx_int_""\n" "pop {r0, lr}\n" "b task_resched_if_needed\n" );
}
const struct irq_priority __attribute__((used)) __attribute__((externally_visible)) prio_188 __attribute__((section(".rodata.irqprio")))
		= { 188, 1 };
void irq_191_handler(void) __attribute__((naked));
typedef struct
{
	int dummy[191 >= (218 - 15) ? -1 : 1];
} irq_num_check_191;
void __attribute__((used)) __attribute__((externally_visible)) ec_uart_tx_int_(
		void);
void irq_191_handler(void)
{
	asm volatile("mov r0, lr\n" "push {r0, lr}\n" "bl task_start_irq_handler\n"
			"bl "
			"ec_uart_tx_int_"
			"\n"
			"pop {r0, lr}\n" "b task_resched_if_needed\n" );
}
const struct irq_priority __attribute__((used)) __attribute__((externally_visible)) prio_191 __attribute__((section(".rodata.irqprio")))
		= { 191, 1 };
void ec_uart_tx_int_(void)
{
	(*((volatile uint32_t *) (x_uart_addr(ec_uart.uart, 0x20)))) = 0x1;
	get_data_from_usb(&ec_uart);
}
void ec_uart_rx_int_(void)
{
	(*((volatile uint32_t *) (x_uart_addr(ec_uart.uart, 0x20)))) = 0x2;
	send_data_to_usb(&ec_uart);
}
