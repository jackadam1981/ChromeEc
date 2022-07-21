#ifndef ZEPHYR_INCLUDE_EMUL_STUB_DEVICE_H_
#define ZEPHYR_INCLUDE_EMUL_STUB_DEVICE_H_

#include <zephyr/device.h>
#include <zephyr/devicetree.h>

/*
 * Needed for emulators without corresponding DEVICE_DT_DEFINE drivers
 */
__maybe_unused static int emul_init_stub(const struct device *dev)
{
	ARG_UNUSED(dev);

	return 0;
}

struct emul_stub_dev_data {
	/* Stub */
};
struct emul_stub_dev_config {
	/* Stub */
};
struct emul_stub_dev_api {
	/* Stub */
};

#define EMUL_STUB_DEVICE(n)                                             \
                                                                        \
	/* Since this is only stub, allocate the structs once. */       \
	static struct emul_stub_dev_data stub_data_##n;                 \
	static struct emul_stub_dev_config stub_config_##n;             \
	static struct emul_stub_dev_api stub_api_##n;                   \
                                                                        \
	DEVICE_DT_INST_DEFINE(n, &emul_init_stub, NULL, &stub_data_##n, \
			      &stub_config_##n, POST_KERNEL, 1,         \
			      &stub_api_##n);

#endif /* ZEPHYR_INCLUDE_EMUL_STUB_DEVICE_H_ */
