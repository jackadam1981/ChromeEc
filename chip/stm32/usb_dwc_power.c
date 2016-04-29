/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "hooks.h"
#include "i2c.h"
#include "link_defs.h"
#include "registers.h"
#include "timer.h"
#include "usb_descriptor.h"
#include "usb_dwc_power.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_I2C, format, ## args)



static int usb_power_init_inas(struct usb_power_config const *config);
static int usb_power_read(struct usb_power_config const *config);
static int usb_power_write_line(struct usb_power_config const *config);


static int8_t usb_power_map_error(int error)
{
	switch (error) {
	case EC_SUCCESS:       return USB_POWER_SUCCESS;
	case EC_ERROR_TIMEOUT: return USB_POWER_TIMEOUT;
	case EC_ERROR_BUSY:    return USB_POWER_BUSY;
	default:               return USB_POWER_UNKNOWN_ERROR | (error & 0x7f);
	}
}


/* Let the USB HW IN-to-host FIFO transmit some bytes */
static void usb_enable_tx(struct usb_power_config const *config, int len)
{
        struct dwc_usb_ep *ep = config->ep;
        ep->in_data = ep->in_databuffer;
        ep->in_packets = 1;
        ep->in_pending = len;

        GR_USB_DIEPTSIZ(config->endpoint) = 0;

        GR_USB_DIEPTSIZ(config->endpoint) |= DXEPTSIZ_PKTCNT(1);
        GR_USB_DIEPTSIZ(config->endpoint) |= DXEPTSIZ_XFERSIZE(len);
        GR_USB_DIEPDMA(config->endpoint) = (uint32_t)ep->in_data;

        GR_USB_DIEPCTL(config->endpoint) |= DXEPCTL_CNAK | DXEPCTL_EPENA;
}

/* Let the USB HW OUT-from-host FIFO receive some bytes */
static void usb_enable_rx(struct usb_power_config const *config, int len)
{
        struct dwc_usb_ep *ep = config->ep;
        ep->out_data = ep->out_databuffer;
        ep->out_pending = 0;

        GR_USB_DOEPTSIZ(config->endpoint) = 0;
        GR_USB_DOEPTSIZ(config->endpoint) |= DXEPTSIZ_PKTCNT(1);
        GR_USB_DOEPTSIZ(config->endpoint) |= DXEPTSIZ_XFERSIZE(len);
        GR_USB_DOEPDMA(config->endpoint) = (uint32_t)ep->out_data;

        GR_USB_DOEPCTL(config->endpoint) |= DXEPCTL_CNAK | DXEPCTL_EPENA;
}

/* True if the HW Rx/OUT FIFO is currently listening. */
static inline int rx_fifo_is_active(struct usb_power_config const *config)
{
	return (GR_USB_DOEPCTL(config->endpoint) & DXEPCTL_EPENA);
}

/* True if the HW Rx/OUT FIFO has bytes for us. */
static inline int rx_fifo_is_ready(struct usb_power_config const *config)
{
        struct dwc_usb_ep *ep = config->ep;

        return ep->out_pending;
}

/* True if the Tx/IN FIFO can take some bytes from us. */
static inline int tx_fifo_is_ready(struct usb_power_config const *config)
{
	struct dwc_usb_ep *ep = config->ep;
	int ready;

	/* Is the tx hw idle? */
	ready = !(GR_USB_DIEPCTL(config->endpoint) & DXEPCTL_EPENA);

	/* Is there no pending data? */
	ready &= (ep->in_pending == 0);
	return ready;
}


void usb_power_deferred(struct usb_power_config const *config)
{
	/*struct dwc_usb_ep *ep = config->ep;

        CPRINTS("usb_power_deferred in_p %d, DIEPCTL 0x%08x, out_p %d, DOEPCTL 0x%08x", 
		ep->in_pending, GR_USB_DIEPCTL(config->endpoint), 
		ep->out_pending, GR_USB_DOEPCTL(config->endpoint));*/

	/* Handle an incoming command if available */
        if (rx_fifo_is_ready(config)) {
		/*CPRINTS("usb_power_deferred calls usb_power_read");*/
		usb_power_read(config);
	}

	/* Enable RX once we have finished replying. We don't want
	 * overlapping transactions */ 
	if (tx_fifo_is_ready(config) && !rx_fifo_is_active(config)) {
		/*CPRINTS("usb_power_deferred calls usb_enable_rx");*/
        	usb_enable_rx(config, USB_MAX_PACKET_SIZE);
	}
        
	//if (tx_fifo_is_ready(config) && usb_power_write(config));
	//	return;

	/*CPRINTS("usb_power_deferred exit");*/
}


/* Tx/IN interrupt handler */
void usb_power_tx(struct usb_power_config const *config)
{
	struct dwc_usb_ep *ep = config->ep;
	uint32_t dieptsiz = GR_USB_DIEPTSIZ(config->endpoint);

        /* Wake up the Tx FIFO handler */
        /*hook_call_deferred(&tx_fifo_handler_data, 0);*/

        /* clear the Tx/IN interrupts */
        GR_USB_DIEPINT(config->endpoint) = 0xffffffff;

	/* Let's assume this is actually true. */
	ep->in_packets = 0;
	ep->in_pending = dieptsiz & GC_USB_DIEPTSIZ1_XFERSIZE_MASK;

	hook_call_deferred(config->deferred, 0);
}

/* Rx/OUT interrupt handler */
void usb_power_rx(struct usb_power_config const *config)
{
	struct dwc_usb_ep *ep = config->ep;

	if (GR_USB_DOEPCTL(config->endpoint) & DXEPCTL_EPENA)
		return;

	/* Bytes received decrement DOEPTSIZ XFERSIZE */
	if (GR_USB_DOEPINT(config->endpoint) & DOEPINT_XFERCOMPL) {
		ep->out_pending = 
			ep->max_packet - 
			(GR_USB_DOEPTSIZ(config->endpoint) &
			 GC_USB_DOEPTSIZ1_XFERSIZE_MASK);
	}

	/* Wake up the Rx FIFO handler */
	/*hook_call_deferred(&rx_fifo_handler_data, 0);*/

	/* clear the RX/OUT interrupts */
	GR_USB_DOEPINT(config->endpoint) = 0xffffffff;

	hook_call_deferred(config->deferred, 0);
}

void usb_power_reset(struct usb_power_config const *config)
{
	config->ep->out_databuffer = config->state->rx_buf;
	config->ep->out_databuffer_max = sizeof(config->state->rx_buf);
	config->ep->in_databuffer = config->state->tx_buf;
	config->ep->in_databuffer_max = sizeof(config->state->tx_buf);

        GR_USB_DOEPCTL(config->endpoint) = DXEPCTL_MPS(USB_MAX_PACKET_SIZE) |
                DXEPCTL_USBACTEP | DXEPCTL_EPTYPE_BULK |
                DXEPCTL_CNAK | DXEPCTL_EPENA;
        GR_USB_DIEPCTL(config->endpoint) = DXEPCTL_MPS(USB_MAX_PACKET_SIZE) |
                DXEPCTL_USBACTEP | DXEPCTL_EPTYPE_BULK |
                DXEPCTL_TXFNUM(config->endpoint);
        GR_USB_DAINTMSK |= DAINT_INEP(config->endpoint) |
                           DAINT_OUTEP(config->endpoint);

        /* Flush any queued data */
        /*hook_call_deferred(&tx_fifo_handler_data, 0);*/
        /*hook_call_deferred(&rx_fifo_handler_data, 0);*/

        usb_enable_rx(config, USB_MAX_PACKET_SIZE);
}




#define LOWB(hword) ((hword) & 0xff)
#define HIGHB(hword) ((hword) >> 8)
#define BTOH(low, high) ((low) + (((uint16_t)(high) << 8)))
#define BTOI(ll, lh, hl, hh) (HTOI(BTOH(ll, lh), BTOH(hl, hh)))
#define HTOI(low, high) ((low) + (((uint32_t)(high) << 16)))

#define SWIZZLE(hword)	((LOWB(hword) << 8) | HIGHB(hword))

static int usb_power_write_line(struct usb_power_config const *config)
{
	struct usb_power_state *state = config->state;
	struct dwc_usb_ep *ep = config->ep;
	void *address = ep->in_databuffer;
	struct usb_power_report *r =
		config->state->reports + config->state->reports_tail;
	/* status + size + timestamps + power list */
	size_t bytes = 6 + 2*config->state->ina_count;

	if (sizeof(struct usb_power_report) != 64)
		CPRINTS("usb_power_write_line: OMG %d != 64!", sizeof(struct usb_power_report));

	if (config->state->reports_head != config->state->reports_tail) {
		/*CPRINTS("usb_power_write_line idx=%d", config->state->reports_tail);*/
		memcpy((void*)address, r, bytes);

		state->reports_tail = (state->reports_tail + 1) % USB_POWER_MAX_CACHED;

		usb_enable_tx(config, bytes);
		return bytes;
	}

	/*CPRINTS("usb_power_write_line: no data");*/
	return 0;
}


static int usb_power_state_reset(struct usb_power_config const *config)
{
	struct usb_power_state *state = config->state;

	state->state = USB_POWER_STATE_OFF;
	state->reports_head = 0;
	state->reports_tail = 0;
	CPRINTS("[RESET] STATE -> OFF");
	return USB_POWER_SUCCESS;
}


static int usb_power_state_stop(struct usb_power_config const *config)
{
	struct usb_power_state *state = config->state;

	/* Only a valid transition from CAPTURING */
	if (state->state != USB_POWER_STATE_CAPTURING) {
		CPRINTS("[STOP] Error not capturing.");
		return USB_POWER_NOT_CAPTURING;
	}

	state->state = USB_POWER_STATE_SETUP;
	state->reports_head = 0;
	state->reports_tail = 0;
	CPRINTS("[STOP] STATE: CAPTURING -> SETUP");
	return USB_POWER_SUCCESS;
}



static int usb_power_state_start(struct usb_power_config const *config, 
				 union usb_power_command_data *cmd, int count)
{
	struct usb_power_state *state = config->state;
	int integration_ms = cmd->start.integration_ms;

	if (state->state != USB_POWER_STATE_SETUP) {
		CPRINTS("[START] Error not setup.");
		return USB_POWER_NOT_SETUP;
	}

	if (count != 6) {
		CPRINTS("[START] Error count %d is not 6", (int)count);
		return USB_POWER_READ_SIZE;
	}

	if (integration_ms == 0) {
		CPRINTS("[START] integration_ms cannot be 0");
		return USB_POWER_UNKNOWN_ERROR;
	}

	state->integration_ms = integration_ms;
	usb_power_init_inas(config);

	state->state = USB_POWER_STATE_CAPTURING;
	CPRINTS("[START] STATE: SETUP -> CAPTURING %dms", integration_ms);
	hook_call_deferred(config->deferred_cap, state->integration_ms * 1000);
	/*hook_call_deferred(config->deferred_cap, 0);*/
	return USB_POWER_SUCCESS;
}


static int usb_power_state_addina(struct usb_power_config const *config, 
				 union usb_power_command_data *cmd, int count)
{
	struct usb_power_state *state = config->state;
	struct usb_power_ina_cfg *ina;

	/* Only valid from OFF or SETUP */
	if ((state->state != USB_POWER_STATE_OFF) &&
	    (state->state != USB_POWER_STATE_SETUP)) {
		CPRINTS("[ADDINA] Error incorrect state.");
		return USB_POWER_NOT_SETUP;
	}

	if (count != 14) {
		CPRINTS("[ADDINA] Error count %d is not 14", (int)count);
		return USB_POWER_READ_SIZE;
	}

	if (state->ina_count >= USB_POWER_MAX_READ_COUNT) {
		CPRINTS("[ADDINA] Error INA list full");
		return  USB_POWER_FULL;
	}

	/* Transition to SETUP state if necessary and clear INA data */
	if (state->state == USB_POWER_STATE_OFF) {
		state->state = USB_POWER_STATE_SETUP;
		state->ina_count = 0;
		CPRINTS("[ADDINA] STATE: OFF -> SETUP");
	} else {
		CPRINTS("[ADDINA] STATE: SETUP -> SETUP");
	}

	/* Select INA to configure */
	ina = state->ina_cfg + state->ina_count;

	ina->port = cmd->addina.port;
	ina->addr = (cmd->addina.addr) << 1; // 7 to 8 bit addr.
	ina->extender_mode = 0;
	ina->extended_addr = 0;
	ina->mv = cmd->addina.mv;
	ina->rs = cmd->addina.rs;
	state->ina_count += 1;
	CPRINTS("[ADDINA] port:%d addr:0x%02x mv:%d rs:%d count:%d",
	        ina->port, ina->addr, ina->mv, ina->rs, state->ina_count);
	return USB_POWER_SUCCESS;
}

static int usb_power_read(struct usb_power_config const *config)
{
	/*
	 * If there is a USB packet waiting we process it and generate a
	 * response.
	 */
	uint8_t count		= rx_fifo_is_ready(config);
	uint8_t result		= USB_POWER_SUCCESS;
	union usb_power_command_data *cmd = 
		(union usb_power_command_data*)config->ep->out_databuffer;

	struct usb_power_state *state = config->state;
	struct dwc_usb_ep *ep = config->ep;

	/* Acknowledge that we have eaten this data */
        ep->out_pending = 0;

	if (count < 2)
		return EC_ERROR_INVAL;

        /*CPRINTS("usb_power_read c:0x%04x", cmd->command);*/

	// State machine.
	if (cmd->command == USB_POWER_CMD_RESET) {
		result = usb_power_state_reset(config);
	} else if (cmd->command == USB_POWER_CMD_STOP) {
		result = usb_power_state_stop(config);
	} else if (cmd->command == USB_POWER_CMD_START) {
		result = usb_power_state_start(config, cmd, count);
	} else if (cmd->command == USB_POWER_CMD_ADDINA) {
		result = usb_power_state_addina(config, cmd, count);
	} else if (cmd->command == USB_POWER_CMD_NEXT) {
		if (state->state == USB_POWER_STATE_CAPTURING) {
			int ret;

			/*CPRINTS("[CAP] Get Next");*/
			ret = usb_power_write_line(config);
			if (ret)
				return EC_SUCCESS;
			result = USB_POWER_BUSY;
		} else {
			CPRINTS("[STOP] Error not capturing.");
			result = USB_POWER_NOT_CAPTURING;
		}
	} else {
		CPRINTS("[ERROR] Unknown command 0x%04x", (int)cmd->command);
		result = USB_POWER_UNKNOWN_ERROR;
	}

	/* Return result code if applicable. */	
	usb_power_map_error(0);
	ep->in_databuffer[0] = result;
	usb_enable_tx(config, 1);

	/*CPRINTS("usb_power_read return 0x%02x", (int)ep->in_databuffer[0]);*/
	return EC_SUCCESS;
}


#define INA231_REG_CONF 0
#define INA231_REG_RSHV 1
#define INA231_REG_BUSV 2
#define INA231_REG_PWR  3
#define INA231_REG_CURR 4
#define INA231_REG_CAL  5
#define INA231_REG_EN   6


#define INA231_CONF_AVG(val)		(((int)(val & 0x7)) << 9)
#define INA231_CONF_BUS_TIME(val)	(((int)(val & 0x7)) << 6)
#define INA231_CONF_SHUNT_TIME(val)	(((int)(val & 0x7)) << 3)
#define INA231_CONF_MODE(val)		(((int)(val & 0x7)) << 0)
#define INA231_MODE_OFF			0x0
#define INA231_MODE_SHUNT		0x5
#define INA231_MODE_BUS			0x6
#define INA231_MODE_BOTH		0x7

uint16_t ina2xx_read(uint8_t port, uint8_t addr, uint8_t reg)
{
        int res;
        int val;

        res = i2c_read16(port, addr, reg, &val);
        if (res) {
                CPRINTS("INA2XX I2C read failed");
                return 0x0bad;
        }
        return (val >> 8) | ((val & 0xff) << 8);
}

int ina2xx_write(uint8_t port, uint8_t addr, uint8_t reg, uint16_t val)
{
        int res;
        uint16_t be_val = (val >> 8) | ((val & 0xff) << 8);

        res = i2c_write16(port, addr, reg, be_val);
        if (res)
                CPRINTS("INA2XX I2C write failed");
        return res;
}


/* Lazy log implementation */
static int ilog2(int val) {
	int l = 0;
	val = val >> 1;
	while (val > 0) {
		val = val >> 1;
		l++;
	}
	return l;
}

static int usb_power_init_inas(struct usb_power_config const *config)
{
	struct usb_power_state *state = config->state;
	int i;

	/* Set up i2c mux. */
	i2c_write8(0, 0xe0, 0, 5);

	if (state->state != USB_POWER_STATE_SETUP) {
		CPRINTS("[ERROR] usb_power_init_inas while not SETUP");
		return -1;
	}
	for (i = 0; i < state->ina_count; i++) {
		int value;
		int actual;
		int ret;
		int shunt_time = 0x4;
		int avg = 0;
		struct usb_power_ina_cfg *ina = state->ina_cfg + i;

		{
		int conf, cal;

		conf = ina2xx_read(ina->port, ina->addr, INA231_REG_CONF);
		cal = ina2xx_read(ina->port, ina->addr, INA231_REG_CAL);
		CPRINTS("[CAP] %d (%d,0x%02x): conf:%x, cal:%x", i, ina->port, ina->addr, conf, cal);
		}
		if (state->integration_ms < 0) {
			shunt_time = 0; // 140uS
			shunt_time = 1; // 204uS
			shunt_time = 2; // 332uS
			shunt_time = 3; // 588uS
			avg = 0;
		} else {
			shunt_time = MIN(7, ilog2(state->integration_ms));
			avg = MIN(7, ilog2(state->integration_ms/8) / 2);
		}
		CPRINTS("[CAP] shunt:%d avg:%d ms:%d", shunt_time, avg, state->integration_ms);

		// INA231
		// Calibration register
		// CurrentLSB = uA per div = 80mV / (Rsh * 2^15)
		// CurrentLSB uA = 80000000nV / (Rsh mOhm * 0x8000)
		ina->scale = 80000000 / (ina->rs * 0x8000);
		// CAL = .00512 / (CurrentLSB * Rsh)
		// CAL = 5120000 / (uA * mOhm)
		value = 5120000 / (ina->scale * ina->rs);
		ret = ina2xx_write(ina->port, ina->addr, INA231_REG_CAL, value);
		if (ret != EC_SUCCESS)
			CPRINTS("[CAP] usb_power_init_inas CAL FAIL: %d", ret);
		actual = ina2xx_read(ina->port, ina->addr, INA231_REG_CAL);
		CPRINTS("[CAP] scale: %d uA/div, %d uW/div, cal:%x act:%x",
			ina->scale, ina->scale*25, value, actual);

		// Conversion time, shunt + bus, set average.
		value = INA231_CONF_MODE(INA231_MODE_BOTH) |
			INA231_CONF_SHUNT_TIME(shunt_time) |
			INA231_CONF_BUS_TIME(shunt_time) |
			INA231_CONF_AVG(avg);
		ret = ina2xx_write(ina->port, ina->addr, INA231_REG_CONF, value);
		if (ret != EC_SUCCESS)
			CPRINTS("[CAP] usb_power_init_inas CONF FAIL: %d", ret);
		actual = ina2xx_read(ina->port, ina->addr, INA231_REG_CONF);
		CPRINTS("[CAP] %d (%d,0x%02x): conf:%x, act:%x", i, ina->port, ina->addr, value, actual);
	}
	return 0;
}

static int usb_power_get_samples(struct usb_power_config const *config) {
	uint64_t time = get_time().val;
	struct usb_power_state *state = config->state;
	struct usb_power_report *r = state->reports + state->reports_head;
	int i;
	int overflow = 0;

	// TODO(nsanders): Would we prefer to evict oldest?
	if (((state->reports_head + 1) % USB_POWER_MAX_CACHED) == state->reports_tail) {
		overflow = 1;
		// Goodbye oldest entry.
		state->reports_tail = (state->reports_tail + 1) % USB_POWER_MAX_CACHED;
	}

	r->status = USB_POWER_SUCCESS;
	r->size = state->ina_count;
	r->timestamp = time & 0xffffffff;
	for (i = 0; i < state->ina_count; i++) {
		int power;
		int ret;
		struct usb_power_ina_cfg *ina = state->ina_cfg + i;

		// INA231
		power = ina2xx_read(ina->port, ina->addr, INA231_REG_PWR);
		r->power[i] = power;
		if (ret != EC_SUCCESS)
			CPRINTS("[CAP] usb_power_get_samples FAIL: %d", ret);
#if 0
		{
		int current;
		int voltage;
		int bvoltage;
		voltage = ina2xx_read(ina->port, ina->addr, INA231_REG_RSHV);
		bvoltage = ina2xx_read(ina->port, ina->addr, INA231_REG_BUSV);
		current = ina2xx_read(ina->port, ina->addr, INA231_REG_CURR);
		}
		{
		int uV = ((int)voltage * 25) / 10;
		int mV = ((int)bvoltage * 125) / 100;
		int uA = (uV * 1000) / ina->rs;
		int CuA = ((int)current * ina->scale);
		int uW = ((int)power * ina->scale*25);
		CPRINTS("[CAP] %d (%d,0x%02x): %dmV / %dmO = %dmA",
                        i, ina->port, ina->addr, uV/1000, ina->rs, uA/1000);
		CPRINTS("[CAP] %duV %dmV %duA %dCuA %duW v:%04x, b:%04x, p:%04x",
                        uV, mV, uA, CuA, uW, voltage, bvoltage, power);
		}
#endif
	}

	// Mark this slot as used.
	state->reports_head = (state->reports_head + 1) % USB_POWER_MAX_CACHED;

	if (overflow)
		return USB_POWER_OVERFLOW;
	else
		return 0;
}


void usb_power_deferred_cap(struct usb_power_config const *config)
{
	int ret;

	// Exit if we have stopped capturing in the meantime.
	if (config->state->state != USB_POWER_STATE_CAPTURING)
		return;

	/*CPRINTS("[CAP] usb_power_deferred_cap");*/
	ret = usb_power_get_samples(config);
	if (ret == USB_POWER_OVERFLOW) {
		CPRINTS("[CAP] usb_power_deferred_cap: OVERFLOW");
		usb_power_state_reset(config);
		return;
	}
		
		
	//hook_call_deferred(config->deferred, 0);

	// Double check that we have stopped capturing.
	if (config->state->state == USB_POWER_STATE_CAPTURING)
		hook_call_deferred(config->deferred_cap, config->state->integration_ms * 1000);
}

