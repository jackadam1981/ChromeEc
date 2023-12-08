/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @file drivers/pdc.h
 * @brief Public APIs for Power Delivery Controller Chip drivers.
 *
 */
#ifndef ZEPHYR_INCLUDE_DRIVERS_PDC_H_
#define ZEPHYR_INCLUDE_DRIVERS_PDC_H_

#include <zephyr/types.h>
#include <zephyr/device.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

enum sop_t {
	TCPM,
	SOP,
	SOPP,
	SOPPP
};

enum pdo_type_t {
	SINK_PDO,
	SOURCE_PDO,
};

enum ack_cc_ci_t {
	CONNECTOR_CHANGE_ACK = 1,
	COMMAND_COMPLETED_ACK = 2,
};

enum power_role_t {
	SINK,
	SOURCE
};

union rdo_fixed_t {
	struct {
		uint32_t max_operating_current:	10;
		uint32_t operating_current:	10;
		uint32_t reserved:		2;
		uint32_t epr_mode_capable:	1;
		uint32_t unchunked_ext_msg:	1;
		uint32_t no_usb_suspend:	1;
		uint32_t usb_comms_capable:	1;
		uint32_t capability_mismatch:	1;
		uint32_t giveback:		1;
		uint32_t obj_position:		4;
	};
	uint32_t raw_value;
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
	uint32_t raw_value;
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

enum source_caps_t {
	CURRENT_SUPP_SOURCE_CAPS,
	ADVERTISED_CAPS,
	MAX_SUPP_SOURCE_CAPS
};

enum connector_reset_t {
	PD_SOFT_RESET = 0,
	PD_HARD_RESET = 1,
};

enum ccom_t {
	CCOM_RP,
	CCOM_RD,
	CCOM_DRP
};

enum drp_mode_t {
	DRP_NORMAL,
	DRP_TRY_SRC,
	DRP_TRY_SNK,
};

struct alt_mode_t {
	uint16_t svid;
	uint32_t mode;
} __packed;


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

union connector_capability_t {
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
		uint32_t reserved               : 22;
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

struct connector_status_t {
	union conn_status_change_t conn_status_change;
	uint8_t power_operation_mode;
	uint8_t connect_status;
	uint8_t power_direction;
	uint8_t conn_partner_flags;
	uint8_t conn_partner_type;
	uint32_t rdo;
	uint8_t battery_charging_cap;
	uint8_t provider_caps_limited;
	uint16_t bcd_pd_version;
	uint8_t orientation;
	uint8_t sink_path_status;
	uint8_t reverse_current_protection_status;
	uint8_t power_reading_ready;
	uint8_t current_scale;
	uint16_t peak_current;
	uint16_t average_current;
	uint8_t voltage_scale;
	uint16_t voltage_reading;
};

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

/**
 * @typedef
 * @brief
 */
typedef int (*pdc_enable_t)(const struct device *dev);
typedef int (*pdc_get_ucsi_version_t)(const struct device *dev, uint16_t *version);
typedef int (*pdc_reset_t)(const struct device *dev);
typedef int (*pdc_cancel_t)(const struct device *dev);
typedef int (*pdc_connector_reset_t)(const struct device *dev, enum connector_reset_t type);
typedef int (*pdc_set_notification_enable_t)(const struct device *dev, union notification_enable_t bits, uint16_t ext_bits);
typedef int (*pdc_get_capability_t)(const struct device *dev, struct device_capability_t *caps);
typedef int (*pdc_get_connector_capability_t)(const struct device *dev, union connector_capability_t *caps);
typedef int (*pdc_set_ccom_t)(const struct device *dev, enum ccom_t ccom, enum drp_mode_t dm);
typedef int (*pdc_set_uor_t)(const struct device *dev, union uor_t uor);
typedef int (*pdc_set_pdr_t)(const struct device *dev, union pdr_t pdr);
typedef int (*pdc_set_sink_path_t)(const struct device *dev, bool en);
typedef int (*pdc_get_connector_status_t)(const struct device *dev, struct connector_status_t *cs);
typedef int (*pdc_get_error_status_t)(const struct device *dev, struct error_status_t *es);

typedef void (*pdc_cci_handler_cb_t)(union cci_event_t cci_event);
typedef int (*pdc_set_handler_cb_t)(const struct device *dev, pdc_cci_handler_cb_t cci_cb);
typedef int (*pdc_get_vbus_t)(const struct device *dev, uint16_t *vbus);

typedef int (*pdc_get_pdo_t)(const struct device *dev, enum pdo_type_t pdo_type, enum pdo_offset_t pdo_offset, uint8_t num_pdos,
                        bool port_partner_pdo, uint32_t *pdos);

typedef int (*pdc_get_rdo_t)(const struct device *dev, uint32_t *rdo);

typedef int (*pdc_get_alternate_mode_t)(const struct device *dev, enum sop_t sop, uint8_t alt_mode_offset, uint8_t num_alt_modes, struct alt_mode_t *alt_modes);

typedef int (*pdc_is_flash_code_t)(const struct device *dev, uint8_t *is_flash_code);
typedef int (*pdc_get_fw_version_t)(const struct device *dev, uint32_t *fw_version);
typedef int (*pdc_get_vid_pid_t)(const struct device *dev, uint32_t *vid_pid);
typedef int (*pdc_get_pd_version_t)(const struct device *dev, uint32_t *pd_version);

typedef int (*pdc_is_pd_ready_t)(const struct device *dev, uint8_t *result);
typedef int (*pdc_is_typec_connected_t)(const struct device *dev, uint8_t *result);

typedef int (*pdc_get_current_pdo_t)(const struct device *dev, uint32_t *pdo);

typedef int (*pdc_set_voltage_t)(const struct device *dev);

typedef int (*pdc_read_power_level_t)(const struct device *dev);

/**
 * @cond INTERNAL_HIDDEN
 *
 * These are for internal use only, so skip these in public documentation.
 */
__subsystem struct pdc_driver_api_t {
	pdc_enable_t enable;
	pdc_get_ucsi_version_t get_ucsi_version;
	pdc_reset_t reset;
	pdc_cancel_t cancel;
	pdc_connector_reset_t connector_reset;
	pdc_set_notification_enable_t set_notification_enable;
	pdc_get_capability_t get_capability;
	pdc_get_connector_capability_t get_connector_capability;
	pdc_set_ccom_t set_ccom;
	pdc_set_uor_t set_uor;
	pdc_set_pdr_t set_pdr;
	pdc_set_sink_path_t set_sink_path;
	pdc_get_connector_status_t get_connector_status;
	pdc_get_alternate_mode_t get_alternate_mode;
	pdc_get_error_status_t get_error_status;
	pdc_set_handler_cb_t set_handler_cb;
	pdc_get_vbus_t get_vbus_voltage;
	pdc_get_vbus_t get_vbus_current;
	pdc_get_current_pdo_t get_current_pdo;
	pdc_get_pdo_t get_pdo;
	pdc_get_rdo_t get_rdo;

	pdc_read_power_level_t read_power_level;

	pdc_is_pd_ready_t is_pd_ready;
        pdc_is_typec_connected_t is_typec_connected;

	pdc_is_flash_code_t is_flash_code;
	pdc_get_fw_version_t get_fw_version;
	pdc_get_vid_pid_t get_vid_pid;
	pdc_get_pd_version_t get_pd_version;

	pdc_set_voltage_t set_voltage;
};
/**
 * @endcond
 */

/**
 * @brief Enable the PDC. Usually the first function that needs to be called 
 *
 * @param dev PDC device structure pointer
 *
 * @retval 0 on API call success
 * @retval -EIO on failure
 */
static inline int pdc_enable(const struct device *dev)
{
	const struct pdc_driver_api_t *api = (const struct pdc_driver_api_t *)dev->api;

	return api->enable(dev);
}

static inline int pdc_read_power_level(const struct device *dev)
{
	const struct pdc_driver_api_t *api = (const struct pdc_driver_api_t *)dev->api;

	return api->read_power_level(dev);
}

/**
 * @brief Read the version of the UCSI supported by the PDC. The version is 
 *        read and cached during PDC initialization, so this is a non-blocking 
 *        call.
 *
 * @param dev PDC device structure pointer
 *
 * @retval PDC Version number in BCD as an uint16. Format is JJMN, where (JJ –  
 *         major version number, M – minor version number, N – sub-minor 
 *         version number)
 */
static inline int pdc_get_ucsi_version(const struct device *dev, uint16_t *version)
{
	const struct pdc_driver_api_t *api = (const struct pdc_driver_api_t *)dev->api;

	return api->get_ucsi_version(dev, version);
}

/**
 * @brief Resets the PDC
 * @note cci_event.reset_completed is set when the PDC reset is complete.
 *
 * @param dev PDC device structure pointer
 *
 * @retval 0 on API call success
 * @retval -EBUSY if not ready to execute the command
 */
static inline int pdc_reset(const struct device *dev)
{
	const struct pdc_driver_api_t *api =
		(const struct pdc_driver_api_t *)dev->api;

	return api->reset(dev);
}

/**
 * @brief Resets a PDC connector
 * @note cci_event.command_completed_indicator is set when the UCSI command
 *       complete.
 * @note cci_event.error_indicator is set if the UCSI command was  
 *       unsuccessfull.
 *
 * @param dev PDC device structure pointer
 *
 * @retval 0 on success
 * @retval -EBUSY if not ready to execute the command
 */
static inline int pdc_connector_reset(const struct device *dev, enum connector_reset_t type)
{
	const struct pdc_driver_api_t *api = (const struct pdc_driver_api_t *)dev->api;

	return api->connector_reset(dev, type);
}

/**
 * @brief Enable the Sink FET while in the Attached Sink State
 * @note cci_event.command_completed_indicator is set when the UCSI command   
 *       completes.
 * @note cci_event.error_indicator is set if the UCSI command was 
 *       unsuccessfull.
 *
 * @param dev PDC device structure pointer
 * @param en true to enable the Sink FET or false to disable it
 *
 * @retval 0 on success
 * @retval -EBUSY if not ready to execute the command
 */
static inline int pdc_set_sink_path(const struct device *dev, bool en)
{
	const struct pdc_driver_api_t *api = (const struct pdc_driver_api_t *)dev->api;

	return api->set_sink_path(dev, en);
}

/**
 * @brief Sets the list of asynchronous events that the PDC may send
 *        notifications about.
 * @note cci_event.command_completed_indicator is set when the UCSI command   
 *       completes.
 * @note cci_event.error_indicator is set if the UCSI command was unsuccessfull
 *
 * @param dev PDC device structure pointer
 * @param bits a bitmask of the notifications.
 *
 * @retval 0 on success
 * @retval -EBUSY if not ready to execute the command
 */
static inline int pdc_set_notification_enable(const struct device *dev, union notification_enable_t bits, uint16_t ext_bits)
{
	const struct pdc_driver_api_t *api = (const struct pdc_driver_api_t *)dev->api;

	return api->set_notification_enable(dev, bits, ext_bits);
}

/**
 * @brief Gets the PDC device capabilities.
 * @note cci_event.command_completed_indicator is set when the UCSI command   
 *       completes.
 * @note cci_event.error_indicator is set if the UCSI command was unsuccessfull
 *
 * @param dev PDC device structure pointer
 * @param caps pointer where the device capabilities are stored.
 *
 * @retval 0 on success
 * @retval -EBUSY if not ready to execute the command
 */
static inline int pdc_get_capability(const struct device *dev, struct device_capability_t *caps)
{
	const struct pdc_driver_api_t *api = (const struct pdc_driver_api_t *)dev->api;

	return api->get_capability(dev, caps);
}

/**
 * @brief Gets the PDC connector status.
 * @note cci_event.command_completed_indicator is set when the UCSI command   
 *       completes.
 * @note cci_event.error_indicator is set if the UCSI command was 
 *       unsuccessfull.
 *
 * @param dev PDC device structure pointer
 * @param cs pointer where the connector status is stored.
 *
 * @retval 0 on success
 * @retval -EBUSY if not ready to execute the command
 */
static inline int pdc_get_connector_status(const struct device *dev, struct connector_status_t *cs)
{
	const struct pdc_driver_api_t *api = (const struct pdc_driver_api_t *)dev->api;

	return api->get_connector_status(dev, cs);
}

/**
 * @brief Gets the details about an error, if the cci_event.error_indicator is 
 *        set.
 * @note cci_event.command_completed_indicator is set when the UCSI command   
 *       completes.
 *
 * @param dev PDC device structure pointer
 * @param es pointer where the error status is stored.
 *
 * @retval 0 on success
 * @retval -EBUSY if not ready to execute the command
 */
static inline int pdc_get_error_status(const struct device *dev, struct error_status_t *es)
{
	const struct pdc_driver_api_t *api = (const struct pdc_driver_api_t *)dev->api;

	return api->get_error_status(dev, es);
}

/**
 * @brief Gets capabilites of a connector 
 * @note cci_event.command_completed_indicator is set when the UCSI command   
 *       completes.
 * @note cci_event.error_indicator is set if the UCSI command was 
 *       unsuccessfull.
 *
 * @param dev PDC device structure pointer
 * @param caps pointer where the connector capabilites are stored.
 *
 * @retval 0 on success
 * @retval -EBUSY if not ready to execute the command
 */
static inline int pdc_get_connector_capability(const struct device *dev, union connector_capability_t *caps)
{
	const struct pdc_driver_api_t *api = (const struct pdc_driver_api_t *)dev->api;

	return api->get_connector_capability(dev, caps);
}

/**
 * @brief Sets the CC operation mode of the PDC 
 * @note cci_event.command_completed_indicator is set when the UCSI command   
 *       completes.
 * @note cci_event.error_indicator is set if the UCSI command was unsuccessfull
 *
 * @param dev PDC device structure pointer
 * @param ccom CC operation mode
 *
 * @retval 0 on success
 * @retval -EBUSY if not ready to execute the command
 */
static inline int pdc_set_ccom(const struct device *dev, enum ccom_t ccom, enum drp_mode_t dm)
{
	const struct pdc_driver_api_t *api = (const struct pdc_driver_api_t *)dev->api;

	return api->set_ccom(dev, ccom, dm);
}

/**
 * @brief Sets the USB operation role of the PDC 
 * @note cci_event.command_completed_indicator is set when the UCSI command   
 *       completes.
 * @note cci_event.error_indicator is set if the UCSI command was unsuccessfull
 *
 * @param dev PDC device structure pointer
 * @param uor USB operation role
 *
 * @retval 0 on success
 * @retval -EBUSY if not ready to execute the command
 */
static inline int pdc_set_uor(const struct device *dev, union uor_t uor)
{
	const struct pdc_driver_api_t *api = (const struct pdc_driver_api_t *)dev->api;

	return api->set_uor(dev, uor);
}

/**
 * @brief Sets the Power direction role of the PDC 
 * @note cci_event.command_completed_indicator is set when the UCSI command   
 *       completes.
 * @note cci_event.error_indicator is set if the UCSI command was unsuccessfull
 *
 * @param dev PDC device structure pointer
 * @param pdr Power direction role
 *
 * @retval 0 on success
 * @retval -EBUSY if not ready to execute the command
 */
static inline int pdc_set_pdr(const struct device *dev, union pdr_t pdr)
{
	const struct pdc_driver_api_t *api = (const struct pdc_driver_api_t *)dev->api;

	return api->set_pdr(dev, pdr);
}

/**
 * @brief 
 *
 * @param dev PDC device structure pointer
 * @param 
 *
 * @retval 0 on success
 * @retval -EBUSY if not ready to execute the command
 */
static inline int pdc_get_alternate_mode(const struct device *dev, enum sop_t sop, uint8_t alt_mode_offset, uint8_t num_alt_modes, struct alt_mode_t *alt_modes)
{
	const struct pdc_driver_api_t *api = (const struct pdc_driver_api_t *)dev->api;

	return api->get_alternate_mode(dev, sop, alt_mode_offset, num_alt_modes, alt_modes);
}

/**
 * @brief Sets the callback the driver uses to communicate events to the TCPM
 *
 * @param dev PDC device structure pointer
 * @param cci_cb pointer to callback
 */
static inline void pdc_set_handler_cb(const struct device *dev, pdc_cci_handler_cb_t cci_cb)
{
	const struct pdc_driver_api_t *api = (const struct pdc_driver_api_t *)dev->api;

	api->set_handler_cb(dev, cci_cb);
}

/**
 * @brief Reads the VBUS voltage
 *
 * @param dev PDC device structure pointer
 * @param voltage pointer to where the voltage is stored
 *
 * @retval 0 on success
 * @retval negative value on error
 */
static inline int pdc_getvbus_voltage(const struct device *dev, uint16_t *voltage)
{
	const struct pdc_driver_api_t *api = (const struct pdc_driver_api_t *)dev->api;

	if (!api->get_vbus_voltage) {
		return -1;
	}

	return api->get_vbus_voltage(dev, voltage);
}

/**
 * @brief Reads the VBUS current
 *
 * @param dev PDC device structure pointer
 * @param current pointer to where the voltage is stored
 *
 * @retval 0 on success
 * @retval negative value on error
 */
static inline int pdc_get_vbus_current(const struct device *dev, uint16_t *current)
{
	const struct pdc_driver_api_t *api = (const struct pdc_driver_api_t *)dev->api;

	if (!api->get_vbus_current) {
		return -1;
	}

	return api->get_vbus_voltage(dev, current);
}

/**
 * @brief Gets the Sink or Source PDOs associated with the connector, or its
 *        capabilites.
 * @note cci_event.command_completed_indicator is set when the UCSI command   
 *       completes.
 * @note cci_event.error_indicator is set if the UCSI command was 
 *       unsuccessfull.
 * @note cci_event.busy_indicator is set if the UCSI command can't be completed
 *       at this time, but will at a later time.
 *
 * @param dev PDC device structure pointer
 * @param partner_pdo true if requesting the PDOs from the attached device
 * @param offset starting offset of the first PDO to be returned. Valid values
 *               are 0 to 7.
 * @param num number of PDOs to return starting from the PDO offset. NOTE: the 
 *            number of PDOs returned is num + 1.
 * @param prole Source for source PDOs or Sink for sink PDOs.
 * @param sc request the Source or Sink Capabilites instead of the PDOs. This
 *           parameter is only valid when partner_pdo is false.
 * @param pdos pointer to where the PDOs or Capabilites are stored.
 * @param es pointer where the error status is stored.
 *
 * @retval 0 on success
 * @retval -EBUSY if not ready to execute the command
 */
static inline int pdc_get_pdo(const struct device *dev, enum pdo_type_t pdo_type, enum pdo_offset_t pdo_offset, uint8_t num_pdos,
                        bool port_partner_pdo, uint32_t *pdos)
{
	const struct pdc_driver_api_t *api = (const struct pdc_driver_api_t *)dev->api;

	return api->get_pdo(dev, pdo_type, pdo_offset, num_pdos, port_partner_pdo, pdos);
}

static inline int pdc_is_flash_code(const struct device *dev, uint8_t *is_flash_code)
{               
	const struct pdc_driver_api_t *api = (const struct pdc_driver_api_t *)dev->api;

	return api->is_flash_code(dev, is_flash_code);
}               

static inline int pdc_is_pd_ready(const struct device *dev, uint8_t *result)
{
	const struct pdc_driver_api_t *api = (const struct pdc_driver_api_t *)dev->api;

	return api->is_pd_ready(dev, result);
}

static inline int pdc_is_typec_connected(const struct device *dev, uint8_t *result)
{
	const struct pdc_driver_api_t *api = (const struct pdc_driver_api_t *)dev->api;

	return api->is_typec_connected(dev, result);
}

static inline int pdc_get_fw_version(const struct device *dev, uint32_t *fw_version)
{
	const struct pdc_driver_api_t *api = (const struct pdc_driver_api_t *)dev->api;

	return api->get_fw_version(dev, fw_version);
}

static inline int pdc_get_vid_pid(const struct device *dev, uint32_t *vid_pid)
{
	const struct pdc_driver_api_t *api = (const struct pdc_driver_api_t *)dev->api;

	return api->get_vid_pid(dev, vid_pid);
}

static inline int pdc_get_pd_version(const struct device *dev, uint32_t *pd_version)
{
	const struct pdc_driver_api_t *api = (const struct pdc_driver_api_t *)dev->api;

	return api->get_pd_version(dev, pd_version);
}

static inline int pdc_get_rdo(const struct device *dev, uint32_t *rdo)
{
	const struct pdc_driver_api_t *api = (const struct pdc_driver_api_t *)dev->api;

	return api->get_rdo(dev, rdo);
}

/**
 * @brief Get the currently selected PDO
 *
 * @param dev PDC device structure pointer
 * @param pdo pointer to where the PDO is stored
 *
 * @retval 0 on success
 * @retval negative value on error
 */
static inline int pdc_get_current_pdo(const struct device *dev, uint32_t *pdo)
{
	const struct pdc_driver_api_t *api = (const struct pdc_driver_api_t *)dev->api;

	return api->get_current_pdo(dev, pdo);
}

static inline int pdc_set_voltage(const struct device *dev)
{
	const struct pdc_driver_api_t *api = (const struct pdc_driver_api_t *)dev->api;

	return api->set_voltage(dev);
}

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* ZEPHYR_INCLUDE_DRIVERS_PDC_H_ */
