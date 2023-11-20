/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * PD Controller subsystem
 */

#define DT_DRV_COMPAT named_usbc_port

#include "charge_manager.h"
#include "charge_state.h"

#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/smf.h>

#include <drivers/pdc.h>
#include <usbc/utils.h>

LOG_MODULE_DECLARE(pdc, LOG_LEVEL_INF);

/**
 * @brief maximum number of times to try and send a command
 */
#define RETRY_MAX 10

/**
 * @brief maximum number of counts to wait the subsystem to respond to an API
 * call
 */
#define BLOCK_COUNTER_MAX 10

/**
 * @brief Time delay before running the state machine loop
 */
#define LOOP_DELAY_MS 150

/**
 * @brief maximum number of PDOs
 */
#define PDO_NUM 7

/**
 * @brief PDC driver commands
 */
enum pdc_cmd_t {
	/** CMD_PDC_NONE */
	CMD_PDC_NONE,
	/** CMD_PDC_RESET */
	CMD_PDC_RESET,
	/** CMD_PDC_SET_POWER_LEVEL */
	CMD_PDC_SET_POWER_LEVEL,
	/** CMD_PDC_SET_CCOM */
	CMD_PDC_SET_CCOM,
	/** CMD_PDC_GET_PDOS */
	CMD_PDC_GET_PDOS,
	/** CMD_PDC_GET_RDO */
	CMD_PDC_GET_RDO,
	/** CMD_PDC_SET_RDO */
	CMD_PDC_SET_RDO,
	/** CMD_PDC_GET_VBUS_VOLTAGE */
	CMD_PDC_GET_VBUS_VOLTAGE,
	/** CMD_PDC_SET_SINK_PATH */
	CMD_PDC_SET_SINK_PATH,
	/** CMD_PDC_READ_POWER_LEVEL */
	CMD_PDC_READ_POWER_LEVEL,
	/** CMD_PDC_GET_INFO */
	CMD_PDC_GET_INFO,
	/** CMD_PDC_GET_CONNECTOR_CAPABILITY */
	CMD_PDC_GET_CONNECTOR_CAPABILITY,
	/** CMD_PDC_SET_UOR */
	CMD_PDC_SET_UOR,
	/** CMD_PDC_SET_PDR */
	CMD_PDC_SET_PDR,
	/** CMD_PDC_GET_CONNECTOR_STATUS */
	CMD_PDC_GET_CONNECTOR_STATUS,

	/** CMD_PDC_COUNT */
	CMD_PDC_COUNT
};

/**
 * @brief Send Local States
 */
enum send_cmd_state_t {
	/** SEND_CMD_START_ENTRY */
	SEND_CMD_START_ENTRY,
	/** SEND_CMD_START_RUN */
	SEND_CMD_START_RUN,
	/** SEND_CMD_WAIT_ENTRY */
	SEND_CMD_WAIT_ENTRY,
	/** SEND_CMD_WAIT_RUN */
	SEND_CMD_WAIT_RUN,
	/** SEND_CMD_WAIT_EXIT */
	SEND_CMD_WAIT_EXIT,
};

/**
 * @ Command type
 */
struct cmd_t {
	/** Command to send */
	enum pdc_cmd_t cmd;
	/** True if command is pending */
	bool pending;
	/** True if command failed to send */
	bool error;
};

/**
 * @ Send command type
 */
struct send_cmd_t {
	/** Send command local state */
	enum send_cmd_state_t local_state;
	/** Retry counter used in local start state */
	uint8_t start_retry_counter;
	/* Retry counter used in local wait state */
	uint8_t wait_retry_counter;
	/* Command sent from public API */
	struct cmd_t public;
	/* Command sent from internal API */
	struct cmd_t intern;
};

/**
 * @brief SNK Attached Local States
 */
enum snk_attached_local_state_t {
	/** SNK_ATTACHED_GET_CONNECTOR_CAPABILITY */
	SNK_ATTACHED_GET_CONNECTOR_CAPABILITY,
	/** SNK_ATTACHED_GET_CONNECTOR_CAPABILITY_WAIT */
	SNK_ATTACHED_GET_CONNECTOR_CAPABILITY_WAIT,
	/** SNK_ATTACHED_READ_POWER_LEVEL */
	SNK_ATTACHED_READ_POWER_LEVEL,
	/** SNK_ATTACHED_READ_POWER_LEVEL_WAIT */
	SNK_ATTACHED_READ_POWER_LEVEL_WAIT,
	/** SNK_ATTACHED_GET_PDOS */
	SNK_ATTACHED_GET_PDOS,
	/** SNK_ATTACHED_GET_PDOS_WAIT */
	SNK_ATTACHED_GET_PDOS_WAIT,
	/** SNK_ATTACHED_GET_RDO */
	SNK_ATTACHED_GET_RDO,
	/** SNK_ATTACHED_GET_RDO_WAIT */
	SNK_ATTACHED_GET_RDO_WAIT,
	/** SNK_ATTACHED_SET_SINK_PATH_ON */
	SNK_ATTACHED_SET_SINK_PATH_ON,
	/** SNK_ATTACHED_SET_SINK_PATH_ON_WAIT */
	SNK_ATTACHED_SET_SINK_PATH_ON_WAIT,
	/** SNK_ATTACHED_START_CHARGING */
	SNK_ATTACHED_START_CHARGING,
	/** SNK_ATTACHED_RUN */
	SNK_ATTACHED_RUN,
};

/**
 * @brief SRC Attached Local States
 */
enum src_attached_local_state_t {
	/** SRC_ATTACHED_SET_SINK_PATH_OFF */
	SRC_ATTACHED_SET_SINK_PATH_OFF,
	/** SRC_ATTACHED_SET_SINK_PATH_OFF_WAIT */
	SRC_ATTACHED_SET_SINK_PATH_OFF_WAIT,
	/** SRC_ATTACHED_RUN */
	SRC_ATTACHED_RUN,
};

/**
 * @brief Unattached Local States
 */
enum unattached_local_state_t {
	/** UNATTACHED_SET_SINK_PATH_OFF */
	UNATTACHED_SET_SINK_PATH_OFF,
	/** UNATTACHED_SET_SINK_PATH_OFF_WAIT */
	UNATTACHED_SET_SINK_PATH_OFF_WAIT,
	/** UNATTACHED_RUN */
	UNATTACHED_RUN,
};

/**
 * @brief Connector Local States
 */
enum connector_local_state_t {
	/** CONNECTOR_WAIT_CHANGE */
	CONNECTOR_WAIT_CHANGE,
	/** CONNECTOR_GET_STATUS */
	CONNECTOR_GET_STATUS,
	/** CONNECTOR_GET_STATUS_WAIT */
	CONNECTOR_GET_STATUS_WAIT,
	/** CONNECTOR_EVAL_STATUS */
	CONNECTOR_EVAL_STATUS,
	/** INIT_DR_SWAP_POLICY */
	CONNECTOR_SET_DR_SWAP_POLICY,
	/** INIT_DR_SWAP_POLICY */
	CONNECTOR_SET_DR_SWAP_POLICY_WAIT,
	/** INIT_PR_SWAP_POLICY */
	CONNECTOR_SET_PR_SWAP_POLICY,
	/** INIT_PR_SWAP_POLICY */
	CONNECTOR_SET_PR_SWAP_POLICY_WAIT,

};

/**
 * @brief CCI Event Flags
 */
enum cci_flag_t {
	/** CCI_RESET_COMPLETED */
	CCI_RESET_COMPLETED,
	/** CCI_BUSY */
	CCI_BUSY,
	/** CCI_ERROR */
	CCI_ERROR,
	/** CCI_CMD_COMPLETED */
	CCI_CMD_COMPLETED,
	/** CCI_EVENT */
	CCI_EVENT,

	/** CCI_FLAGS_COUNT */
	CCI_FLAGS_COUNT
};

/**
 * @brief State Machine States
 */
enum pdc_state_t {
	/** PDC_CONNECTOR_CHANGE_EVENT */
	PDC_CONNECTOR_CHANGE_EVENT,
	/** PDC_UNATTACHED */
	PDC_UNATTACHED,
	/** PDC_SNK_ATTACHED */
	PDC_SNK_ATTACHED,
	/** PDC_SRC_ATTACHED */
	PDC_SRC_ATTACHED,
};

/**
 * @brief State Machine State Names
 */
static const char *const pdc_state_names[] = {
	[PDC_CONNECTOR_CHANGE_EVENT] = "PortChange.EVENT",
	[PDC_UNATTACHED] = "Unattached",
	[PDC_SNK_ATTACHED] = "Attached.SNK",
	[PDC_SRC_ATTACHED] = "Attached.SRC",
};

/**
 * @brief Unattached policy flags
 */
enum policy_unattached_t {
	/** UNA_POLICY_TCC */
	UNA_POLICY_TCC,
	/** UNA_POLICY_CC_MODE */
	UNA_POLICY_CC_MODE,
	/** UNA_POLICY_COUNT */
	UNA_POLICY_COUNT,
};

/**
 * @brief Unattached policy object
 */
struct pdc_unattached_policy_t {
	/** Unattached policy flags */
	ATOMIC_DEFINE(flags, UNA_POLICY_COUNT);
	/** Type-C current */
	enum usb_typec_current_t tcc;
	/** CC Operation Mode */
	enum ccom_t cc_mode;
	/** DRP Operation Mode */
	enum drp_mode_t drp_mode;
};

/**
 * @brief Sink policy flags
 */
enum policy_snk_attached_t {
	/** Request a new power level */
	SNK_POLICY_NEW_POWER_REQUEST,
	/** Enables swap to Source */
	SNK_POLICY_SWAP_TO_SRC,
	/** Selects the low power PDO on connect */
	SNK_POLICY_REQUEST_LOW_POWER_PDO,
	/** Selects the highest powered PDO on connect */
	SNK_POLICY_REQUEST_HIGH_POWER_PDO,

	/** SNK_POLICY_COUNT */
	SNK_POLICY_COUNT,
};

/**
 * @brief Sink attached policy object
 */
struct pdc_snk_attached_policy_t {
	/** SNK Attached policy flags */
	ATOMIC_DEFINE(flags, SNK_POLICY_COUNT);
	/** Currently active PDO */
	uint32_t pdo;
	/** PDOs supported by the Source */
	uint32_t pdos[PDO_NUM];
	/** Sent RDO */
	uint32_t rdo;
	/** New RDO to send */
	uint32_t rdo_to_send;
};

/**
 * @brief Source attached policy flags
 */
enum policy_src_attached_t {
	/** Enables swap to Sink */
	SRC_POLICY_SWAP_TO_SNK,

	/** SRC_POLICY_COUNT */
	SRC_POLICY_COUNT
};

/**
 * @brief Source attached policy object
 */
struct pdc_src_attached_policy_t {
	/** SRC Attached policy flags */
	ATOMIC_DEFINE(flags, SRC_POLICY_COUNT);
};

/**
 * @brief PDC Port object
 */
struct pdc_port_t {
	/** State machine context */
	struct smf_ctx ctx;
	/** Subsystem device */
	const struct device *dev;
	/** PDC device */
	const struct device *pdc;

	/** CCI flags */
	ATOMIC_DEFINE(cci_flags, CCI_FLAGS_COUNT);
	/** PDC Cmd flags */
	ATOMIC_DEFINE(pdc_cmd_flags, CMD_PDC_COUNT);

	/** Connector change local state variable */
	enum connector_local_state_t connector_local_state;
	/** Unattached local state variable */
	enum unattached_local_state_t unattached_local_state;
	/** Sink attached local state variable */
	enum snk_attached_local_state_t snk_attached_local_state;
	/* Source attached local state variable */
	enum src_attached_local_state_t src_attached_local_state;

	/** Transitioning from last_state */
	enum pdc_state_t last_state;

	/** PDC Unattached policy */
	struct pdc_unattached_policy_t una_policy;
	/** PDC Sink Attached policy */
	struct pdc_snk_attached_policy_t snk_policy;
	/** PDC Source Attached policy */
	struct pdc_src_attached_policy_t src_policy;

	/** PDC version and other information */
	struct pdc_info_t info;
	/** Public API block counter */
	uint8_t block_counter;
	/** True when the CCAPS temp variable has valid data */
	bool ccaps_ready;
	/** PDC command to send */
	struct send_cmd_t send_cmd;
	/** Pointer to current pending command */
	struct cmd_t *cmd;
	/** CCAPS temp variable used with CMD_PDC_GET_CONNECTOR_CAPABILITY
	 * command */
	union connector_capability_t ccaps;
	/** CONNECTOR_STATUS temp variable used with CONNECTOR_GET_STATUS
	 * command */
	struct connector_status_t connector_status;
	/** SINK_PATH_EN temp variable used with CMD_PDC_SET_SINK_PATH command
	 */
	bool sink_path_en;
	/** VBUS temp variable used with CMD_PDC_GET_VBUS_VOLTAGE command */
	uint16_t vbus;
	/** UOR temp variable used with CMD_PDC_SET_UOR command */
	union uor_t uor;
	/** PDR temp variable used with CMD_PDC_SET_PDR command */
	union pdr_t pdr;
	/** True if battery can charge from this port */
	bool active_charge;
};

/**
 * @brief Subsystem PDC Data
 */
struct pdc_data_t {
	/** This port's thread */
	k_tid_t thread;
	/** This port thread's data */
	struct k_thread thread_data;
	/** Port data */
	struct pdc_port_t port;
};

/**
 * @brief Subsystem PDC Config
 */
struct pdc_config_t {
	/** Port number for the connector */
	uint8_t connector_num;
	/**
	 * The usbc stack initializes this pointer that creates the
	 * main thread for this port
	 */
	void (*create_thread)(const struct device *dev);
};

static void run_send_cmd(struct pdc_port_t *port);
static const struct smf_state pdc_states[];
static int pdc_subsys_init(const struct device *dev);
static bool pdc_is_in_run_state(struct pdc_port_t *port);
static void send_cmd_init(struct pdc_port_t *port);
static void queue_internal_cmd(struct pdc_port_t *port, enum pdc_cmd_t pdc_cmd);
static void queue_public_cmd(struct pdc_port_t *port, enum pdc_cmd_t pdc_cmd);

/**
 * @brief Run a command started by a public api function call
 */
static void run_public_api_command(struct pdc_port_t *port)
{
	/* Only allow public initiated commands from run states */
	if (!pdc_is_in_run_state(port)) {
		return;
	}

	/* Don't send another public initiated command if one is already pending
	 */
	if (port->send_cmd.public.pending) {
		return;
	}

	/* Send the public initiated command */
	if (atomic_test_and_clear_bit(port->pdc_cmd_flags, CMD_PDC_RESET)) {
		queue_public_cmd(port, CMD_PDC_RESET);
	} else if (atomic_test_and_clear_bit(port->pdc_cmd_flags,
					     CMD_PDC_GET_VBUS_VOLTAGE)) {
		queue_public_cmd(port, CMD_PDC_GET_VBUS_VOLTAGE);
	}
}

/**
 * @brief PDC thread
 */
static ALWAYS_INLINE void pdc_thread(void *pdc_dev, void *unused1,
				     void *unused2)
{
	const struct device *dev = (const struct device *)pdc_dev;
	struct pdc_data_t *data = dev->data;
	struct pdc_port_t *port = &data->port;

	while (1) {
		/* Run port connection state machine */
		smf_run_state(&port->ctx);
		/* Send any pending commands */
		run_send_cmd(port);
		/* Delay */
		k_sleep(K_MSEC(LOOP_DELAY_MS));
	}
}

#define PDC_SUBSYS_INIT(inst)                                                  \
	K_THREAD_STACK_DEFINE(my_stack_area_##inst,                            \
			      CONFIG_PDC_POWER_MGMT_STACK_SIZE);               \
                                                                               \
	static void create_thread_##inst(const struct device *dev)             \
	{                                                                      \
		struct pdc_data_t *data = dev->data;                           \
                                                                               \
		data->thread = k_thread_create(                                \
			&data->thread_data, my_stack_area_##inst,              \
			K_THREAD_STACK_SIZEOF(my_stack_area_##inst),           \
			pdc_thread, (void *)dev, 0, 0,                         \
			CONFIG_PDC_POWER_MGMT_THREAD_PRIORTY, K_ESSENTIAL,     \
			K_NO_WAIT);                                            \
		k_thread_name_set(data->thread,                                \
				  "PDC Power Mgmt" STRINGIFY(inst));           \
	}                                                                      \
                                                                               \
	static struct pdc_data_t data_##inst = {                               \
		.port.dev = DEVICE_DT_INST_GET(inst), /* Initial policy read   \
							 from device tree */   \
		.port.pdc = DEVICE_DT_GET(DT_INST_PROP(inst, pdc)),            \
		.port.una_policy.tcc = TC_CURRENT_1_5A, /* TODO: Read From DT  \
							 */                    \
                                                                               \
		.port.una_policy.cc_mode = CCOM_DRP, /* TODO: Read From DT */  \
		.port.una_policy.drp_mode = DRP_TRY_SRC, /* TODO: Read From DT \
							  */                   \
                                                                               \
	};                                                                     \
                                                                               \
	static struct pdc_config_t config_##inst = {                           \
		.connector_num = USBC_PORT_NEW(DT_DRV_INST(inst)),             \
		.create_thread = create_thread_##inst,                         \
	};                                                                     \
                                                                               \
	DEVICE_DT_INST_DEFINE(inst, &pdc_subsys_init, NULL, &data_##inst,      \
			      &config_##inst, POST_KERNEL,                     \
			      CONFIG_PDC_POWER_MGMT_INIT_PRIORITY, NULL);

DT_INST_FOREACH_STATUS_OKAY(PDC_SUBSYS_INIT)

#define PDC_DATA_INIT(inst) [USBC_PORT_NEW(DT_DRV_INST(inst))] = &data_##inst,

/**
 * @brief data structure used by public API to map port number to PDC_DATA.
 *        The port number is used to index the array.
 */
static struct pdc_data_t *pdc_data[] = { DT_INST_FOREACH_STATUS_OKAY(
	PDC_DATA_INIT) };

static enum pdc_state_t get_pdc_state(struct pdc_port_t *port)
{
	return port->ctx.current - &pdc_states[0];
}

static void set_pdc_state(struct pdc_port_t *port, enum pdc_state_t next_state)
{
	if (get_pdc_state(port) != next_state) {
		port->last_state = get_pdc_state(port);
		smf_set_state(SMF_CTX(port), &pdc_states[next_state]);
	}
}

static void print_current_pdc_state(struct pdc_port_t *port)
{
	const struct pdc_config_t *const config = port->dev->config;

	LOG_INF("C%d: %s", config->connector_num,
		pdc_state_names[get_pdc_state(port)]);
}

static void send_cmd_init(struct pdc_port_t *port)
{
	port->send_cmd.start_retry_counter = 0;
	port->send_cmd.wait_retry_counter = 0;
	port->send_cmd.public.cmd = CMD_PDC_NONE;
	port->send_cmd.public.error = false;
	port->send_cmd.public.pending = false;
	port->send_cmd.intern.cmd = CMD_PDC_NONE;
	port->send_cmd.intern.error = false;
	port->send_cmd.intern.pending = false;
	port->send_cmd.local_state = SEND_CMD_START_ENTRY;
}

static void queue_public_cmd(struct pdc_port_t *port, enum pdc_cmd_t pdc_cmd)
{
	LOG_DBG("SEND_CMD: %d\n", pdc_cmd);

	/* Already have a pending public command */
	if (port->send_cmd.public.pending) {
		return;
	}
	port->send_cmd.public.cmd = pdc_cmd;
	port->send_cmd.intern.error = false;
	port->send_cmd.public.pending = true;
}

static void queue_internal_cmd(struct pdc_port_t *port, enum pdc_cmd_t pdc_cmd)
{
	/* Already have a pending internal command */
	if (port->send_cmd.intern.pending) {
		return;
	}
	port->send_cmd.intern.cmd = pdc_cmd;
	port->send_cmd.intern.error = false;
	port->send_cmd.intern.pending = true;
}

static void send_snk_path_en_cmd(struct pdc_port_t *port, bool en)
{
	port->sink_path_en = en;
	queue_internal_cmd(port, CMD_PDC_SET_SINK_PATH);
}

/**
 * @brief Returns true if the local states are running and not performing some
 * initialization
 */
static bool pdc_is_in_run_state(struct pdc_port_t *port)
{
	enum pdc_state_t pdc_state = get_pdc_state(port);

	if (pdc_state == PDC_SNK_ATTACHED) {
		return (port->snk_attached_local_state == SNK_ATTACHED_RUN);
	} else if (pdc_state == PDC_SRC_ATTACHED) {
		return (port->src_attached_local_state == SRC_ATTACHED_RUN);
	} else if (pdc_state == PDC_UNATTACHED) {
		return (port->unattached_local_state == UNATTACHED_RUN);
	}

	return false;
}

/**
 * @brief Entering connector change event state
 */
static void pdc_connector_change_event_entry(void *obj)
{
	struct pdc_port_t *port = (struct pdc_port_t *)obj;

	port->connector_local_state = CONNECTOR_WAIT_CHANGE;
}

/**
 * @brief Run connector change event state
 */
static void pdc_connector_change_event_run(void *obj)
{
	struct pdc_port_t *port = (struct pdc_port_t *)obj;

	switch (port->connector_local_state) {
	case CONNECTOR_WAIT_CHANGE:
		/* Wait for a CCI event */
		if (!atomic_test_bit(port->cci_flags, CCI_EVENT)) {
			break;
		}
		/* Wait for any pending commands to finish */
		if (port->send_cmd.intern.pending ||
		    port->send_cmd.public.pending) {
			break;
		}

		/* CCI event detected */
		port->connector_local_state = CONNECTOR_GET_STATUS;
		/* fall-through */
	case CONNECTOR_GET_STATUS:
		/* Clear any left over flags */
		atomic_clear(port->cci_flags);
		/* Send command */
		queue_internal_cmd(port, CMD_PDC_GET_CONNECTOR_STATUS);
		/* Wait fro command to send */
		port->connector_local_state = CONNECTOR_GET_STATUS_WAIT;
		/* fall-through */
	case CONNECTOR_GET_STATUS_WAIT:
		/* wait until command completes or error. */
		if (port->send_cmd.intern.error) {
			/* try again */
			port->connector_local_state = CONNECTOR_GET_STATUS;
		} else if (port->send_cmd.intern.pending) {
			break;
		}
		port->connector_local_state = CONNECTOR_EVAL_STATUS;
		/* fall-through */
	case CONNECTOR_EVAL_STATUS:
		if (!port->connector_status.connect_status) {
			/* Connector status retrieved, clear the CCI_EVENT */
			atomic_clear_bit(port->cci_flags, CCI_EVENT);
			/* Not connected */
			set_pdc_state(port, PDC_UNATTACHED);
			port->connector_local_state = CONNECTOR_WAIT_CHANGE;
			break;
		}
		port->connector_local_state = CONNECTOR_SET_DR_SWAP_POLICY;
		/* fall-through */
	case CONNECTOR_SET_DR_SWAP_POLICY:
		port->uor.accept_dr_swap = 1; /* TODO read from DT */
		queue_internal_cmd(port, CMD_PDC_SET_UOR);
		port->connector_local_state = CONNECTOR_SET_DR_SWAP_POLICY_WAIT;
		/* fall-through */
	case CONNECTOR_SET_DR_SWAP_POLICY_WAIT:
		if (port->send_cmd.intern.error) {
			/* try again */
			port->connector_local_state =
				CONNECTOR_SET_DR_SWAP_POLICY;
			break;
		} else if (port->send_cmd.intern.pending) {
			break;
		}
		port->connector_local_state = CONNECTOR_SET_PR_SWAP_POLICY;
		/* fall-through */
	case CONNECTOR_SET_PR_SWAP_POLICY:
		port->pdr.accept_pr_swap = 1; /* TODO read from DT */
		queue_internal_cmd(port, CMD_PDC_SET_PDR);
		port->connector_local_state = CONNECTOR_SET_PR_SWAP_POLICY_WAIT;
		/* fall-through */
	case CONNECTOR_SET_PR_SWAP_POLICY_WAIT:
		if (port->send_cmd.intern.error) {
			/* try again */
			port->connector_local_state =
				CONNECTOR_SET_PR_SWAP_POLICY;
			break;
		} else if (port->send_cmd.intern.pending) {
			break;
		}
		/* Connector status retrieved, clear the CCI_EVENT */
		atomic_clear_bit(port->cci_flags, CCI_EVENT);

		if (port->connector_status.power_direction) {
			/* Port partner is a sink device */
			set_pdc_state(port, PDC_SRC_ATTACHED);
		} else {
			/* Port partner is a source device */
			set_pdc_state(port, PDC_SNK_ATTACHED);
		}

		port->connector_local_state = CONNECTOR_WAIT_CHANGE;
		break;
	}
}

/**
 * @brief Entering unattached state
 */
static void pdc_unattached_entry(void *obj)
{
	struct pdc_port_t *port = (struct pdc_port_t *)obj;

	print_current_pdc_state(port);

	port->ccaps_ready = false;
	port->send_cmd.intern.pending = false;
	port->unattached_local_state = UNATTACHED_SET_SINK_PATH_OFF;
}

/**
 * @brief Run unattached state
 */
static void pdc_unattached_run(void *obj)
{
	struct pdc_port_t *port = (struct pdc_port_t *)obj;

	/* A connection event has occurred. Wait until it has been resolved */
	if (atomic_test_bit(port->cci_flags, CCI_EVENT)) {
		return;
	}

	switch (port->unattached_local_state) {
	case UNATTACHED_SET_SINK_PATH_OFF:
		send_snk_path_en_cmd(port, false);
		port->unattached_local_state =
			UNATTACHED_SET_SINK_PATH_OFF_WAIT;
		/* fall-through */
	case UNATTACHED_SET_SINK_PATH_OFF_WAIT:
		if (port->send_cmd.intern.error) {
			/* try again */
			port->unattached_local_state =
				UNATTACHED_SET_SINK_PATH_OFF;
			break;
		} else if (port->send_cmd.intern.pending) {
			break;
		}
		port->unattached_local_state = UNATTACHED_RUN;
		break;
	case UNATTACHED_RUN:
		/* Enforce Unattached Policies */
		if (atomic_test_and_clear_bit(port->una_policy.flags,
					      UNA_POLICY_CC_MODE)) {
			/* Set CC PULL Resistor and TrySrc or TrySnk */
			queue_internal_cmd(port, CMD_PDC_SET_CCOM);
		} else if (atomic_test_and_clear_bit(port->una_policy.flags,
						     UNA_POLICY_TCC)) {
			/* Set RP current policy */
			queue_internal_cmd(port, CMD_PDC_SET_POWER_LEVEL);
		} else {
			/* Run public API call */
			run_public_api_command(port);
		}
	}
}

/**
 * @brief Entering source attached state
 */
static void pdc_src_attached_entry(void *obj)
{
	struct pdc_port_t *port = (struct pdc_port_t *)obj;

	print_current_pdc_state(port);
	port->send_cmd.intern.pending = false;

	if (port->last_state == PDC_SNK_ATTACHED) {
		port->src_attached_local_state = SRC_ATTACHED_SET_SINK_PATH_OFF;
	} else {
		port->src_attached_local_state = SRC_ATTACHED_RUN;
	}
}

/**
 * @brief Run source attached state
 */
static void pdc_src_attached_run(void *obj)
{
	struct pdc_port_t *port = (struct pdc_port_t *)obj;

	/* A connection event has occurred. Wait until it has been resolved */
	if (atomic_test_bit(port->cci_flags, CCI_EVENT)) {
		return;
	}

	switch (port->src_attached_local_state) {
	case SRC_ATTACHED_SET_SINK_PATH_OFF:
		send_snk_path_en_cmd(port, false);
		port->src_attached_local_state =
			SRC_ATTACHED_SET_SINK_PATH_OFF_WAIT;
		/* fall-through */
	case SRC_ATTACHED_SET_SINK_PATH_OFF_WAIT:
		if (port->send_cmd.intern.error) {
			/* try again */
			port->src_attached_local_state =
				UNATTACHED_SET_SINK_PATH_OFF;
			break;
		} else if (port->send_cmd.intern.pending) {
			break;
		}
		port->src_attached_local_state = SRC_ATTACHED_RUN;
		break;
	case SRC_ATTACHED_RUN:
		/* Run public API call */
		run_public_api_command(port);
		break;
	}
}

static int start_pdc_cmd(struct pdc_port_t *port)
{
	switch (port->cmd->cmd) {
	case CMD_PDC_RESET:
		return pdc_reset(port->pdc);
	case CMD_PDC_GET_INFO:
		return pdc_get_info(port->pdc, &port->info);
	case CMD_PDC_SET_POWER_LEVEL:
		return pdc_set_power_level(port->pdc, port->una_policy.tcc);
	case CMD_PDC_SET_CCOM:
		return pdc_set_ccom(port->pdc, port->una_policy.cc_mode,
				    port->una_policy.drp_mode);
	case CMD_PDC_GET_PDOS:
		return pdc_get_pdos(port->pdc, SOURCE_PDO, PDO_OFFSET_0,
				    PDO_NUM, true, &port->snk_policy.pdos[0]);
	case CMD_PDC_GET_RDO:
		return pdc_get_rdo(port->pdc, &port->snk_policy.rdo);
	case CMD_PDC_SET_RDO:
		return pdc_set_rdo(port->pdc, port->snk_policy.rdo_to_send);
	case CMD_PDC_GET_VBUS_VOLTAGE:
		return pdc_get_vbus_voltage(port->pdc, &port->vbus);
	case CMD_PDC_SET_SINK_PATH:
		return pdc_set_sink_path(port->pdc, port->sink_path_en);
	case CMD_PDC_READ_POWER_LEVEL:
		return pdc_read_power_level(port->pdc);
	case CMD_PDC_GET_CONNECTOR_CAPABILITY:
		return pdc_get_connector_capability(port->pdc, &port->ccaps);
	case CMD_PDC_SET_UOR:
		return pdc_set_uor(port->pdc, port->uor);
	case CMD_PDC_SET_PDR:
		return pdc_set_pdr(port->pdc, port->pdr);
	case CMD_PDC_GET_CONNECTOR_STATUS:
		return pdc_get_connector_status(port->pdc,
						&port->connector_status);
	default:
		return -EINVAL;
	}
}

static void run_send_cmd(struct pdc_port_t *port)
{
	switch (port->send_cmd.local_state) {
	case SEND_CMD_START_ENTRY:
		port->send_cmd.start_retry_counter = 0;
		port->send_cmd.local_state = SEND_CMD_START_RUN;
		/* fall-through */
	case SEND_CMD_START_RUN:
		int rv = 0;

		if (port->send_cmd.intern.pending) {
			port->cmd = &port->send_cmd.intern;
		} else if (port->send_cmd.public.pending) {
			port->cmd = &port->send_cmd.public;
		} else {
			break;
		}

		/* Send PDC command via driver API */
		rv = start_pdc_cmd(port);

		/* Test if command was successful. If not, try again until max
		 * retries is reached */
		if (rv) {
			port->send_cmd.start_retry_counter++;
			if (port->send_cmd.start_retry_counter > RETRY_MAX) {
				/* Could not send command: TODO handle error */
				LOG_ERR("Command retry timout");
				port->cmd->error = true;
				port->cmd->pending = false;
				port->send_cmd.local_state =
					SEND_CMD_START_ENTRY;
			}
			/* Failed */
			break;
		}
		/* Command was sent successfully */
		port->send_cmd.local_state = SEND_CMD_WAIT_ENTRY;
		/* fall-through */
	case SEND_CMD_WAIT_ENTRY:
		port->send_cmd.wait_retry_counter = 0;
		port->send_cmd.local_state = SEND_CMD_WAIT_RUN;
		/* fall-through */
	case SEND_CMD_WAIT_RUN:
		/* Wait for command status notification from driver */

		/*
		 * On a PDC_RESET, the PDC sets CCI_RESET_COMPLETED to notify
		 * that the reset is complete
		 */
		if (port->cmd->cmd == CMD_PDC_RESET) {
			if (atomic_test_and_clear_bit(port->cci_flags,
						      CCI_RESET_COMPLETED)) {
				port->cmd->error = false;
				port->cmd->pending = false;
				port->send_cmd.local_state =
					SEND_CMD_START_ENTRY;
			}
		} else if (atomic_test_and_clear_bit(port->cci_flags,
						     CCI_BUSY)) {
			LOG_DBG("CCI_BUSY");
			/* Busy is treated the same as a retry */
			port->send_cmd.wait_retry_counter++;
			if (port->send_cmd.wait_retry_counter == RETRY_MAX) {
				/* Could not send command: TODO handle error */
				LOG_ERR("Could not send command");
				port->cmd->error = true;
				port->cmd->pending = false;
				port->send_cmd.local_state =
					SEND_CMD_START_ENTRY;
			}
		} else if (atomic_test_and_clear_bit(port->cci_flags,
						     CCI_ERROR)) {
			LOG_DBG("CCI_ERROR");
			port->send_cmd.wait_retry_counter++;
			if (port->send_cmd.wait_retry_counter == RETRY_MAX) {
				/* Could not send command: TODO handle error */
				LOG_ERR("Could not send command");
				port->cmd->error = true;
				port->cmd->pending = false;
			}
			/* Try to send the command again if max retry hasn't
			 * been reached */
			port->send_cmd.local_state = SEND_CMD_START_ENTRY;
		} else if (atomic_test_and_clear_bit(port->cci_flags,
						     CCI_CMD_COMPLETED)) {
			LOG_DBG("CCI_CMD_COMPLETED: %p", port->cmd);
			port->cmd->error = false;
			port->cmd->pending = false;
			port->send_cmd.local_state = SEND_CMD_WAIT_EXIT;
		} else {
			break;
		}
		/* fall-through */
	case SEND_CMD_WAIT_EXIT:
		/* */
		switch (port->cmd->cmd) {
		case CMD_PDC_GET_PDOS:
			/* Filter out Augmented Power Data Objects (APDO) */
			/* TODO This is temporary until APDOs can be handled  */
			for (int i = 0; i < 7; i++) {
				if (port->snk_policy.pdos[i] &
				    PDO_TYPE_AUGMENTED) {
					port->snk_policy.pdos[i] = 0;
				}
			}
			break;
		case CMD_PDC_GET_CONNECTOR_CAPABILITY:
			port->ccaps_ready = true;
			break;
		default:
		}
		port->send_cmd.local_state = SEND_CMD_START_ENTRY;
		break;
	}
}

/**
 * @brief Entering sink attached state
 */
static void pdc_snk_attached_entry(void *obj)
{
	struct pdc_port_t *port = (struct pdc_port_t *)obj;

	print_current_pdc_state(port);

	port->send_cmd.intern.pending = false;
	port->snk_attached_local_state = SNK_ATTACHED_GET_CONNECTOR_CAPABILITY;
}

/**
 * @brief Run sink attached state.
 */
static void pdc_snk_attached_run(void *obj)
{
	struct pdc_port_t *port = (struct pdc_port_t *)obj;
	const struct pdc_config_t *const config = port->dev->config;
	uint32_t max_ma, max_mv, max_mw;

	/* A connection event has occurred. Wait until it has been resolved */
	if (atomic_test_bit(port->cci_flags, CCI_EVENT)) {
		return;
	}

	switch (port->snk_attached_local_state) {
	case SNK_ATTACHED_GET_CONNECTOR_CAPABILITY:
		queue_internal_cmd(port, CMD_PDC_GET_CONNECTOR_CAPABILITY);
		port->snk_attached_local_state =
			SNK_ATTACHED_GET_CONNECTOR_CAPABILITY_WAIT;
		/* fall-through */
	case SNK_ATTACHED_GET_CONNECTOR_CAPABILITY_WAIT:
		if (port->send_cmd.intern.error) {
			/* try again */
			port->snk_attached_local_state =
				SNK_ATTACHED_GET_CONNECTOR_CAPABILITY;
			break;
		} else if (port->send_cmd.intern.pending) {
			break;
		}
		port->snk_attached_local_state = SNK_ATTACHED_READ_POWER_LEVEL;
		break;
	case SNK_ATTACHED_READ_POWER_LEVEL:
		queue_internal_cmd(port, CMD_PDC_READ_POWER_LEVEL);
		port->snk_attached_local_state =
			SNK_ATTACHED_READ_POWER_LEVEL_WAIT;
		/* fall-through */
	case SNK_ATTACHED_READ_POWER_LEVEL_WAIT:
		if (port->send_cmd.intern.error) {
			/* try again */
			port->snk_attached_local_state =
				SNK_ATTACHED_READ_POWER_LEVEL;
			break;
		} else if (port->send_cmd.intern.pending) {
			break;
		}
		port->snk_attached_local_state = SNK_ATTACHED_GET_PDOS;
		break;
	case SNK_ATTACHED_GET_PDOS:
		queue_internal_cmd(port, CMD_PDC_GET_PDOS);
		port->snk_attached_local_state = SNK_ATTACHED_GET_PDOS_WAIT;
		/* fall-through */
	case SNK_ATTACHED_GET_PDOS_WAIT:
		if (port->send_cmd.intern.error) {
			/* try again */
			port->snk_attached_local_state = SNK_ATTACHED_GET_PDOS;
			break;
		} else if (port->send_cmd.intern.pending) {
			break;
		}
		port->snk_attached_local_state = SNK_ATTACHED_GET_RDO;
		break;
	case SNK_ATTACHED_GET_RDO:
		queue_internal_cmd(port, CMD_PDC_GET_RDO);
		port->snk_attached_local_state = SNK_ATTACHED_GET_RDO_WAIT;
		/* fall-through */
	case SNK_ATTACHED_GET_RDO_WAIT:
		if (port->send_cmd.intern.error) {
			/* try again */
			port->snk_attached_local_state = SNK_ATTACHED_GET_RDO;
			break;
		} else if (port->send_cmd.intern.pending) {
			break;
		}

		/* Test if battery can be charged from this port */
		if (port->active_charge) {
			port->snk_attached_local_state =
				SNK_ATTACHED_SET_SINK_PATH_ON;
		} else {
			port->snk_attached_local_state = SNK_ATTACHED_RUN;
		}
		break;
	case SNK_ATTACHED_SET_SINK_PATH_ON:
		send_snk_path_en_cmd(port, true);
		port->snk_attached_local_state =
			SNK_ATTACHED_SET_SINK_PATH_ON_WAIT;
		/* fall-through */
	case SNK_ATTACHED_SET_SINK_PATH_ON_WAIT:
		if (port->send_cmd.intern.error) {
			/* try again */
			port->snk_attached_local_state =
				SNK_ATTACHED_SET_SINK_PATH_ON;
			break;
		} else if (port->send_cmd.intern.pending) {
			break;
		}
		port->snk_attached_local_state = SNK_ATTACHED_START_CHARGING;
		break;
	case SNK_ATTACHED_START_CHARGING:
		for (int i = 0; i < PDO_NUM; i++) {
			LOG_INF("PDO%d: %08x, %d %d", i,
				port->snk_policy.pdos[i],
				PDO_FIXED_GET_VOLT(port->snk_policy.pdos[i]),
				PDO_FIXED_GET_CURR(port->snk_policy.pdos[i]));
		}

		LOG_INF("RDO: %d", RDO_POS(port->snk_policy.rdo));
		port->snk_policy.pdo =
			port->snk_policy.pdos[RDO_POS(port->snk_policy.rdo) - 1];

		/* Extract Current, Voltage, and calculate Power */
		max_ma = PDO_FIXED_GET_CURR(port->snk_policy.pdo);
		max_mv = PDO_FIXED_GET_VOLT(port->snk_policy.pdo);
		max_mw = max_ma * max_mv / 1000;

		LOG_INF("Charging ON PORT%d\n", config->connector_num);
		LOG_INF("PDO: %08x", port->snk_policy.pdo);
		LOG_INF("V: %d", max_mv);
		LOG_INF("C: %d", max_ma);
		LOG_INF("P: %d", max_mw);

		pd_set_input_current_limit(config->connector_num, max_ma,
					   max_mv);
		charge_manager_set_ceil(config->connector_num,
					CEIL_REQUESTOR_PD, max_ma);

		if (((PDO_GET_TYPE(port->snk_policy.pdo) == 0) &&
		     (!(port->snk_policy.pdo & PDO_FIXED_GET_DRP) ||
		      (port->snk_policy.pdo &
		       PDO_FIXED_GET_UNCONSTRAINED_PWR))) ||
		    (max_mw >= PD_DRP_CHARGE_POWER_MIN)) {
			charge_manager_update_dualrole(config->connector_num,
						       CAP_DEDICATED);
		} else {
			charge_manager_update_dualrole(config->connector_num,
						       CAP_DUALROLE);
		}

		port->snk_attached_local_state = SNK_ATTACHED_RUN;
		/* fall-through */
	case SNK_ATTACHED_RUN:
		/* Enforce Sink Policies */
		if (atomic_test_and_clear_bit(port->snk_policy.flags,
					      SNK_POLICY_NEW_POWER_REQUEST)) {
			port->snk_attached_local_state = SNK_ATTACHED_GET_PDOS;
		} else {
			run_public_api_command(port);
		}
		break;
	}
}

/**
 * @brief Exiting from sink attached state.
 */
static void pdc_snk_attached_exit(void *obj)
{
	struct pdc_port_t *port = (struct pdc_port_t *)obj;
	const struct pdc_config_t *const config = port->dev->config;

	/* Set input current limit to zero */
	pd_set_input_current_limit(config->connector_num, 0, 0);
	/* Set charge ceil to none */
	charge_manager_set_ceil(config->connector_num, CEIL_REQUESTOR_PD,
				CHARGE_CEIL_NONE);
	charge_manager_update_dualrole(config->connector_num, CAP_UNKNOWN);

	/* Invalidate PDO */
	port->snk_policy.pdo = 0;
}

/**
 * @brief Populate state table
 */
static const struct smf_state pdc_states[] = {
	/* Parent States */
	[PDC_CONNECTOR_CHANGE_EVENT] =
		SMF_CREATE_STATE(pdc_connector_change_event_entry,
				 pdc_connector_change_event_run, NULL, NULL),
	/* Normal States */
	[PDC_UNATTACHED] =
		SMF_CREATE_STATE(pdc_unattached_entry, pdc_unattached_run, NULL,
				 &pdc_states[PDC_CONNECTOR_CHANGE_EVENT]),
	[PDC_SNK_ATTACHED] = SMF_CREATE_STATE(
		pdc_snk_attached_entry, pdc_snk_attached_run,
		pdc_snk_attached_exit, &pdc_states[PDC_CONNECTOR_CHANGE_EVENT]),
	[PDC_SRC_ATTACHED] =
		SMF_CREATE_STATE(pdc_src_attached_entry, pdc_src_attached_run,
				 NULL, &pdc_states[PDC_CONNECTOR_CHANGE_EVENT]),
};

/**
 * @brief CCI event handler call back
 */
static void pdc_cci_handler_cb(union cci_event_t cci_event, void *cb_data)
{
	struct pdc_port_t *port = (struct pdc_port_t *)cb_data;

	/* Handle reset completed event from driver */
	if (cci_event.reset_completed) {
		atomic_set_bit(port->cci_flags, CCI_RESET_COMPLETED);
	}

	/* Handle busy event from driver */
	if (cci_event.busy) {
		atomic_set_bit(port->cci_flags, CCI_BUSY);
	}

	/* Handle error event from driver */
	if (cci_event.error) {
		atomic_set_bit(port->cci_flags, CCI_ERROR);
	}

	/* Handle command completed event from driver */
	if (cci_event.command_completed) {
		atomic_set_bit(port->cci_flags, CCI_CMD_COMPLETED);
	}

	/* Handle generic vendor defined event from driver */
	if (cci_event.vendor_defined_indicator) {
		atomic_set_bit(port->cci_flags, CCI_EVENT);
	}
}

/**
 * @brief Initialize the PDC Subsystem
 */
static int pdc_subsys_init(const struct device *dev)
{
	struct pdc_data_t *data = dev->data;
	struct pdc_port_t *port = &data->port;
	const struct pdc_config_t *const config = dev->config;

	/* Make sure PD Controller is ready */
	if (!device_is_ready(port->pdc)) {
		LOG_ERR("PDC not ready");
		return -ENODEV;
	}

	/* Init port variables */

	atomic_clear(port->pdc_cmd_flags);
	atomic_clear(port->cci_flags);

	/* Initialize Send Command data */
	send_cmd_init(port);
	/* Set cci call back */
	pdc_set_handler_cb(port->pdc, pdc_cci_handler_cb, (void *)port);
	/* Initialize the connection state machine */
	atomic_set_bit(port->cci_flags, CCI_EVENT);
	smf_set_initial(&port->ctx, &pdc_states[PDC_CONNECTOR_CHANGE_EVENT]);
	/* Create the thread for this port */
	config->create_thread(dev);

	return 0;
}

/**
 * @brief Called from a public API function to block until the command completes
 * or time outs
 */
static int public_api_block(int port, enum pdc_cmd_t pdc_cmd)
{
	/* Block calling thread until command is processed, errors or timeout
	 * occurs. */
	while (pdc_data[port]->port.send_cmd.public.pending &&
	       !pdc_data[port]->port.send_cmd.public.error) {
		/* block until command completes or max block count is reached
		 */

		/* give time for command to be processed */
		k_sleep(K_MSEC(LOOP_DELAY_MS));
		pdc_data[port]->port.block_counter++;
		if (pdc_data[port]->port.block_counter > BLOCK_COUNTER_MAX) {
			/* something went wrong */
			LOG_ERR("Public API blocking timeout");
			return 1;
		}
	}

	if (pdc_data[port]->port.send_cmd.public.error) {
		LOG_ERR("Public API command not sent");
		return 1;
	}

	return 0;
}

/**
 * PDC Power Management Public API
 */
bool pdc_power_mgmt_is_sink_connected(int port)
{
	if (port > CONFIG_USB_PD_PORT_MAX_COUNT) {
		return false;
	}

	if (get_pdc_state(&pdc_data[port]->port) != PDC_SNK_ATTACHED) {
		return false;
	}

	return true;
}

bool pdc_power_mgmt_is_source_connected(int port)
{
	if (port > CONFIG_USB_PD_PORT_MAX_COUNT) {
		return false;
	}

	if (get_pdc_state(&pdc_data[port]->port) != PDC_SRC_ATTACHED) {
		return false;
	}

	return true;
}

bool pdc_power_mgmt_is_connected(int port)
{
	if (port > CONFIG_USB_PD_PORT_MAX_COUNT) {
		return false;
	}

	if (get_pdc_state(&pdc_data[port]->port) == PDC_UNATTACHED) {
		return false;
	}

	return true;
}

uint8_t pdc_power_mgmt_get_usb_pd_port_count(void)
{
	return CONFIG_USB_PD_PORT_MAX_COUNT;
}

int pdc_power_mgmt_set_active_charge_port(int charge_port)
{
	if (charge_port >= CONFIG_USB_PD_PORT_MAX_COUNT) {
		return 1;
	}

	for (int i = 0; i < CONFIG_USB_PD_PORT_MAX_COUNT; i++) {
		if (i == charge_port) {
			pdc_data[i]->port.active_charge = true;
		} else {
			pdc_data[i]->port.active_charge = false;
		}
	}

	return EC_SUCCESS;
}

void pdc_power_mgmt_set_new_power_request(int port)
{
	/* Make sure port is sink connected */
	if (!pdc_power_mgmt_is_sink_connected(port)) {
		return;
	}

	atomic_set_bit(pdc_data[port]->port.snk_policy.flags,
		       SNK_POLICY_NEW_POWER_REQUEST);
}

uint8_t pdc_power_mgmt_get_task_state(int port)
{
	/* TODO */
	return 0;
}

int pdc_power_mgmt_comm_is_enabled(int port)
{
	/* Make sure port is connected */
	if (!pdc_power_mgmt_is_connected(port)) {
		return false;
	}

	/* TODO */
	return true;
}

bool pdc_power_mgmt_get_vconn_state(int port)
{
	/* Make sure port is connected */
	if (!pdc_power_mgmt_is_connected(port)) {
		return false;
	}

	/* TODO: Add driver support for this */
	return true;
}

bool pdc_power_mgmt_get_partner_usb_comm_capable(int port)
{
	/* Make sure port is sink connected */
	if (!pdc_power_mgmt_is_sink_connected(port)) {
		return false;
	}

	return (pdc_data[port]->port.snk_policy.pdo &
		PDO_FIXED_GET_USB_COMM_CAPABLE);
}

bool pdc_power_mgmt_get_partner_unconstr_power(int port)
{
	/* Make sure port is sink connected */
	if (!pdc_power_mgmt_is_sink_connected(port)) {
		return false;
	}

	return (pdc_data[port]->port.snk_policy.pdo &
		PDO_FIXED_GET_UNCONSTRAINED_PWR);
}

int pdc_power_mgmt_accept_data_swap(int port, bool val)
{
	/* Make sure port is connected */
	if (!pdc_power_mgmt_is_connected(port)) {
		return 1;
	}

	/* Set DR accept swap policy */
	pdc_data[port]->port.uor.raw_value = 0;
	pdc_data[port]->port.uor.accept_dr_swap = val;

	/* Reset block counter */
	pdc_data[port]->port.block_counter = 0;

	/* Set flag to send the command */
	atomic_set_bit(pdc_data[port]->port.pdc_cmd_flags, CMD_PDC_SET_UOR);

	/* Block until command completes */
	if (public_api_block(port, CMD_PDC_SET_UOR)) {
		/* something went wrong */
		return 1;
	}

	return 0;
}

int pdc_power_mgmt_accept_power_swap(int port, bool val)
{
	/* Make sure port is connected */
	if (!pdc_power_mgmt_is_connected(port)) {
		return 1;
	}

	/* Set PR accept swap policy */
	pdc_data[port]->port.pdr.raw_value = 0;
	pdc_data[port]->port.pdr.accept_pr_swap = val;

	/* Reset block counter */
	pdc_data[port]->port.block_counter = 0;

	/* Set flag to send the command */
	atomic_set_bit(pdc_data[port]->port.pdc_cmd_flags, CMD_PDC_SET_PDR);

	/* Block until command completes */
	if (public_api_block(port, CMD_PDC_SET_PDR)) {
		/* something went wrong */
		return 1;
	}

	return EC_SUCCESS;
}

static int pdc_power_mgmt_request_data_swap(int port, enum pd_data_role role)
{
	/* Make sure port is connected */
	if (!pdc_power_mgmt_is_connected(port)) {
		return 1;
	}

	/* Set DR accept swap policy */
	if (role == PD_ROLE_UFP) {
		/* Attempt to swapt to UFP */
		pdc_data[port]->port.uor.swap_to_dfp = 0;
		pdc_data[port]->port.uor.swap_to_ufp = 1;
	} else {
		/* Attempt to swapt to DFP */
		pdc_data[port]->port.uor.swap_to_dfp = 1;
		pdc_data[port]->port.uor.swap_to_ufp = 0;
	}

	/* Reset block counter */
	pdc_data[port]->port.block_counter = 0;

	/* Set flag to send the command */
	atomic_set_bit(pdc_data[port]->port.pdc_cmd_flags, CMD_PDC_SET_UOR);

	/* Block until command completes */
	if (public_api_block(port, CMD_PDC_SET_UOR)) {
		/* something went wrong */
		return 1;
	}

	return EC_SUCCESS;
}

void pdc_power_mgmt_request_data_swap_to_ufp(int port)
{
	pdc_power_mgmt_request_data_swap(port, PD_ROLE_UFP);
}

void pdc_power_mgmt_request_data_swap_to_dfp(int port)
{
	pdc_power_mgmt_request_data_swap(port, PD_ROLE_DFP);
}

static int pdc_power_mgmt_request_power_swap(int port, enum pd_power_role role)
{
	/* Make sure port is connected */
	if (!pdc_power_mgmt_is_connected(port)) {
		return 1;
	}

	/* Set DR accept swap policy */
	if (role == PD_ROLE_SOURCE) {
		/* Attempt to swap to SOURCE */
		pdc_data[port]->port.pdr.swap_to_snk = 0;
		pdc_data[port]->port.pdr.swap_to_src = 1;
	} else {
		/* Attempt to swap to SINK */
		pdc_data[port]->port.pdr.swap_to_snk = 1;
		pdc_data[port]->port.pdr.swap_to_src = 0;
	}

	/* Reset block counter */
	pdc_data[port]->port.block_counter = 0;

	/* Set flag to send the command */
	atomic_set_bit(pdc_data[port]->port.pdc_cmd_flags, CMD_PDC_SET_PDR);

	/* Block until command completes */
	if (public_api_block(port, CMD_PDC_SET_PDR)) {
		/* something went wrong */
		return 1;
	}

	return EC_SUCCESS;
}

void pdc_power_mgmt_request_swap_to_src(int port)
{
	pdc_power_mgmt_request_power_swap(port, PD_ROLE_SOURCE);
}

void pdc_power_mgmt_request_swap_to_snk(int port)
{
	pdc_power_mgmt_request_power_swap(port, PD_ROLE_SINK);
}

enum tcpc_cc_polarity pdc_power_mgmt_pd_get_polarity(int port)
{
	/* Make sure port is connected */
	if (!pdc_power_mgmt_is_connected(port)) {
		return PD_ROLE_SINK;
	}

	if (pdc_data[port]->port.connector_status.orientation) {
		return POLARITY_CC2;
	}

	return POLARITY_CC1;
}

enum pd_data_role pdc_power_mgmt_pd_get_data_role(int port)
{
	/* Make sure port is connected */
	if (!pdc_power_mgmt_is_connected(port)) {
		return PD_ROLE_DISCONNECTED;
	}

	if (pdc_data[port]->port.connector_status.conn_partner_type ==
	    DFP_ATTACHED) {
		return PD_ROLE_UFP;
	}

	return PD_ROLE_DFP;
}

enum pd_power_role pdc_power_mgmt_get_power_role(int port)
{
	/* Make sure port is connected */
	if (!pdc_power_mgmt_is_connected(port)) {
		return PD_ROLE_SINK;
	}

	if (pdc_data[port]->port.connector_status.power_direction) {
		return PD_ROLE_SOURCE;
	}

	return PD_ROLE_SINK;
}

enum pd_cc_states pdc_power_mgmt_get_task_cc_state(int port)
{
	/* Make sure port is connected */
	if (!pdc_power_mgmt_is_connected(port)) {
		return PD_CC_NONE;
	}

	switch (pdc_data[port]->port.connector_status.conn_partner_type) {
	case DFP_ATTACHED:
		return PD_CC_DFP_ATTACHED;
	case UFP_ATTACHED:
		return PD_CC_UFP_ATTACHED;
	case POWERED_CABLE_NO_UFP_ATTACHED:
		return PD_CC_NONE;
	case POWERED_CABLE_UFP_ATTACHED:
		return PD_CC_UFP_ATTACHED;
	case DEBUG_ACCESSORY_ATTACHED:
		return PD_CC_UFP_DEBUG_ACC;
	case AUDIO_ADAPTER_ACCESSORY_ATTACHED:
		return PD_CC_UFP_AUDIO_ACC;
	}

	return PD_CC_NONE;
}

bool pdc_power_mgmt_pd_capable(int port)
{
	/* Make sure port is connected */
	if (!pdc_power_mgmt_is_connected(port)) {
		return false;
	}

	/* Check if the port partner is PD connected */
	if (pdc_data[port]->port.connector_status.power_operation_mode !=
	    PD_OPERATION) {
		return false;
	}

	/* PD capable */
	return true;
}

bool pdc_power_mgmt_get_partner_dual_role_power(int port)
{
	/* Make sure port is connected */
	if (!pdc_power_mgmt_is_connected(port)) {
		return false;
	}

	/*
	 * The subsystem is in an attached state, so wait until the
	 * connector capabilities are read.
	 */
	while (!pdc_data[port]->port.ccaps_ready) {
		k_sleep(K_MSEC(LOOP_DELAY_MS));
	}

	/* Return DRP capability */
	return pdc_data[port]->port.ccaps.op_mode_drp;
}

bool pdc_power_mgmt_get_partner_data_swap_capable(int port)
{
	/* Make sure port is connected */
	if (!pdc_power_mgmt_is_connected(port)) {
		return false;
	}

	/*
	 * The subsystem is in an attached state, so wait until the
	 * connector capabilities are read.
	 */
	while (!pdc_data[port]->port.ccaps_ready) {
		k_sleep(K_MSEC(LOOP_DELAY_MS));
	}

	/* Make sure port partner is DRP, RP only, or RD only */
	if (!pdc_data[port]->port.ccaps.op_mode_drp &&
	    !pdc_data[port]->port.ccaps.op_mode_rp_only &&
	    !pdc_data[port]->port.ccaps.op_mode_rd_only) {
		return false;
	}

	/* Return swap to UFP or DFP capability */
	return pdc_data[port]->port.ccaps.swap_to_dfp ||
	       pdc_data[port]->port.ccaps.swap_to_ufp;
}

uint32_t pdc_power_mgmt_get_vbus_voltage(int port)
{
	/* Make sure port is connected */
	if (!pdc_power_mgmt_is_connected(port)) {
		return 0;
	}

	/* Reset block counter */
	pdc_data[port]->port.block_counter = 0;

	/* Set flag to send the command */
	atomic_set_bit(pdc_data[port]->port.pdc_cmd_flags,
		       CMD_PDC_GET_VBUS_VOLTAGE);

	/* Block until command completes */
	if (public_api_block(port, CMD_PDC_GET_VBUS_VOLTAGE)) {
		/* something went wrong */
		return 0;
	}

	/* Return VBUS */
	return pdc_data[port]->port.vbus;
}

void pdc_power_mgmt_reset(int port)
{
	/* Make sure port is connected */
	if (!pdc_power_mgmt_is_connected(port)) {
		return;
	}

	/* Reset block counter */
	pdc_data[port]->port.block_counter = 0;

	/* Set flag to send the command */
	atomic_set_bit(pdc_data[port]->port.pdc_cmd_flags, CMD_PDC_RESET);

	/* Block until command completes */
	public_api_block(port, CMD_PDC_RESET);
}
