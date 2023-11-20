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
 * @brief PDC driver commands
 */
enum pdc_cmd_t {
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

	/** CMD_PDC_COUNT */
	CMD_PDC_COUNT
};

/**
 * @brief Send Local States
 */
enum cmd_send_local_state_t {
	/** CMD_SEND_START */
	CMD_SEND_START,
	/** CMD_SEND_WAIT */
	CMD_SEND_WAIT,
};

/**
 * @brief SNK Attached Local States
 */
enum snk_attached_local_state_t {
	/** SNK_ATTACHED_GET_CONNECTOR_CAPABILITY */
	SNK_ATTACHED_GET_CONNECTOR_CAPABILITY,
	/** SNK_ATTACHED_READ_POWER_LEVEL */
	SNK_ATTACHED_READ_POWER_LEVEL,
	/** SNK_ATTACHED_GET_PDOS */
	SNK_ATTACHED_GET_PDOS,
	/** SNK_ATTACHED_GET_RDO */
	SNK_ATTACHED_GET_RDO,
	/** SNK_ATTACHED_START_CHARGING */
	SNK_ATTACHED_START_CHARGING,
	/** SNK_ATTACHED_SET_SINK_PATH */
	SNK_ATTACHED_SET_SINK_PATH,
	/** SNK_ATTACHED_RUN */
	SNK_ATTACHED_RUN,
};

/**
 * @brief Connector Local States
 */
enum connector_local_state_t {
	/** CONNECTOR_WAIT_CHANGE */
	CONNECTOR_WAIT_CHANGE,
	/** CONNECTOR_GET_STATUS */
	CONNECTOR_GET_STATUS,
	/** CONNECTOR_EVAL_STATUS */
	CONNECTOR_EVAL_STATUS,
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
	/** PDC_CMD_SEND */
	PDC_CMD_SEND,
};

/**
 * @brief State Machine State Names
 */
static const char *const pdc_state_names[] = {
	[PDC_CONNECTOR_CHANGE_EVENT] = "PortChange.EVENT",
	[PDC_UNATTACHED] = "Unattached",
	[PDC_SNK_ATTACHED] = "Attached.SNK",
	[PDC_SRC_ATTACHED] = "Attached.SRC",
	[PDC_CMD_SEND] = "Send.CMD",
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
	uint32_t pdos[7];
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
	/** Sink attached local state variable */
	enum snk_attached_local_state_t snk_attached_local_state;
	/** Command send local state variable */
	enum cmd_send_local_state_t cmd_send_local_state;

	/** Transitioning from last_state */
	enum pdc_state_t last_state;
	/** Transitioning to next_state */
	enum pdc_state_t next_state;

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
	/** Retry counter that tracks the number of attempts to send a command
	 */
	uint8_t retry_counter;
	/** PDC command to send */
	enum pdc_cmd_t pdc_cmd;
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

static const struct smf_state pdc_states[];
static int pdc_subsys_init(const struct device *dev);
static void pdc_cmd_send(struct pdc_port_t *port, enum pdc_cmd_t pdc_cmd);

/**
 * @brief Run a command started by a public api function call
 */
static void run_public_api_command(struct pdc_port_t *port)
{
	if (atomic_test_bit(port->pdc_cmd_flags, CMD_PDC_RESET)) {
		pdc_cmd_send(port, CMD_PDC_RESET);
	} else if (atomic_test_bit(port->pdc_cmd_flags,
				   CMD_PDC_GET_VBUS_VOLTAGE)) {
		pdc_cmd_send(port, CMD_PDC_GET_VBUS_VOLTAGE);
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
		/* Delay */
		k_sleep(K_MSEC(200));
	}
}

#define PDC_SUBSYS_INIT(inst)                                                  \
	K_THREAD_STACK_DEFINE(my_stack_area_##inst, CONFIG_PDC_STACK_SIZE);    \
                                                                               \
	static void create_thread_##inst(const struct device *dev)             \
	{                                                                      \
		struct pdc_data_t *data = dev->data;                           \
                                                                               \
		data->thread = k_thread_create(                                \
			&data->thread_data, my_stack_area_##inst,              \
			K_THREAD_STACK_SIZEOF(my_stack_area_##inst),           \
			pdc_thread, (void *)dev, 0, 0,                         \
			CONFIG_PDC_THREAD_PRIORITY, K_ESSENTIAL, K_NO_WAIT);   \
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
		.connector_num = 0, /*USBC_PORT_NEW(inst), */                  \
		.create_thread = create_thread_##inst,                         \
	};                                                                     \
                                                                               \
	DEVICE_DT_INST_DEFINE(inst, &pdc_subsys_init, NULL, &data_##inst,      \
			      &config_##inst, POST_KERNEL,                     \
			      CONFIG_PDC_STACK_INIT_PRIORITY, NULL);

DT_INST_FOREACH_STATUS_OKAY(PDC_SUBSYS_INIT)

#define PDC_DATA_INIT(inst) &data_##inst
//[USBC_PORT_NEW(inst)] = &data_##inst,

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
		port->next_state = next_state;
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

static void pdc_cmd_send(struct pdc_port_t *port, enum pdc_cmd_t pdc_cmd)
{
	port->pdc_cmd = pdc_cmd;
	set_pdc_state(port, PDC_CMD_SEND);
}

/**
 * @brief Entering connector change event state
 */
static void pdc_connector_change_event_entry(void *o)
{
	struct pdc_port_t *port = (struct pdc_port_t *)o;

	port->retry_counter = 0;
	port->connector_local_state = CONNECTOR_WAIT_CHANGE;
}

/**
 * @brief Run connector change event state
 */
static void pdc_connector_change_event_run(void *o)
{
	struct pdc_port_t *port = (struct pdc_port_t *)o;

	switch (port->connector_local_state) {
	case CONNECTOR_WAIT_CHANGE:
		/* Wait for a CCI event */
		if (!atomic_test_and_clear_bit(port->cci_flags, CCI_EVENT)) {
			/* no CCI event */
			break;
		}
		/* CCI event detected */
		/* Reset retry counter */
		port->retry_counter = 0;
		/* Transition to CONNECTOR_GET_STATUS local state */
		port->connector_local_state = CONNECTOR_GET_STATUS;
		break;
	case CONNECTOR_GET_STATUS:
		/* Clear any left over flags */
		atomic_clear(port->cci_flags);
		/* Get connector status */
		if (pdc_get_connector_status(port->pdc,
					     &port->connector_status)) {
			/* Failed to get the connector status, so increment
			 * retry counter */
			port->retry_counter++;
			if (port->retry_counter > RETRY_MAX) {
				/* Retry counter exceeded: TODO handle error */
				port->connector_local_state =
					CONNECTOR_WAIT_CHANGE;
			}
			break;
		}
		/* Reset retry counter */
		port->retry_counter = 0;
		/* Transition to CONNECTOR_EVAL_STATUS local state */
		port->connector_local_state = CONNECTOR_EVAL_STATUS;
		break;
	case CONNECTOR_EVAL_STATUS:
		/* Wait until Get_Connector_Status UCSI command completes */
		if (!atomic_test_and_clear_bit(port->cci_flags,
					       CCI_CMD_COMPLETED)) {
			/* Command has not completed, so increment retry counter
			 */
			port->retry_counter++;
			if (port->retry_counter > RETRY_MAX) {
				/* Retry counter exceeded: TODO handle error */
				port->connector_local_state =
					CONNECTOR_WAIT_CHANGE;
			}
			break;
		}
		/* Test if we're connected */
		if (port->connector_status.connect_status) {
			if (port->connector_status.power_direction) {
				/* Port partner is a sink device */
				set_pdc_state(port, PDC_SRC_ATTACHED);
			} else {
				/* Port partner is a source device */
				set_pdc_state(port, PDC_SNK_ATTACHED);
			}
		} else {
			/* Not connected */
			set_pdc_state(port, PDC_UNATTACHED);
		}
		/* Transition to CONNECTOR_WAIT_CHANGE local state */
		port->connector_local_state = CONNECTOR_WAIT_CHANGE;
		break;
	}
}

/**
 * @brief Entering unattached state
 */
static void pdc_unattached_entry(void *o)
{
	struct pdc_port_t *port = (struct pdc_port_t *)o;

	print_current_pdc_state(port);

	port->ccaps_ready = false;

	if (port->last_state == PDC_SNK_ATTACHED) {
		/* Disable Sink Power Path */
		port->sink_path_en = false;
		pdc_cmd_send(port, CMD_PDC_SET_SINK_PATH);
	}
}

/**
 * @brief Run unattached state
 */
static void pdc_unattached_run(void *o)
{
	struct pdc_port_t *port = (struct pdc_port_t *)o;

	/* Enforce Unattached Policies */

	if (atomic_test_and_clear_bit(port->una_policy.flags,
				      UNA_POLICY_CC_MODE)) {
		/* Set CC PULL Resistor and TrySrc or TrySnk */
		pdc_cmd_send(port, CMD_PDC_SET_CCOM);
	} else if (atomic_test_and_clear_bit(port->una_policy.flags,
					     UNA_POLICY_TCC)) {
		/* Set RP current policy */
		pdc_cmd_send(port, CMD_PDC_SET_POWER_LEVEL);
	} else {
		/* Run public API call */
		run_public_api_command(port);
	}
}

/**
 * @brief Entering source attached state
 */
static void pdc_src_attached_entry(void *o)
{
	struct pdc_port_t *port = (struct pdc_port_t *)o;

	print_current_pdc_state(port);

	if (port->last_state == PDC_SNK_ATTACHED) {
		/* Disable Sink Power Path */
		port->sink_path_en = false;
		pdc_cmd_send(port, CMD_PDC_SET_SINK_PATH);
	}
}

/**
 * @brief Run source attached state
 */
static void pdc_src_attached_run(void *o)
{
	struct pdc_port_t *port = (struct pdc_port_t *)o;

	/* Run public API call */
	run_public_api_command(port);
}

/**
 * @brief Entering cmd send state
 */
static void pdc_cmd_send_entry(void *o)
{
	struct pdc_port_t *port = (struct pdc_port_t *)o;

	print_current_pdc_state(port);
	port->cmd_send_local_state = CMD_SEND_START;
	port->retry_counter = 0;
}

/**
 * @brief Run cmd send state
 */
static void pdc_cmd_send_run(void *o)
{
	struct pdc_port_t *port = (struct pdc_port_t *)o;
	int rv = 0;

	switch (port->cmd_send_local_state) {
	case CMD_SEND_START:
		/* Clear any left over flags */
		atomic_clear(port->cci_flags);

		/* Send PDC command via driver API */
		switch (port->pdc_cmd) {
		case CMD_PDC_RESET:
			rv = pdc_reset(port->pdc);
			break;
		case CMD_PDC_GET_INFO:
			rv = pdc_get_info(port->pdc, &port->info);
			break;
		case CMD_PDC_SET_POWER_LEVEL:
			rv = pdc_set_power_level(port->pdc,
						 port->una_policy.tcc);
			break;
		case CMD_PDC_SET_CCOM:
			rv = pdc_set_ccom(port->pdc, port->una_policy.cc_mode,
					  port->una_policy.drp_mode);
			break;
		case CMD_PDC_GET_PDOS:
			rv = pdc_get_pdos(port->pdc, SOURCE_PDO, PDO_OFFSET_0,
					  7, true, &port->snk_policy.pdos[0]);
			break;
		case CMD_PDC_GET_RDO:
			rv = pdc_get_rdo(port->pdc, &port->snk_policy.rdo);
			break;
		case CMD_PDC_SET_RDO:
			rv = pdc_set_rdo(port->pdc,
					 port->snk_policy.rdo_to_send);
			break;
		case CMD_PDC_GET_VBUS_VOLTAGE:
			rv = pdc_get_vbus_voltage(port->pdc, &port->vbus);
			break;
		case CMD_PDC_SET_SINK_PATH:
			rv = pdc_set_sink_path(port->pdc, port->sink_path_en);
			break;
		case CMD_PDC_READ_POWER_LEVEL:
			rv = pdc_read_power_level(port->pdc);
			break;
		case CMD_PDC_GET_CONNECTOR_CAPABILITY:
			rv = pdc_get_connector_capability(port->pdc,
							  &port->ccaps);
			break;
		case CMD_PDC_SET_UOR:
			rv = pdc_set_uor(port->pdc, port->uor);
			break;
		case CMD_PDC_SET_PDR:
			rv = pdc_set_pdr(port->pdc, port->pdr);
			break;
		default:
			break;
		}

		/* Test if command was successful. If not, try again until max
		 * retries is reached */
		if (rv) {
			/* Increment retry counter */
			port->retry_counter++;
			/* Test if max retries has been reached */
			if (port->retry_counter > RETRY_MAX) {
				/* Could not send command: TODO handle error */
				LOG_ERR("Command retry timout");
				set_pdc_state(port, port->last_state);
			}
			/* Failed */
			return;
		}

		/* Command was sent successfully */

		/* Reset retry counter */
		port->retry_counter = 0;
		/* Transition to next state */
		port->cmd_send_local_state = CMD_SEND_WAIT;
		break;
	case CMD_SEND_WAIT:
		/* Wait for command status notification from driver */

		/* If command was PDC_RESET, test the CCI_RESET_COMPLETED
		 * notification */
		if (port->pdc_cmd == CMD_PDC_RESET) {
			/* Test if reset has completed */
			if (atomic_test_and_clear_bit(port->cci_flags,
						      CCI_RESET_COMPLETED)) {
				set_pdc_state(port, port->last_state);
			}
		}
		/* Test if PDC is busy processing the command */
		else if (atomic_test_and_clear_bit(port->cci_flags, CCI_BUSY)) {
			/* Busy is treated the same as a retry */
			/* Increment retry counter */
			port->retry_counter++;
			/* Test if max retries has been reached */
			if (port->retry_counter == RETRY_MAX) {
				/* Could not send command: TODO handle error */
				set_pdc_state(port, port->last_state);
			}
		}
		/* Test if an error occurred trying to send the command */
		else if (atomic_test_and_clear_bit(port->cci_flags,
						   CCI_ERROR)) {
			/* Increment retry counter */
			port->retry_counter++;
			/* Test if max retries has been reached */
			if (port->retry_counter == RETRY_MAX) {
				/* Could not send command: TODO handle error */
				set_pdc_state(port, port->last_state);
			} else {
				/* Try to send the command again */
				port->cmd_send_local_state = CMD_SEND_START;
			}
		}
		/* Test if command was completed successfully */
		else if (atomic_test_and_clear_bit(port->cci_flags,
						   CCI_CMD_COMPLETED)) {
			set_pdc_state(port, port->last_state);
		}
		break;
	}
}

/**
 * @brief Exiting cmd send state
 */
static void pdc_cmd_send_exit(void *o)
{
	struct pdc_port_t *port = (struct pdc_port_t *)o;

	/* */
	switch (port->pdc_cmd) {
	case CMD_PDC_GET_PDOS:
		/* Filter out Augmented Power Data Objects (APDO) */
		/* TODO This is temparary until APDOs can be handled  */
		for (int i = 0; i < 7; i++) {
			if (port->snk_policy.pdos[i] & PDO_TYPE_AUGMENTED) {
				port->snk_policy.pdos[i] = 0;
			}
		}
		break;
	case CMD_PDC_GET_CONNECTOR_CAPABILITY:
		port->ccaps_ready = true;
		break;
	default:
	}

	/* Clear PDC command */
	port->pdc_cmd = 0;
	/* Clear PDC command flags */
	atomic_clear(port->pdc_cmd_flags);
}

/**
 * @brief Entering sink attached state
 */
static void pdc_snk_attached_entry(void *o)
{
	struct pdc_port_t *port = (struct pdc_port_t *)o;

	print_current_pdc_state(port);

	/* Don't set snk_attached_local_state on entry from cmd send state */
	if (port->last_state != PDC_CMD_SEND) {
		port->snk_attached_local_state =
			SNK_ATTACHED_GET_CONNECTOR_CAPABILITY;
	}
}

/**
 * @brief Run sink attached state.
 */
static void pdc_snk_attached_run(void *o)
{
	struct pdc_port_t *port = (struct pdc_port_t *)o;
	const struct pdc_config_t *const config = port->dev->config;
	uint32_t max_ma, max_mv, max_mw;

	switch (port->snk_attached_local_state) {
	case SNK_ATTACHED_GET_CONNECTOR_CAPABILITY:
		pdc_cmd_send(port, CMD_PDC_GET_CONNECTOR_CAPABILITY);
		/* Transition to the next state */
		port->snk_attached_local_state = SNK_ATTACHED_READ_POWER_LEVEL;
		break;
	case SNK_ATTACHED_READ_POWER_LEVEL:
		pdc_cmd_send(port, CMD_PDC_READ_POWER_LEVEL);
		/* Transition to the next state */
		port->snk_attached_local_state = SNK_ATTACHED_GET_PDOS;
		break;
	case SNK_ATTACHED_GET_PDOS:
		pdc_cmd_send(port, CMD_PDC_GET_PDOS);
		/* Transition to the next state */
		port->snk_attached_local_state = SNK_ATTACHED_GET_RDO;
		break;
	case SNK_ATTACHED_GET_RDO:
		pdc_cmd_send(port, CMD_PDC_GET_RDO);
		/* Transition to the next state */
		port->snk_attached_local_state = SNK_ATTACHED_SET_SINK_PATH;
		break;
	case SNK_ATTACHED_SET_SINK_PATH:
		port->sink_path_en = true;
		pdc_cmd_send(port, CMD_PDC_SET_SINK_PATH);
		/* Transition to the next state */
		port->snk_attached_local_state = SNK_ATTACHED_START_CHARGING;
		break;
	case SNK_ATTACHED_START_CHARGING:
		for (int i = 0; i < 7; i++) {
			printk("PDO%d: %08x, %d %d\n", i,
			       port->snk_policy.pdos[i],
			       PDO_FIXED_GET_VOLT(port->snk_policy.pdos[i]),
			       PDO_FIXED_GET_CURR(port->snk_policy.pdos[i]));
		}

		printk("RDO: %d\n", RDO_POS(port->snk_policy.rdo));
		port->snk_policy.pdo =
			port->snk_policy.pdos[RDO_POS(port->snk_policy.rdo) - 1];

		/* Extract Current, Voltage, and calculate Power */
		max_ma = PDO_FIXED_GET_CURR(port->snk_policy.pdo);
		max_mv = PDO_FIXED_GET_VOLT(port->snk_policy.pdo);
		max_mw = max_ma * max_mv / 1000;

		LOG_INF("Charging ON PORT%d\n", config->connector_num);
		printk("PDO: %08x\n", port->snk_policy.pdo);
		printk("V: %d\n", max_mv);
		printk("C: %d\n", max_ma);
		printk("P: %d\n", max_mw);

		pd_set_input_current_limit(config->connector_num, max_ma,
					   max_mv);
		charge_manager_set_ceil(config->connector_num,
					CEIL_REQUESTOR_PD, max_ma);

		if (((PDO_GET_TYPE(port->snk_policy.pdo) == 0) &&
		     (!PDO_FIXED_GET_DRP(port->snk_policy.pdo) ||
		      PDO_FIXED_GET_UNCONSTRAINED_PWR(port->snk_policy.pdo))) ||
		    (max_mw >= PD_DRP_CHARGE_POWER_MIN)) {
			charge_manager_update_dualrole(config->connector_num,
						       CAP_DEDICATED);
		} else {
			charge_manager_update_dualrole(config->connector_num,
						       CAP_DUALROLE);
		}

		/* Transition to next state */
		port->snk_attached_local_state = SNK_ATTACHED_RUN;
		break;
	case SNK_ATTACHED_RUN:
		/* Enforce Sink Policies */
		/*
		SNK_POLICY_REQUEST_LOW_POWER_PDO,
		SNK_POLICY_REQUEST_HIGH_POWER_PDO,
		SNK_POLICY_PDO_REQUEST_FROM_CHARGER
		*/

		/* Run public API call */
		run_public_api_command(port);
		break;
	}
}

/**
 * @brief Exiting from sink attached state.
 */
static void pdc_snk_attached_exit(void *o)
{
	struct pdc_port_t *port = (struct pdc_port_t *)o;
	const struct pdc_config_t *const config = port->dev->config;

	/* Don't stop charging if exiting to send a command */
	if (port->next_state != PDC_CMD_SEND) {
		/* Set input current limit to zero */
		pd_set_input_current_limit(config->connector_num, 0, 0);
		/* Set charge ceil to none */
		charge_manager_set_ceil(config->connector_num,
					CEIL_REQUESTOR_PD, CHARGE_CEIL_NONE);
		charge_manager_update_dualrole(config->connector_num,
					       CAP_UNKNOWN);

		/* Invalidate PDO */
		port->snk_policy.pdo = 0;
	}
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
	[PDC_CMD_SEND] = SMF_CREATE_STATE(pdc_cmd_send_entry, pdc_cmd_send_run,
					  pdc_cmd_send_exit, NULL),
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

	/* Set cci call back */
	pdc_set_handler_cb(port->pdc, pdc_cci_handler_cb, (void *)port);
	/* Initialize the connection state machine */
	smf_set_initial(&port->ctx, &pdc_states[PDC_UNATTACHED]);
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
	/* Block calling thread until command is processed or timeout occurs. */
	while (atomic_test_bit(pdc_data[port]->port.pdc_cmd_flags, pdc_cmd)) {
		/* block until command completes or max block count is reached
		 */

		/* give time for command to be processed */
		k_sleep(K_MSEC(200));
		/* increment block counter */
		pdc_data[port]->port.block_counter++;
		/* test if max block count has been reached */
		if (pdc_data[port]->port.block_counter > BLOCK_COUNTER_MAX) {
			/* something went wrong */
			LOG_ERR("Public API blocking timeout");
			return 1;
		}
	}

	return 0;
}

/**
 * PDC Power Management Public API
 */

bool pm_is_connected(int port)
{
	if (port > CONFIG_USB_PD_PORT_MAX_COUNT) {
		return false;
	}

	if (get_pdc_state(&pdc_data[port]->port) == PDC_UNATTACHED) {
		return false;
	}

	return true;
}

uint8_t pm_get_usb_pd_port_count(void)
{
	return CONFIG_USB_PD_PORT_MAX_COUNT;
}

int pm_set_active_charge_port(int charge_port)
{
	/* TODO */
	return EC_SUCCESS;
}

void pm_set_new_power_request(int port)
{
	/* TODO */
}

uint8_t pm_get_task_state(int port)
{
	/* TODO */
	return 0;
}

int pm_comm_is_enabled(int port)
{
	/* TODO */
	return 1;
}

bool pm_get_vconn_state(int port)
{
	/* TODO */
	return true;
}

bool pm_get_partner_usb_comm_capable(int port)
{
	/* TODO */
	return false;
}

bool pm_get_partner_unconstr_power(int port)
{
	/* TODO */
	return false;
}

int pm_accept_data_swap(int port, bool val)
{
	/* Make sure port is connected */
	if (!pm_is_connected(port)) {
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

int pm_accept_power_swap(int port, bool val)
{
	/* Make sure port is connected */
	if (!pm_is_connected(port)) {
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

static int pm_request_data_swap(int port, enum pd_data_role role)
{
	/* Make sure port is connected */
	if (!pm_is_connected(port)) {
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

void pm_request_data_swap_to_ufp(int port)
{
	pm_request_data_swap(port, PD_ROLE_UFP);
}

void pm_request_data_swap_to_dfp(int port)
{
	pm_request_data_swap(port, PD_ROLE_DFP);
}

static int pm_request_power_swap(int port, enum pd_power_role role)
{
	/* Make sure port is connected */
	if (!pm_is_connected(port)) {
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

void pm_request_swap_to_src(int port)
{
	pm_request_power_swap(port, PD_ROLE_SOURCE);
}

void pm_request_swap_to_snk(int port)
{
	pm_request_power_swap(port, PD_ROLE_SINK);
}

enum tcpc_cc_polarity pm_pd_get_polarity(int port)
{
	/* Make sure port is connected */
	if (!pm_is_connected(port)) {
		return PD_ROLE_SINK;
	}

	if (pdc_data[port]->port.connector_status.orientation) {
		return POLARITY_CC2;
	}

	return POLARITY_CC1;
}

enum pd_data_role pm_pd_get_data_role(int port)
{
	/* Make sure port is connected */
	if (!pm_is_connected(port)) {
		return PD_ROLE_SINK;
	}

	if (pdc_data[port]->port.connector_status.conn_partner_type ==
	    DFP_ATTACHED) {
		return EC_PD_DATA_ROLE_UFP;
	}

	return EC_PD_DATA_ROLE_DFP;
}

enum pd_power_role pm_get_power_role(int port)
{
	/* Make sure port is connected */
	if (!pm_is_connected(port)) {
		return PD_ROLE_SINK;
	}

	if (pdc_data[port]->port.connector_status.power_direction) {
		return PD_ROLE_SOURCE;
	}

	return PD_ROLE_SINK;
}

enum pd_cc_states pm_get_task_cc_state(int port)
{
	/* Make sure port is connected */
	if (!pm_is_connected(port)) {
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

bool pm_pd_capable(int port)
{
	/* Make sure port is connected */
	if (!pm_is_connected(port)) {
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

bool pm_get_partner_dual_role_power(int port)
{
	/* Make sure port is connected */
	if (!pm_is_connected(port)) {
		return false;
	}

	/*
	 * The subsystem is in an attached state, so wait until the
	 * connector capabilities are read.
	 */
	while (!pdc_data[port]->port.ccaps_ready) {
		k_sleep(K_MSEC(200));
	}

	/* Return DRP capability */
	return pdc_data[port]->port.ccaps.op_mode_drp;
}

bool pm_get_partner_data_swap_capable(int port)
{
	/* Make sure port is connected */
	if (!pm_is_connected(port)) {
		return false;
	}

	/*
	 * The subsystem is in an attached state, so wait until the
	 * connector capabilities are read.
	 */
	while (!pdc_data[port]->port.ccaps_ready) {
		k_sleep(K_MSEC(200));
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

uint32_t pm_get_vbus_voltage(int port)
{
	/* Make sure port is connected */
	if (!pm_is_connected(port)) {
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

void pm_reset(int port)
{
	/* Make sure port is connected */
	if (!pm_is_connected(port)) {
		return;
	}

	/* Reset block counter */
	pdc_data[port]->port.block_counter = 0;

	/* Set flag to send the command */
	atomic_set_bit(pdc_data[port]->port.pdc_cmd_flags, CMD_PDC_RESET);

	/* Block until command completes */
	public_api_block(port, CMD_PDC_RESET);
}
