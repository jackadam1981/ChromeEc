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

#include "ucsi_v3.h"
#include "zephyr/drivers/usb_c/usbc_pd.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @typedef
 * @brief These are the API function types
 */
typedef int (*pdc_enable_t)(const struct device *dev);
typedef int (*pdc_get_ucsi_version_t)(const struct device *dev,
					uint16_t *version);
typedef int (*pdc_reset_t)(const struct device *dev);
typedef int (*pdc_connector_reset_t)(const struct device *dev,
					enum connector_reset_t type);
typedef int (*pdc_set_notification_enable_t)(const struct device *dev,
					union notification_enable_t bits,
					uint16_t ext_bits);
typedef int (*pdc_get_capability_t)(const struct device *dev,
					struct capability_t *caps);
typedef int (*pdc_get_connector_capability_t)(const struct device *dev,
					union connector_capability_t *caps);
typedef int (*pdc_set_ccom_t)(const struct device *dev, enum ccom_t ccom,
					enum drp_mode_t dm);
typedef int (*pdc_set_uor_t)(const struct device *dev, union uor_t uor);
typedef int (*pdc_set_pdr_t)(const struct device *dev, union pdr_t pdr);
typedef int (*pdc_set_sink_path_t)(const struct device *dev, bool en);
typedef int (*pdc_get_connector_status_t)(const struct device *dev,
					struct connector_status_t *cs);
typedef int (*pdc_get_error_status_t)(const struct device *dev,
					union error_status_t *es);
typedef void (*pdc_cci_handler_cb_t)(union cci_event_t cci_event);
typedef int (*pdc_set_handler_cb_t)(const struct device *dev,
					pdc_cci_handler_cb_t cci_cb);
typedef int (*pdc_get_vbus_t)(const struct device *dev, uint16_t *vbus);
typedef int (*pdc_get_pdos_t)(const struct device *dev,
				enum pdo_type_t pdo_type,
				enum pdo_offset_t pdo_offset, uint8_t num_pdos,
				bool port_partner_pdo, uint32_t *pdos);
typedef int (*pdc_get_rdo_t)(const struct device *dev, uint32_t *rdo);
typedef int (*pdc_is_flash_code_t)(const struct device *dev,
				uint8_t *is_flash_code);
typedef int (*pdc_get_fw_version_t)(const struct device *dev,
				uint32_t *fw_version);
typedef int (*pdc_get_vid_pid_t)(const struct device *dev, uint32_t *vidpid);
typedef int (*pdc_get_pd_version_t)(const struct device *dev,
				uint32_t *pd_version);
typedef int (*pdc_get_current_pdo_t)(const struct device *dev, uint32_t *pdo);
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
	pdc_connector_reset_t connector_reset;
	pdc_set_notification_enable_t set_notification_enable;
	pdc_get_capability_t get_capability;
	pdc_get_connector_capability_t get_connector_capability;
	pdc_set_ccom_t set_ccom;
	pdc_set_uor_t set_uor;
	pdc_set_pdr_t set_pdr;
	pdc_set_sink_path_t set_sink_path;
	pdc_get_connector_status_t get_connector_status;
	pdc_get_error_status_t get_error_status;
	pdc_set_handler_cb_t set_handler_cb;
	pdc_get_vbus_t get_vbus_voltage;
	pdc_get_current_pdo_t get_current_pdo;
	pdc_get_pdos_t get_pdos;
	pdc_get_rdo_t get_rdo;
	pdc_read_power_level_t read_power_level;
	pdc_is_flash_code_t is_flash_code;
	pdc_get_fw_version_t get_fw_version;
	pdc_get_vid_pid_t get_vid_pid;
	pdc_get_pd_version_t get_pd_version;
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
	const struct pdc_driver_api_t *api =
			(const struct pdc_driver_api_t *)dev->api;

	__ASSERT(api->enable != NULL,
			"ENABLE is not optional");

	return api->enable(dev);
}

/**
 * @brief Trigger the PDC to ead the power level when in Source mode.
 *        If the connector is in Sink Mode or is disconnected, this
 *        command has no effect, and the Error information field in
 *        the CCI event is set.
 *
 * @param dev PDC device structure pointer
 *
 * @retval 0 on API call success
 * @retval -EBUSY if not ready to execute the command
 */
static inline int pdc_read_power_level(const struct device *dev)
{
	const struct pdc_driver_api_t *api = (const struct pdc_driver_api_t *)dev->api;

	__ASSERT(api->read_power_level != NULL,
			"READ_POWER_LEVEL is not optional");

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
 * @retval -EINVAL if caps is NULL
 */
static inline int pdc_get_ucsi_version(const struct device *dev,
					uint16_t *version)
{
	const struct pdc_driver_api_t *api =
			(const struct pdc_driver_api_t *)dev->api;

	__ASSERT(api->get_ucsi_version != NULL,
			"GET_UCSI_VERSION is not optional");

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

	__ASSERT(api->reset != NULL,
			"RESET is not optional");

	return api->reset(dev);
}

/**
 * @brief Resets a PDC connector
 * @note cci_event.command_completed_indicator is set when the UCSI command
 *       complete.
 * @note cci_event.error_indicator is set if the UCSI command was
 *       unsuccessful.
 *
 * @param dev PDC device structure pointer
 *
 * @retval 0 on success
 * @retval -EBUSY if not ready to execute the command
 */
static inline int pdc_connector_reset(const struct device *dev,
				enum connector_reset_t type)
{
	const struct pdc_driver_api_t *api =
			(const struct pdc_driver_api_t *)dev->api;

	__ASSERT(api->connector_reset != NULL,
			"CONNECTOR_RESET is not optional");

	return api->connector_reset(dev, type);
}

/**
 * @brief Enable the Sink FET while in the Attached Sink State
 * @note cci_event.command_completed_indicator is set when the UCSI command
 *       completes.
 * @note cci_event.error_indicator is set if the UCSI command was
 *       unsuccessful.
 *
 * @param dev PDC device structure pointer
 * @param en true to enable the Sink FET or false to disable it
 *
 * @retval 0 on success
 * @retval -EBUSY if not ready to execute the command
 */
static inline int pdc_set_sink_path(const struct device *dev, bool en)
{
	const struct pdc_driver_api_t *api =
			(const struct pdc_driver_api_t *)dev->api;

	__ASSERT(api->set_sink_path != NULL,
			"SET_SINK_PATH is not optional");

	return api->set_sink_path(dev, en);
}

/**
 * @brief Sets the list of asynchronous events that the PDC may send
 *        notifications about.
 * @note cci_event.command_completed_indicator is set when the UCSI command
 *       completes.
 * @note cci_event.error_indicator is set if the UCSI command was unsuccessful
 *
 * @param dev PDC device structure pointer
 * @param bits a bitmask of the notifications.
 *
 * @retval 0 on success
 * @retval -EBUSY if not ready to execute the command
 */
static inline int pdc_set_notification_enable(const struct device *dev,
			union notification_enable_t bits, uint16_t ext_bits)
{
	const struct pdc_driver_api_t *api =
				(const struct pdc_driver_api_t *)dev->api;

	__ASSERT(api->set_notification_enable != NULL,
			"SET_NOTIFICATION_ENABLE is not optional");

	return api->set_notification_enable(dev, bits, ext_bits);
}

/**
 * @brief Gets the PDC device capabilities.
 * @note cci_event.command_completed_indicator is set when the UCSI command
 *       completes.
 * @note cci_event.error_indicator is set if the UCSI command was unsuccessful
 *
 * @param dev PDC device structure pointer
 * @param caps pointer where the device capabilities are stored.
 *
 * @retval 0 on success
 * @retval -EBUSY if not ready to execute the command
 * @retval -EINVAL if caps is NULL
 */
static inline int pdc_get_capability(const struct device *dev,
				struct capability_t *caps)
{
	const struct pdc_driver_api_t *api =
			(const struct pdc_driver_api_t *)dev->api;

	__ASSERT(api->get_capability != NULL,
			"GET_CAPABILITY is not optional");

	return api->get_capability(dev, caps);
}

/**
 * @brief Gets the PDC connector status.
 * @note cci_event.command_completed_indicator is set when the UCSI command
 *       completes.
 * @note cci_event.error_indicator is set if the UCSI command was
 *       unsuccessful.
 *
 * @param dev PDC device structure pointer
 * @param cs pointer where the connector status is stored.
 *
 * @retval 0 on success
 * @retval -EBUSY if not ready to execute the command
 * @retval -EINVAL if cs is NULL
 */
static inline int pdc_get_connector_status(const struct device *dev,
				struct connector_status_t *cs)
{
	const struct pdc_driver_api_t *api =
			(const struct pdc_driver_api_t *)dev->api;

	__ASSERT(api->get_connector_status != NULL,
			"GET_CONNECTOR_STATUS is not optional");

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
 * @retval -EINVAL if es is NULL
 */
static inline int pdc_get_error_status(const struct device *dev,
					union error_status_t *es)
{
	const struct pdc_driver_api_t *api =
				(const struct pdc_driver_api_t *)dev->api;

	__ASSERT(api->get_error_status != NULL,
			"GET_ERROR_STATUS is not optional");

	return api->get_error_status(dev, es);
}

/**
 * @brief Gets capabilities of a connector
 * @note cci_event.command_completed_indicator is set when the UCSI command
 *       completes.
 * @note cci_event.error_indicator is set if the UCSI command was
 *       unsuccessful.
 *
 * @param dev PDC device structure pointer
 * @param caps pointer where the connector capabilities are stored.
 *
 * @retval 0 on success
 * @retval -EBUSY if not ready to execute the command
 * @retval -EINVAL if caps is NULL
 */
static inline int pdc_get_connector_capability(const struct device *dev,
				union connector_capability_t *caps)
{
	const struct pdc_driver_api_t *api =
			(const struct pdc_driver_api_t *)dev->api;

	__ASSERT(api->get_connector_capability != NULL,
			"GET_CONNECTOR_CAPABILITY is not optional");

	return api->get_connector_capability(dev, caps);
}

/**
 * @brief Sets the CC operation mode of the PDC
 * @note cci_event.command_completed_indicator is set when the UCSI command
 *       completes.
 * @note cci_event.error_indicator is set if the UCSI command was unsuccessful
 *
 * @param dev PDC device structure pointer
 * @param ccom CC operation mode
 *
 * @retval 0 on success
 * @retval -EBUSY if not ready to execute the command
 * @retval -ENOSYS if not implemented
 */
static inline int pdc_set_ccom(const struct device *dev,
			enum ccom_t ccom, enum drp_mode_t dm)
{
	const struct pdc_driver_api_t *api =
			(const struct pdc_driver_api_t *)dev->api;

	/* This is an optional feature, so it might not be implemented */
	if (api->set_ccom == NULL) {
		return -ENOSYS;
	}

	return api->set_ccom(dev, ccom, dm);
}

/**
 * @brief Sets the USB operation role of the PDC
 * @note cci_event.command_completed_indicator is set when the UCSI command
 *       completes.
 * @note cci_event.error_indicator is set if the UCSI command was unsuccessful
 *
 * @param dev PDC device structure pointer
 * @param uor USB operation role
 *
 * @retval 0 on success
 * @retval -EBUSY if not ready to execute the command
 */
static inline int pdc_set_uor(const struct device *dev, union uor_t uor)
{
	const struct pdc_driver_api_t *api =
			(const struct pdc_driver_api_t *)dev->api;

	__ASSERT(api->set_uor != NULL, "SET_UOR is not optional");

	return api->set_uor(dev, uor);
}

/**
 * @brief Sets the Power direction role of the PDC
 * @note cci_event.command_completed_indicator is set when the UCSI command
 *       completes.
 * @note cci_event.error_indicator is set if the UCSI command was unsuccessful
 *
 * @param dev PDC device structure pointer
 * @param pdr Power direction role
 *
 * @retval 0 on success
 * @retval -EBUSY if not ready to execute the command
 */
static inline int pdc_set_pdr(const struct device *dev, union pdr_t pdr)
{
	const struct pdc_driver_api_t *api =
			(const struct pdc_driver_api_t *)dev->api;

	__ASSERT(api->set_pdr != NULL, "SET_PDR is not optional");

	return api->set_pdr(dev, pdr);
}

/**
 * @brief Sets the callback the driver uses to communicate events to the TCPM
 *
 * @param dev PDC device structure pointer
 * @param cci_cb pointer to callback
 */
static inline void pdc_set_handler_cb(const struct device *dev,
				pdc_cci_handler_cb_t cci_cb)
{
	const struct pdc_driver_api_t *api =
			(const struct pdc_driver_api_t *)dev->api;

	__ASSERT(api->set_handler_cb != NULL, "SET_HANDLER_CB is not optional");

	api->set_handler_cb(dev, cci_cb);
}

/**
 * @brief Reads the VBUS voltage
 *
 * @param dev PDC device structure pointer
 * @param voltage pointer to where the voltage is stored
 *
 * @retval 0 on success
 * @retval -EBUSY if not ready to execute the command
 * @retval -EINVAL if voltage pointer is NULL
 */
static inline int pdc_get_vbus_voltage(const struct device *dev,
					uint16_t *voltage)
{
	const struct pdc_driver_api_t *api =
			(const struct pdc_driver_api_t *)dev->api;

	__ASSERT(api->get_vbus_voltage != NULL, "GET_RDO is not optional");

	return api->get_vbus_voltage(dev, voltage);
}

/**
 * @brief Gets the Sink or Source PDOs associated with the connector, or its
 *        capabilities.
 * @note cci_event.command_completed_indicator is set when the UCSI command
 *       completes.
 * @note cci_event.error_indicator is set if the UCSI command was
 *       unsuccessful.
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
 * @param sc request the Source or Sink Capabilities instead of the PDOs. This
 *           parameter is only valid when partner_pdo is false.
 * @param pdos pointer to where the PDOs or Capabilities are stored.
 * @param es pointer where the error status is stored.
 *
 * @retval 0 on success
 * @retval -EBUSY if not ready to execute the command
 * @retval -EINVAL if pdos is NULL
 * @retval -ENOSYS if not implemented
 */
static inline int pdc_get_pdos(const struct device *dev,
			enum pdo_type_t pdo_type, enum pdo_offset_t pdo_offset,
			uint8_t num_pdos, bool port_partner_pdo, uint32_t *pdos)
{
	const struct pdc_driver_api_t *api =
			(const struct pdc_driver_api_t *)dev->api;

	/* This is an optional feature, so it might not be implemented */
	if (api->get_pdos == NULL) {
		return -ENOSYS;
	}

	return api->get_pdos(dev, pdo_type, pdo_offset, num_pdos,
			port_partner_pdo, pdos);
}

/**
 * @brief Tests if the PDC is executing from Flash or ROM code
 *
 * @param dev PDC device structure pointer
 * @param is_flash_code pointer to where the boolean is stored
 *
 * @retval 0 on success
 * @retval -EBUSY if not ready to execute the command
 * @retval -EINVAL if is_flash_code is NULL
 * @retval -ENOSYS if not implemented
 */
static inline int pdc_is_flash_code(const struct device *dev,
				uint8_t *is_flash_code)
{
	const struct pdc_driver_api_t *api =
			(const struct pdc_driver_api_t *)dev->api;

	/* This is an optional feature, so it might not be implemented */
	if (api->is_flash_code == NULL) {
		return -ENOSYS;
	}

	return api->is_flash_code(dev, is_flash_code);
}

/**
 * @brief Get the FW version of the PDC
 *
 * @param dev PDC device structure pointer
 * @param fw_version pointer to where the FW version is stored
 *
 * @retval 0 on success
 * @retval -EBUSY if not ready to execute the command
 * @retval -EINVAL if fw_version pointers is NULL
 * @retval -ENOSYS if not implemented
 */
static inline int pdc_get_fw_version(const struct device *dev,
				uint32_t *fw_version)
{
	const struct pdc_driver_api_t *api =
			(const struct pdc_driver_api_t *)dev->api;

	/* This is an optional feature, so it might not be implemented */
	if (api->get_fw_version == NULL) {
		return -ENOSYS;
	}

	return api->get_fw_version(dev, fw_version);
}

/**
 * @brief Get the PDC's VID and PID
 *
 * @param dev PDC device structure pointer
 * @param vid_pid pointer to where the VID:PID is stored
 *
 * @retval 0 on success
 * @retval -EBUSY if not ready to execute the command
 * @retval -EINVAL if vidpid pointers is NULL
 * @retval -ENOSYS if not implemented
 */
static inline int pdc_get_vid_pid(const struct device *dev,
				uint32_t *vidpid)
{
	const struct pdc_driver_api_t *api =
			(const struct pdc_driver_api_t *)dev->api;

	/* This is an optional feature, so it might not be implemented */
	if (api->get_vid_pid == NULL) {
		return -ENOSYS;
	}

	return api->get_vid_pid(dev, vidpid);
}

/**
 * @brief Get the PD Version of the Port Partner
 *
 * @param dev PDC device structure pointer
 * @param pd_version pointer to where the pd_version is stored
 *
 * @retval 0 on success
 * @retval -EBUSY if not ready to execute the command
 * @retval -EINVAL if pdo pointer is NULL
 * @retval -ENOSYS if not implemented
 */
static inline int pdc_get_pd_version(const struct device *dev,
					uint32_t *pd_version)
{
	const struct pdc_driver_api_t *api =
			(const struct pdc_driver_api_t *)dev->api;

	/* This is an optional feature, so it might not be implemented */
	if (api->get_pd_version == NULL) {
		return -ENOSYS;
	}

	return api->get_pd_version(dev, pd_version);
}

/**
 * @brief Get the Requested Data Object sent to the Source
 *
 * @param dev PDC device structure pointer
 * @param pdo pointer to where the PDO is stored
 *
 * @retval 0 on success
 * @retval -EBUSY if not ready to execute the command
 * @retval -EINVAL if rdo pointer is NULL
 */
static inline int pdc_get_rdo(const struct device *dev, uint32_t *rdo)
{
	const struct pdc_driver_api_t *api =
			(const struct pdc_driver_api_t *)dev->api;

	__ASSERT(api->get_rdo != NULL, "GET_RDO is not optional");

	return api->get_rdo(dev, rdo);
}

/**
 * @brief Get the currently selected PDO that was requested from the Source
 *
 * @param dev PDC device structure pointer
 * @param pdo pointer to where the PDO is stored
 *
 * @retval 0 on success
 * @retval -EBUSY if not ready to execute the command
 * @retval -EINVAL if pdo pointer is NULL
 * @retval -ENOSYS if not implemented
 */
static inline int pdc_get_current_pdo(const struct device *dev, uint32_t *pdo)
{
	const struct pdc_driver_api_t *api =
			(const struct pdc_driver_api_t *)dev->api;

	/* This is an optional feature, so it might not be implemented */
	if (api->get_current_pdo == NULL) {
		return -ENOSYS;
	}

	return api->get_current_pdo(dev, pdo);
}

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* ZEPHYR_INCLUDE_DRIVERS_PDC_H_ */
