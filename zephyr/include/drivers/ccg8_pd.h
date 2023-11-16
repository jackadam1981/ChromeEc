#include <zephyr/device.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HOST_CAPCTRL_REG_PD_P0 0x105C
#define HOST_CAPCTRL_REG_PD_P1 0x205C
#define INTEL_HOSTCAP_CTRL_PD_REG_LEN 1

#define PD_ICL_BB_RETIMER_CMD_REG 0x0046
#define PD_ICL_BB_RETIMER_CMD_REG_LEN 2
#define PD_ICL_CTRL_REG 0x0040
#define PD_ICL_CTRL_REG_LEN 1

struct ccg8_hostcap_driver_api {
	int (*pd_write_powmode)(const struct device *dev,
                               uint16_t reg, uint8_t len, void *data);
};

__syscall int pd_write_powmode(const struct device *dev,
			      uint16_t reg, uint8_t len, void *data);

static inline int z_impl_pd_write_powmode(const struct device *dev,
					 uint16_t reg, uint8_t len, void *data)
{
	const struct ccg8_hostcap_driver_api *api =
		(const struct ccg8_hostcap_driver_api *)dev->api;

	return api->pd_write_powmode(dev, reg, len, data);
}

#ifdef __cplusplus
}
#endif

/**
 * @}
 */
#include <syscalls/ccg8_pd.h>
