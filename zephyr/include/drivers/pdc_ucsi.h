/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @file
 * @brief Public APIs for the PD Intel Alternate Mode drivers.
 *
 * Elaborate details can be found in respective SoC's "Platform Power
 * Delivery Controller Interface for SoC and Retimer" document.
 */
#ifndef ZEPHYR_INCLUDE_USBC_PDC_UCSI_H_
#define ZEPHYR_INCLUDE_USBC_PDC_UCSI_H_

#include <zephyr/device.h>

#ifdef __cplusplus
extern "C" {
#endif

enum ack_cc_ci_t {
	CONNECTOR_CHANGE_ACK = 1,
	COMMAND_COMPLETED_ACK = 2,
};

enum pdo_type_t {
	FIXED		= 0,
	BATTERY		= 1,
	VARIABLE	= 2
};

union pdo_source_t {
	struct {
		uint32_t max_current            : 10;
		uint32_t voltage                : 10;
		uint32_t peak_current           : 2;
		uint32_t reserved               : 3;
		uint32_t drd                    : 1;
		uint32_t usb_coms               : 1;
		uint32_t unconstrained_pwr      : 1;
		uint32_t suspend                : 1;
		uint32_t drp                    : 1;
		uint32_t type                   : 2;
	};
#if 0
	struct {
		uint32_t type			: 2;
		uint32_t drp			: 1;
		uint32_t suspend		: 1;
		uint32_t unconstrained_pwr	: 1;
		uint32_t usb_coms		: 1;
		uint32_t drd			: 1;
		uint32_t reserved		: 3;
		uint32_t peak_current		: 2;
		uint32_t voltage		: 10;
		uint32_t max_current		: 10;
	};
#endif
	uint32_t raw_value;
};

enum port_t {
	PORT0,
	PORT1
};

enum pdo_offset_t {
	PDO_OFFSET_0,
	PDO_OFFSET_1,
	PDO_OFFSET_2,
	PDO_OFFSET_3,
	PDO_OFFSET_4,
	PDO_OFFSET_5,
	PDO_OFFSET_6,
	PDO_OFFSET_7,
};

enum power_role_t {
	SINK,
	SOURCE
};

enum source_caps_t {
	CURRENT_SUPP_SOURCE_CAPS,
	ADVERTISED_CAPS,
	MAX_SUPP_SOURCE_CAPS
};

enum port_reset_t {
	HARD_RESET,
	DATA_RESET
};

union bmOptionalFeatures_t {
	struct {
		uint16_t set_ccom		: 1;
		uint16_t set_power_level	: 1;
		uint16_t alt_mode_details	: 1;
		uint16_t alt_mode_override	: 1;
		uint16_t pdo_details		: 1;
		uint16_t cable_details		: 1;
		uint16_t external_supply_notify : 1;
		uint16_t pd_reset_notify	: 1;
		uint16_t get_pd_message		: 1;
		uint16_t get_attention_vdo	: 1;
		uint16_t fw_update_request	: 1;
		uint16_t negotiated_power_level_change : 1;
		uint16_t security_request	: 1;
		uint16_t set_retimer_mode	: 1;
		uint16_t reserved		: 2;
	};
	uint16_t raw_value;
};

union bmAttributes_t {
	struct {
		uint32_t disabled_state_supported	: 1;
		uint32_t battery_charging		: 1;
		uint32_t usb_power_delivery		: 1;
		uint32_t reserved0			: 3;
		uint32_t usb_typec_current		: 1;
		uint32_t reserved1			: 1;
		uint32_t ac_supply			: 1;
		uint32_t reserved2			: 1;
		uint32_t other				: 1;
		uint32_t reserved3			: 3;
		uint32_t uses_vbus			: 1;
		uint32_t reserved4			: 1;
		uint32_t reserved5			: 16;
	};
	uint32_t raw_value;
};

struct device_capability_t {
	union bmAttributes_t bmAttributes;
	uint8_t bNumPorts;
	union bmOptionalFeatures_t bmOptionalFeatures;
	uint8_t reserved0;
	uint8_t bNumAltModes;
	uint8_t reserved1;
	uint16_t bcdBCVersion;
	uint16_t bcdPDVersion;
	uint16_t bcdUSBTypeCVersion;
} __packed;

union port_capability_t {
	struct {	
		uint32_t rp_only		: 1;
		uint32_t rd_only		: 1;
		uint32_t drp			: 1;
		uint32_t analog_audio_acc_mode	: 1;
		uint32_t debug_acc_mode		: 1;
		uint32_t usb2			: 1;
		uint32_t usb3			: 1;
		uint32_t alternate_mode		: 1;
		uint32_t provider		: 1;
		uint32_t consumer		: 1;
		uint32_t swap_to_dfp		: 1;
		uint32_t swap_to_ufp		: 1;
		uint32_t swap_to_src		: 1;
		uint32_t swap_to_snk		: 1;
		uint32_t usb4_v1		: 1;
		uint32_t reserved0		: 7;
		uint32_t fw_update		: 1;
		uint32_t security		: 1;
		uint32_t reserved1		: 2;
		uint32_t reverse_current_prot	: 1;
		uint32_t reserved2		: 5;
	};
	uint32_t raw_value;
};

union notification_enable_t {
	struct {
		uint16_t command_completed                      : 1;
		uint16_t external_supply_change                 : 1;
		uint16_t power_operation_mode_change            : 1;
		uint16_t reserved0                              : 1;
		uint16_t reserved1                              : 1;
		uint16_t provider_capability_change_supported   : 1;
		uint16_t negotiated_power_level_change          : 1;
		uint16_t pd_reset_complete                      : 1;
		uint16_t support_cam_change                     : 1;
		uint16_t battery_charging_status_change         : 1;
		uint16_t reserved2                              : 1;
		uint16_t connector_partner_change               : 1;
		uint16_t power_direction_change                 : 1;
		uint16_t reserved3                              : 1;
		uint16_t connect_change                         : 1;
		uint16_t error                                  : 1;
	};
	uint16_t raw_value;
};

union cc_operation_mode_t {
	struct {
		uint8_t rp_only	: 1;
		uint8_t rd_only : 1;
		uint8_t drp	: 1;
		uint8_t rsvd	: 5;
	};
	uint8_t raw_value;
};

union uor_t {
	struct {
		uint8_t swap_to_dfp     : 1;
		uint8_t swap_to_ufp     : 1;
		uint8_t accept_dr_swap  : 1;
		uint8_t reserved        : 5;
	};
	int8_t raw_value;
};

union pdr_t {
	struct {
		uint8_t swap_to_src     : 1;
		uint8_t swap_to_snk     : 1;
		uint8_t accept_pr_swap  : 1;
		uint8_t reserved        : 5;
	};
	uint8_t raw_value;
};

union cci_event_t {
	struct {
		uint32_t end_of_message		: 1;
		uint32_t connector_change	: 7;
		uint32_t data_len		: 8;
		uint32_t reserved0		: 7;
		uint32_t security_request	: 1;
		uint32_t fw_update_request	: 1;
		uint32_t not_supported		: 1;
		uint32_t cancel_completed	: 1;
		uint32_t reset_completed	: 1;
		uint32_t busy			: 1;
		uint32_t acknowledge_command	: 1;
		uint32_t error			: 1;
		uint32_t command_completed	: 1;
	};
	uint32_t raw_value;
};

struct pdo_t {
	uint32_t pdo0;
	uint32_t pdo1;
	uint32_t pdo2;
	uint32_t pdo3;
};

union conn_status_change_t {
	struct {
		uint16_t reserved0		: 1;
		uint16_t ext_supply		: 1;
		uint16_t pwr_operation_mode	: 1;
		uint16_t reserved1		: 1;
		uint16_t reserved2		: 1;
		uint16_t supported_provider_caps : 1;
		uint16_t negotiated_power_level	: 1;
		uint16_t pd_reset_complete	: 1;
		uint16_t supported_cam		: 1;
		uint16_t battery_charging_status : 1;
		uint16_t reserved3		: 1;
		uint16_t connector_partner	: 1;
		uint16_t pwr_direction		: 1;
		uint16_t reserved4		: 1;
		uint16_t connect_change		: 1;
		uint16_t error			: 1;
	};
	uint16_t raw_value;
};

union general_status_t {
	struct {
		uint16_t power_operation_mode	: 3;
		uint16_t connect_status		: 1;
		uint16_t power_direction	: 1;
		uint16_t conn_partner_flags	: 8;
		uint16_t conn_partner_type	: 3;
	};
	uint16_t raw_value;
};

union extra_status_t {
	struct {
		uint32_t battery_charging_caps	: 2;
		uint32_t provider_caps_limited	: 4;
		uint32_t bcd_pd_version		: 16;
		uint32_t reserved		: 10;
	};
	uint32_t raw_value;
};

struct connector_status_t {
	union conn_status_change_t conn_status_change;
	union general_status_t general_status;
	uint32_t rdo;
	union extra_status_t extra_status;
	uint32_t reserved;
} __packed;

struct error_status_bits_t {
	uint16_t unrecognized_command		: 1;
	uint16_t non_existent_connector_number	: 1;
	uint16_t invalid_command_specific_param	: 1;
	uint16_t incompatible_connector_partner	: 1;
	uint16_t cc_communication_error		: 1;
	uint16_t cmd_unsuccessful_dead_batt	: 1;
	uint16_t contract_negotiation_failed	: 1;
	uint16_t overcurrent			: 1;
	uint16_t undefined			: 1;
	uint16_t port_partner_rejected_swap	: 1;
	uint16_t hard_reset			: 1;
	uint16_t ppm_policy_conflict		: 1;
	uint16_t swap_rejected			: 1;
	uint16_t reserved			: 3;
};

struct error_status_t {
	struct error_status_bits_t esb;
	uint8_t vendor_defined[14];
}__packed;


union hpi_pd_status_t {
        struct {
                uint32_t default_data_role              : 2;
                uint32_t default_data_role_when_drd     : 1;
                uint32_t default_power_role             : 2;
                uint32_t default_power_role_when_drp    : 1;
                uint32_t data_role                      : 1;
                uint32_t reserved0                      : 1;
                uint32_t power_role                     : 1;
                uint32_t reserved1                      : 1;
                uint32_t pd_contract                    : 1;
                uint32_t emca_present                   : 1;
                uint32_t vconn_supplier                 : 1;
                uint32_t vconn_status                   : 1;
                uint32_t rp_status                      : 1;
                uint32_t pe_rdy_state                   : 1;
                uint32_t pd_spec_revision               : 2;
                uint32_t pp_pd3_capable                 : 1;
                uint32_t pp_unchunked_ext_msg_supported : 1;
                uint32_t emca_pd_spec_revision          : 2;
                uint32_t cable_type                     : 1;
                uint32_t epr_status                     : 1;
                uint32_t epr_sink_supported             : 1;
                uint32_t epr_source_supported           : 1;
                uint32_t reserved2                      : 7;
        };
        uint32_t raw_value;
};

union hpi_tc_status_t {
        struct {
                uint8_t is_connected    : 1;
                uint8_t cc_polarity     : 1;
                uint8_t attached_state  : 3;
                uint8_t ra              : 1;
                uint8_t rp              : 2;
        };
        uint8_t raw_value;
};



/**
 * @brief Callback for PD Alternate mode event
 */
typedef int (*pdc_reset_t)(const struct device *dev);
typedef int (*pdc_cancel_t)(const struct device *dev);
typedef int (*pdc_port_reset_t)(const struct device *dev, enum port_t port, enum port_reset_t type);
typedef int (*pdc_set_notification_enable_t)(const struct device *dev, union notification_enable_t bits);
typedef int (*pdc_get_capability_t)(const struct device *dev, struct device_capability_t *caps);
typedef int (*pdc_get_port_capability_t)(const struct device *dev, enum port_t port, union port_capability_t *caps);
typedef int (*pdc_set_ccom_t)(const struct device *dev, uint8_t port, union cc_operation_mode_t ccom);
typedef int (*pdc_set_uor_t)(const struct device *dev, uint8_t port, union uor_t uor);
typedef int (*pdc_set_pdr_t)(const struct device *dev, uint8_t port, union pdr_t pdr);
typedef int (*pdc_set_sink_path_t)(const struct device *dev, uint8_t port, bool en);
typedef int (*pdc_get_pdos_t)(const struct device *dev, enum port_t port,
                             bool partner_pdo, enum pdo_offset_t offset,
                             uint8_t num, enum power_role_t prole,
                             enum source_caps_t sc, struct pdo_t *pdos);
typedef int (*pdc_get_connector_status_t)(const struct device *dev, enum port_t port, struct connector_status_t *cs);
typedef int (*pdc_get_error_status_t)(const struct device *dev, enum port_t port, struct error_status_t *es);

typedef void (*pdc_cci_handler_cb_t)(enum port_t port, union cci_event_t cci_event);
typedef void (*pdc_port_handler_cb_t)(enum port_t port);
typedef int (*pdc_set_handler_cb_t)(const struct device *dev, pdc_cci_handler_cb_t cci_cb, pdc_port_handler_cb_t port_cb);
typedef int (*pdc_read_vbus_t)(const struct device *dev, enum port_t port, uint16_t *vbus);

typedef int (*pdc_get_tc_status_t)(const struct device *dev, enum port_t port, uint8_t *status);
typedef int (*pdc_get_pd_status_t)(const struct device *dev, enum port_t port, uint32_t *status);
typedef int (*pdc_get_current_pdo_t)(const struct device *dev, enum port_t port, uint32_t *pdo);
/**
 * @cond INTERNAL_HIDDEN
 *
 * These are for internal use only, so skip these in public documentation.
 */
__subsystem struct pdc_power_driver_api_t {
	pdc_reset_t reset;
	pdc_cancel_t cancel;
	pdc_port_reset_t port_reset;
	pdc_set_notification_enable_t set_notification_enable;
	pdc_get_capability_t get_capability;
	pdc_get_port_capability_t get_port_capability;
	pdc_set_ccom_t set_ccom;
	pdc_set_uor_t set_uor;
	pdc_set_pdr_t set_pdr;
	pdc_get_pdos_t get_pdos;
	pdc_set_sink_path_t set_sink_path;
	pdc_get_connector_status_t get_connector_status;
	pdc_get_error_status_t get_error_status;

	pdc_set_handler_cb_t set_handler_cb;
	pdc_read_vbus_t read_vbus;
	pdc_get_tc_status_t get_tc_status;
	pdc_get_pd_status_t get_pd_status;
	pdc_get_current_pdo_t get_current_pdo;
};
/**
 * @endcond
 */

/**
 * @brief Read from PD alternate mode status register
 *
 * @param dev Pointer to device structure of Intel Altmode driver instance.
 * @param data Pointer to Data Status register data.
 *
 * @retval 0 if success or I2C error.
 * @retval -EIO general input/output error.
 */
__syscall int pdc_reset(const struct device *dev);
static inline int z_impl_pdc_reset(const struct device *dev)
{
	const struct pdc_power_driver_api_t *api =
		(const struct pdc_power_driver_api_t *)dev->api;

	return api->reset(dev);
}

__syscall int pdc_port_reset(const struct device *dev, uint8_t port, enum port_reset_t type);
static inline int z_impl_pdc_port_reset(const struct device *dev, uint8_t port, enum port_reset_t type)
{
	const struct pdc_power_driver_api_t *api =
		(const struct pdc_power_driver_api_t *)dev->api;

	return api->port_reset(dev, port, type);
}

__syscall int pdc_set_sink_path(const struct device *dev, enum port_t port, bool en);
static inline int z_impl_pdc_set_sink_path(const struct device *dev, enum port_t port, bool en)
{
	const struct pdc_power_driver_api_t *api =
		(const struct pdc_power_driver_api_t *)dev->api;

	return api->set_sink_path(dev, port, en);
}

__syscall int pdc_set_notification_enable(const struct device *dev, union notification_enable_t bits);
static inline int z_impl_pdc_set_notification_enable(const struct device *dev, union notification_enable_t bits)
{
        const struct pdc_power_driver_api_t *api =
                (const struct pdc_power_driver_api_t *)dev->api;

        return api->set_notification_enable(dev, bits);
}

__syscall int pdc_get_capability(const struct device *dev, struct device_capability_t *caps);
static inline int z_impl_pdc_get_capability(const struct device *dev, struct device_capability_t *caps)
{
        const struct pdc_power_driver_api_t *api =
                (const struct pdc_power_driver_api_t *)dev->api;

        return api->get_capability(dev, caps);
}

__syscall int pdc_get_connector_status(const struct device *dev, enum port_t port, struct connector_status_t *cs);
static inline int z_impl_pdc_get_connector_status(const struct device *dev, enum port_t port, struct connector_status_t *cs)
{
	const struct pdc_power_driver_api_t *api =
		(const struct pdc_power_driver_api_t *)dev->api;

	return api->get_connector_status(dev, port, cs);
}

__syscall int pdc_get_error_status(const struct device *dev, enum port_t port, struct error_status_t *es);
static inline int z_impl_pdc_get_error_status(const struct device *dev, enum port_t port, struct error_status_t *es)
{
	const struct pdc_power_driver_api_t *api =
		(const struct pdc_power_driver_api_t *)dev->api;

	 return api->get_error_status(dev, port, es);
}

__syscall int pdc_get_pdos(const struct device *dev, enum port_t port,
                             bool partner_pdo, enum pdo_offset_t offset,
                             uint8_t num, enum power_role_t prole,
                             enum source_caps_t sc, struct pdo_t *pdos);
static inline int z_impl_pdc_get_pdos(const struct device *dev, enum port_t port,
                             bool partner_pdo, enum pdo_offset_t offset,
                             uint8_t num, enum power_role_t prole,
                             enum source_caps_t sc, struct pdo_t *pdos) {
	const struct pdc_power_driver_api_t *api =
		(const struct pdc_power_driver_api_t *)dev->api;

	return api->get_pdos(dev, port, partner_pdo, offset, num, prole, sc, pdos);
}

__syscall int pdc_get_port_capability(const struct device *dev, enum port_t port, union port_capability_t *caps);
static inline int z_impl_pdc_get_port_capability(const struct device *dev, enum port_t port, union port_capability_t *caps)
{
        const struct pdc_power_driver_api_t *api =
                (const struct pdc_power_driver_api_t *)dev->api;

        return api->get_port_capability(dev, port, caps);
}

__syscall int pdc_set_ccom(const struct device *dev, uint8_t port, union cc_operation_mode_t ccom);
static inline int z_impl_pdc_set_ccom(const struct device *dev, uint8_t port, union cc_operation_mode_t ccom)
{
	const struct pdc_power_driver_api_t *api =
		(const struct pdc_power_driver_api_t *)dev->api;

	return api->set_ccom(dev, port, ccom);
}

__syscall int pdc_set_uor(const struct device *dev, uint8_t port, union uor_t uor);
static inline int z_impl_pdc_set_uor(const struct device *dev, uint8_t port, union uor_t uor)
{
	const struct pdc_power_driver_api_t *api =
		(const struct pdc_power_driver_api_t *)dev->api;

	return api->set_uor(dev, port, uor);
}

__syscall int pdc_set_pdr(const struct device *dev, uint8_t port, union pdr_t pdr);
static inline int z_impl_pdc_set_pdr(const struct device *dev, uint8_t port, union pdr_t pdr)
{
	const struct pdc_power_driver_api_t *api =
		(const struct pdc_power_driver_api_t *)dev->api;

	return api->set_pdr(dev, port, pdr);
}

/**
 * @brief Register a callback for PD Alternate Mode event result
 *
 * @param dev Pointer to device structure of Intel Altmode driver instance.
 * @param cb Function pointer for the result callback.
 */
__syscall void pdc_set_handler_cb(const struct device *dev,
					pdc_cci_handler_cb_t cci_cb,
					pdc_port_handler_cb_t port_cb);

static inline void z_impl_pdc_set_handler_cb(const struct device *dev,
						   pdc_cci_handler_cb_t cci_cb,
						   pdc_port_handler_cb_t port_cb)
{
	const struct pdc_power_driver_api_t *api =
		(const struct pdc_power_driver_api_t *)dev->api;

	api->set_handler_cb(dev, cci_cb, port_cb);
}

__syscall int pdc_read_vbus(const struct device *dev, uint8_t port, uint16_t *vbus);
static inline int z_impl_pdc_read_vbus(const struct device *dev, uint8_t port, uint16_t *vbus)
{
	const struct pdc_power_driver_api_t *api =
		(const struct pdc_power_driver_api_t *)dev->api;

	if (!api->read_vbus) {
		return 0;
	}

	return api->read_vbus(dev, port, vbus);
}

__syscall int pdc_get_tc_status(const struct device *dev, enum port_t port, uint8_t *status);
static inline int z_impl_pdc_get_tc_status(const struct device *dev, enum port_t port, uint8_t *status)
{
	const struct pdc_power_driver_api_t *api =
		(const struct pdc_power_driver_api_t *)dev->api;

	return api->get_tc_status(dev, port, status);
}

__syscall int pdc_get_pd_status(const struct device *dev, enum port_t port, uint32_t *status);
static inline int z_impl_pdc_get_pd_status(const struct device *dev, enum port_t port, uint32_t *status)
{
	const struct pdc_power_driver_api_t *api =
		(const struct pdc_power_driver_api_t *)dev->api;

	return api->get_pd_status(dev, port, status);
}

__syscall int pdc_get_current_pdo(const struct device *dev, enum port_t port, uint32_t *pdo);
static inline int z_impl_pdc_get_current_pdo(const struct device *dev, enum port_t port, uint32_t *pdo)
{
	const struct pdc_power_driver_api_t *api =
		(const struct pdc_power_driver_api_t *)dev->api;

	return api->get_current_pdo(dev, port, pdo);
}

#ifdef __cplusplus
}
#endif

/**
 * @}
 */
#if 1
 #include <syscalls/pdc_ucsi.h>
#endif
#endif /* ZEPHYR_INCLUDE_USBC_PDC_UCSI_H_ */
