/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * TCPMv3
 */
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <drivers/pdc.h>

LOG_MODULE_DECLARE(pdc, LOG_LEVEL_INF);

#define NODE_ID	DT_NODELABEL(pdc_power_p0p1)
#if DT_NODE_HAS_STATUS(NODE_ID, okay)
#define PDC0	NODE_ID
#else
#error "Can't find PDC Node"
#endif


static void create_thread(const struct device *dev);

struct tcpm_data_t {
	/** This port's thread */
	k_tid_t thread;
	/** This port thread's data */
	struct k_thread thread_data;
	const struct device *dev;
	const struct device *pdc[2];
};

struct tcpm_config_t {
	/**
	 * The usbc stack initializes this pointer that creates the
	 * main thread for this port
	 */
	void (*create_thread)(const struct device *dev);
};

K_THREAD_STACK_DEFINE(tcpm_stack_area, 2000);

static struct tcpm_data_t tcpm_data = {
	.pdc[0] = DEVICE_DT_GET(PDC0),
};

static const struct tcpm_config_t tcpm_config = {
	.create_thread = create_thread,
};

static void tcpm_cci_handler_cb(union cci_event_t cci_event)
{
	LOG_INF("DATA_LEN: %d\n", cci_event.data_len);

	if (cci_event.reset_completed) {
		LOG_INF("TCPM reset\n");
	}

	if (cci_event.busy) {
              LOG_INF("TCPM busy\n");
        }

        if (cci_event.error) {
		LOG_INF("TCPM error\n");
        }

        if (cci_event.command_completed) {
		LOG_INF("TCPM done\n");
        }
}


/*
 * Some TEST code to access the PDC driver
 */
static void run_tcpm(void *dev, void *unused1, void *unused2)
{
	struct tcpm_data_t *data = ((const struct device *)dev)->data;
	int rv;

	int k = 0;
	union notification_enable_t bits;
	struct device_capability_t caps;
	union connector_capability_t ccaps;
	uint16_t vb;
	uint32_t fwv;
	uint32_t vid_pid;
	uint32_t pd_version;
	int delay = 0;
	uint8_t result;
	struct connector_status_t cs;

	union pdo_source_t pdos[7];
	union rdo_fixed_t rdo;

	caps.bNumPorts = 0;

	while (1) {
		/* Let Zephyr logging stabilize before starting the PDC */
		/* This is only added so the PDC logs are easier to read */
		if (delay != 5) {
			delay++;
			printk("DELAY: %d\n", delay);
		}

		if (delay == 5) {
			switch (k) {
			case 0:
				/* Enable PDC */
				pdc_enable(data->pdc[0]);
				k++;
				break;
			case 1:
				/* Set notifications */
				bits.raw_value = 0xDBE7; //0xffff;
				pdc_set_notification_enable(data->pdc[0], bits, 0);
				k++;
				break;
			case 2:
				/* Reset PDC */
				pdc_reset(data->pdc[0]);
				k++;
				break;
			case 3:
				/* Set CC Operation mode */
				pdc_set_ccom(data->pdc[0], CCOM_DRP, DRP_NORMAL);
				k++;
				break;
			case 4:
				/* Get device capabilities */
				pdc_get_capability(data->pdc[0], &caps);
				k++;
				break;
			case 5:
				/* Analyze device caps */
				printk("\n\n***PNUM: %d\n", caps.bNumPorts);
				k++;
				break;
			case 6:
				/* Get Connector caps */
				pdc_get_connector_capability(data->pdc[0], &ccaps);
				k++;
				break;
			case 7:
				/* Analyze connector caps */
				printk("\n\n***CCAPS: %04x\n", ccaps.raw_value);
				k++;
				break;
			case 8:
				/* Realtek said this needed to be called before every
				 * pdc_getvbus_voltage command, but it seems it only
				 * needs to be called once.
				 */
				pdc_read_power_level(data->pdc[0]);
				k++;
				break;
			case 9:
				vb = 0;
				rv = pdc_getvbus_voltage(data->pdc[0], &vb);
				k++;
				break;
			case 10:
				printk("V(%d): %04x\n", rv, vb);
				k++;
				break;
			case 11:
				vb = 0;
				rv = pdc_get_fw_version(data->pdc[0], &fwv);
				k++;
				break;
			case 12:
				printk("TEST(%d): %08x\n", rv, fwv);
				k++;
				break;
			case 13:
				rv = pdc_get_vid_pid(data->pdc[0], &vid_pid);
				k++;
				break;
			case 14:
				printk("VIDPID(%d): %08x\n", rv, vid_pid);
				k++;
				break;
			case 15:
				rv = pdc_get_pd_version(data->pdc[0], &pd_version);
				k++;
				break;
			case 16:
				printk("PDV(%d): %08x\n", rv, pd_version);
				k++;
				break;
			case 17:
				rv = pdc_is_typec_connected(data->pdc[0], &result);
				k++;
				break;
			case 18:
				printk("TC Conn(%d): %d\n", rv, result);
				k++;
				break;
			case 19:
				rv = pdc_is_pd_ready(data->pdc[0], &result);
				k++;
				break;
			case 20:
				printk("PD Ready(%d): %d\n", rv, result);
				k++;
				break;
			case 21:
				rv = pdc_get_rdo(data->pdc[0], &rdo.raw_value);
				k++;
				break;
			case 22:
				printk("RDO(%d): %d\n",rv, rdo.obj_position);
				k++;
				break;
			case 23:
				rv = pdc_get_pdo(data->pdc[0],
						SOURCE_PDO,
						PDO_OFFSET_0,
						7,
						true,
						&pdos[0].raw_value);
				k++;
				break;
			case 24:
				for (int i = 0; i < 7; i++) {
					printk("PDO%d: %d\n", i, pdos[i].voltage * 50);
				}
				k++;
				break;
			case 25:
				rv = pdc_get_connector_status(data->pdc[0], &cs);
				k++;
				break;
			case 26:
				printk("RDO: %08x\n", cs.rdo);
				printk("VBUS: %d %d\n", cs.voltage_scale, cs.voltage_reading * 50);
				printk("FET: %d\n", cs.sink_path_status);
				printk("ORI: %d\n", cs.orientation);
				k++;
				break;
			case 27:
				break;
			}
		}

		k_sleep(K_MSEC(500));
	}
}

/**
 * @brief Initialize the USB-C Subsystem
 */
static int tcpm_subsys_init(const struct device *dev)
{
	struct tcpm_data_t *data = dev->data;
	const struct tcpm_config_t *const cfg = dev->config;

	/* Make sure TCPC is ready */
	if (!device_is_ready(data->pdc[0])) {
		LOG_ERR("PDC NOT READY\n");
		return -ENODEV;
	}

	/* Set CCI Event callback */
	pdc_set_handler_cb(data->pdc[0], tcpm_cci_handler_cb);

	/* Create the thread for this port */
	cfg->create_thread(dev);
	data->dev = dev;

	LOG_INF("\n\nTCPMv3 Started\n");
	return 0;
}

static void create_thread(const struct device *dev)
{
	struct tcpm_data_t *data = dev->data;

	printk("CREATE THREAD\n");
	data->thread = k_thread_create(
		&data->thread_data, tcpm_stack_area,
		K_THREAD_STACK_SIZEOF(tcpm_stack_area), run_tcpm, (void *)dev,
		0, 0, 8, K_ESSENTIAL, K_NO_WAIT);
}

DEVICE_DEFINE(pdc_power_p0p1, "pdc_power_p0p1", &tcpm_subsys_init, NULL, &tcpm_data,
			&tcpm_config, POST_KERNEL,
			CONFIG_APPLICATION_INIT_PRIORITY, NULL);
