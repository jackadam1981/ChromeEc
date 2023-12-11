/* Copyright 2023 The ChromiumOS Authors
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief UCSI data structures and types used for USB-C PDC drivers
 *
 * The information in this file was taken from the UCSI
 * Revision 3.0
 */

/**
 * @file drivers/ucsi_v3.h
 * @brief UCSI Data Structures and Types.
 *
 */
#ifndef ZEPHYR_INCLUDE_DRIVERS_UCSI_V3_H_
#define ZEPHYR_INCLUDE_DRIVERS_UCSI_V3_H_

#include <zephyr/types.h>
#include <zephyr/device.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Mamimun number of data bytes the PDC can transfer or receive
 *        at a time
 */
#define PDC_MAX_DATA_LENGTH	256

/**
 * @brief CCI - USB Type-C Command Status and Connector Change Indication
 */
union cci_event_t {
	struct {
		/**
		 * Used in multi-chunk commands such as a FW update
		 * request (FW Update Request Indicator =1) or
		 * security request (Security Request Indicator =1).
		 * For all other commands, it is reserved and shall
		 * be set to 0.
		 */
		uint32_t end_of_message		: 1;
		/**
		 * Used to indicate the connector number that a change
		 * occurred on. Valid values are 0 to the maximum number
		 * of connectors supported on the platform.
		 * If this field is set to zero, then no change occurred
		 * on any of the connectors.
		 */
		uint32_t connector_change	: 7;
		/**
		 * Length of valid data in bytes. If this value is greater
		 * than zero, then the user's Data Structure contents are
		 * valid. The value in this register shall be less than or
		 * equal to PDC_MAX_DATA_LENGTH.
		 */
		uint32_t data_len		: 8;
		/**
		 * This bit is set when a custom defined message is ready.
		 * It is mutually exclusive with any other Indicator. On
		 * some commands, like the FW update, this bit is repurposed.
		 */
		uint32_t vendor_defined_indicator : 1,
		/** Reserved and shall be set to zero. */
		uint32_t reserved0		: 6;
		/**
		 * For a Security Request, set to 1 when the request comes
		 * from the Port Partner (Asynchronous message). Otherwise
		 * set to 0.
		 */
		uint32_t security_request	: 1;
		/**
		 * For an LPM FW Update Request, set to 1 when the request
		 * comes from the Port Partner (Asynchronous message).
		 * Otherwise set to 0.
		 */
		uint32_t fw_update_request	: 1;
		/**
		 * Indicate's that the PDC does not currently support a
		 * command. This field shall only be valid when the
		 * Command Completed Indicator field is set to one.
		 */
		uint32_t not_supported		: 1;
		/**
		 * Is set to one when a command has been canceled.
		 * This field shall only be valid when the Command Completed
		 * Indicator field is set to one.
		 */
		uint32_t cancel_completed	: 1;
		/**
		 * Is set when the PPM_RESET command is completed.
		 * If this field is set to one, then no other bits in this
		 * Data Structure shall be set.
		 */
		uint32_t reset_completed	: 1;
		/**
		 * Is set when the PDC or driver is busy. If this field is
		 * set to one, then no other bits in this Data Structure
		 * shall be set.
		 */
		uint32_t busy			: 1;
		/** Not used */
		uint32_t acknowledge_command	: 1;
		/**
		 * Is set when the PDC or driver encounters an error.
		 * This field shall only be valid when the Command Completed
		 * Indicator field is set to one.
		 */
		uint32_t error			: 1;
		/** Is set a command is completed. */
		uint32_t command_completed	: 1;
	};
	uint32_t raw_value;
};

/**
 * @brief Indicates the reason for a reported error
 */
union error_status_t {
	struct {
		/** Unrecognized command */
		uint32_t unrecognized_command		: 1;
		/** Non-existent connector number */
		uint32_t non_existent_connector_number	: 1;
		/** Invalid command specific parameters */
		uint32_t invalid_command_specific_param	: 1;
		/** Incompatible connector partner */
		uint32_t incompatible_connector_partner	: 1;
		/** CC communication error */
		uint32_t cc_communication_error		: 1;
		/** Command unsuccessful due to dead battery condition */
		uint32_t cmd_unsuccessful_dead_batt	: 1;
		/** Contract negotiation failure */
		uint32_t contract_negotiation_failed	: 1;
		/** Overcurrent */
		uint32_t overcurrent			: 1;
		/** Undefined */
		uint32_t undefined			: 1;
		/** Port partner rejected swap */
		uint32_t port_partner_rejected_swap	: 1;
		/** Hard Reset */
		uint32_t hard_reset			: 1;
		/** PPM Policy Conflict */
		uint32_t ppm_policy_conflict		: 1;
		/** Swap Rejected */
		uint32_t swap_rejected			: 1;
		/** Reverse Current Protection */
		uint32_t reverse_current_protection	: 1;
		/** Set Sink Path Rejected */
		uint32_t set_sink_path_rejected		: 1;
		/** Reserved and shall be set to zero */
		uint32_t reserved0			: 1;

		/** Vendor Specific Bits follow */

		/** Ping Retry Count exceeded */
		uint32_t ping_retry_count		: 1;
		/** I2C Read Error */
		uint32_t i2c_read_error			: 1;
		/** I2c Write Error */
		uint32_t i2c_write_error		: 1;
	};
	uint32_t raw_value;
};

/**
 * @brief PDC Notifications that trigger an IRQ
 */
union notification_enable_t {
	struct {
		/** Command Completed */
		uint32_t command_completed                      : 1;
		/** (Optional) External Supply Change */
		uint32_t external_supply_change                 : 1;
		/** Power Operation Mode Change */
		uint32_t power_operation_mode_change            : 1;
		/** (Optional) Attention */
		uint32_t attention                              : 1;
		/** (Optional) FW Update Request */
		uint32_t fw_update_request                      : 1;
		/** (Optional) Provider Capabilities Change */
		uint32_t provider_capability_change_supported   : 1;
		/** (Optional) Negotiated Power Level Change */
		uint32_t negotiated_power_level_change          : 1;
		/** (Optional) PD Reset Complete */
		uint32_t pd_reset_complete                      : 1;
		/** (Optional) Supported CAM Change */
		uint32_t support_cam_change                     : 1;
		/** Battery Charging Status Change */
		uint32_t battery_charging_status_change         : 1;
		/** (Optional) Security Request from Port Partner */
		uint32_t security_request_from_port_partner     : 1;
		/** Connector partner Change */
		uint32_t connector_partner_change               : 1;
		/** Power Direction Change */
		uint32_t power_direction_change                 : 1;
		/** (Option) Set Re-timer Mode */
		uint32_t set_retimer_mode                       : 1;
		/** Connect Change */
		uint32_t connect_change                         : 1;
		/** Error */
		uint32_t error                                  : 1;
		/** Sink Path Status Change */
		uint32_t sink_path_status_change		: 1;
		/** Reserved and shall be set to zero */
		uint32_t reserved0				:15;
	};
	uint32_t raw_value;
};

/**
 * @brief capabilities of a connector
 */
union connector_capability_t {
	struct {
		/**
		 * The op_mode_x fields indicate wht mode
		 * that the connector supports
		 */

		/** RP only */
		uint32_t op_mode_rp_only	: 1;
		/** RD only */
		uint32_t op_mode_rd_only	: 1;
		/** DRP */
		uint32_t op_mode_drp		: 1;
		/** Analog audio Accessory (Ra/Ra) */
		uint32_t op_mode_analog_audio	: 1;
		/** Debug Accessory Mode (Rd/Rd) */
		uint32_t op_mode_debug_acc	: 1;
		/** USB2 */
		uint32_t op_mode_usb2		: 1;
		/** USB3 */
		uint32_t op_mode_usb3		: 1;
		/** Alternate Modes */
		uint32_t op_mode_alternate	: 1;
		/**
		 * Valid only when the operation mode is DRP or
		 * Rp only. This bit shall be set to one if the
		 * connector is capable of providing power on
		 * this connector.
		 */
		uint32_t provider		: 1;
		/**
		 * Valid only when the operation mode is DRP or
		 * Rd only. This bit shall be set to one if the
		 * connector is capable of consuming power on
		 * this connector.
		 */
		uint32_t consumer		: 1;
		/**
		 * Valid only when the operation mode is DRP or
		 * Rp only or Rd only. This bit shall be set to one if the
		 * connector is capable of accepting swap to DFP.
		 */
		uint32_t swap_to_dfp		: 1;
		/**
		 * Valid only when the operation mode is DRP or
		 * Rp only or Rd only. This bit shall be set to one if the
		 * connector is capable of accepting swap to UFP.
		 */
		uint32_t swap_to_ufp		: 1;
		/**
		 * Valid only when the operation mode is DRP. This
		 * bit shall be set to one if the connector is capable of
		 * accepting swap to SRC.
		 */
		uint32_t swap_to_src		: 1;
		/**
		 * Valid only when the operation mode is DRP. This
		 * bit shall be set to one if the connector is capable of
		 * accepting swap to SNK.
		 */
		uint32_t swap_to_snk		: 1;
		/** USB4 Gen 2 */
		uint32_t ext_op_mode_usb4_gen2	: 1;
		/** EPR Source */
		uint32_t ext_op_mode_epr_source	: 1;
		/** EPR Sink */
		uint32_t ext_op_mode_epr_sink	: 1;
		/** USB4 Gen 3 */
		uint32_t ext_op_mode_usb4_gen3	: 1;
		/** USB4 Gen 4 */
		uint32_t ext_op_mode_usb4_gen4	: 1;
		/** Reserved */
		uint32_t ext_op_mode_reserved0	: 1;
		/** Reserved */
		uint32_t ext_op_mode_reserved1  : 1;
		/** Reserved */
		uint32_t ext_op_mode_reserved2  : 1;
		/** FW Update */
		uint32_t misc_caps_fw_update	: 1;
		/** Security */
		uint32_t misc_caps_security	: 1;
		/** Reserved, set to 0 */
		uint32_t misc_caps_reserved0	: 1;
		/** Reserved, set to 0 */
		uint32_t misc_caps_reserved1	: 1;
		/**
		 * Debug information. This bit shall be set to one
		 * the the feature is supported. Otherwise, this bit shall
		 * be set to zero.
		 */
		uint32_t reverse_current_prot	: 1;
		/**
		 * Port Partner’s major USB PD Revision from the Specification
		 * Revision field of the USB PD message Header.
		 */
		uint32_t partner_pd_revision	: 2;
		/** Reserved, set to 0 */
		uint32_t reserved		: 3;
	};
	uint32_t raw_value;
};

/**
 * @brief Connector Status Change Field
 */
union conn_status_change_t {
	struct {
		/** Reserved, set to 0 */
		uint16_t reserved0		: 1;
		/**
		 * When set to 1b, the GET_PDO command can be sent to the
		 * attached supply.
		 */
		uint16_t external_supply_change	: 1;
		/**
		 * When set to 1b, the Power Operation Mode field in the STATUS
		 * Data Structure shall indicate the current power operational
		 * mode of the connector.
		 */
		uint16_t pwr_operation_mode	: 1;
		/**
		 * This bit shall be set to 1b when the PDC receives an
		 * attention from the port partner.
		 */
		uint16_t attention		: 1;
		/** Reserved, set to 0 */
		uint16_t reserved1		: 1;
		/**
		 * When set to 1b, the updated Power Data Objects should be
		 * requested using the GET_PDOS command.
		 */
		uint16_t supported_provider_caps : 1;
		/**
		 * When set to 1b, the Request Data Object field in the STATUS
		 * Data Structure shall indicate the newly negotiated power
		 * level.
		 */
		uint16_t negotiated_power_level	: 1;
		/**
		 * This bit shall be set to 1b when the PDC completes a PD Hard Reset
		 * requested by the connector partner.
		 */
		uint16_t pd_reset_complete	: 1;
		/**
		 * When set to 1b, the updated Alternate Modes should be
		 * read with the GET_CAM_SUPPORTED command.
		 */
		uint16_t supported_cam		: 1;
		/**
		 * This bit shall be set to 1b when the Battery Charging
		 * status changes.
		 */
		uint16_t battery_charging_status : 1;
		/**
		 * Reserved, set to 0.
		 */
		uint16_t reserved2		: 1;
		/**
		 * This bit shall be set to 1b when the Connector Partner
		 * Type field or Connector Partner Flags change.
		 */
		uint16_t connector_partner	: 1;
		/**
		 * This bit shall be set to 1b when the PDC completes a Power
		 * Role Swap is completed.
		 */
		uint16_t pwr_direction		: 1;
		/**
		 * This bit shall be set to 1b when the Sink Path
		 * Status changes.
		 */
		uint16_t sink_path_status_change : 1;
		/**
		 * This bit shall be set to 1b when a device gets either
		 * connected or disconnected and the Connect Status field
		 * in the GET_CONNECTOR_STATUS Data Structure changes.
		 */
		uint16_t connect_change		: 1;
		/**
		 * When set to 1b, this field shall indicate that an error
		 * has occurred on the connector.
		 */
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

enum pdo_type_t {
	SINK_PDO,
	SOURCE_PDO,
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


struct pdo_t {
	uint32_t pdo0;
	uint32_t pdo1;
	uint32_t pdo2;
	uint32_t pdo3;
};


#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* ZEPHYR_INCLUDE_DRIVERS_UCSI_V3_H_ */
