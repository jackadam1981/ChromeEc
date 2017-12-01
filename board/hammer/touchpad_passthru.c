
#include "touchpad_passthru.h"
#include "usb-isochronous.h"
#include "board.h"
#include "link_defs.h"

#ifdef USB_TOUCHPAD_PASSTHRU

/* Console output macro */
#define CPRINTF(format, args...) cprintf(CC_USB, format, ## args)
#define CPRINTS(format, args...) cprints(CC_USB, format, ## args)

/* declare interface */
USB_ISOCHRONOUS_CONFIG_FULL(usb_touchpad_passthru_config,
			    USB_IFACE_TOUCHPAD_PASSTHRU,
			    USB_CLASS_VENDOR_SPEC,
			    0,  /* subclass */
			    0,  /* protocol */
			    0,  /* interface name */
			    USB_EP_TOUCHPAD_PASSTHRU,
			    USB_MAX_PACKET_SIZE)

void touchpad_passthru_generate_event()
{
	static uint8_t cc = 0;
	uint8_t buf[16];
	int i;
	size_t written_count;

	CPRINTS("STIMIM: %s called\n", __func__);
	for (i = 0; i < 16; ++ i) {
		buf[i] = (cc ++);
	}

	written_count = usb_isochronous_write_queue(
			&usb_touchpad_passthru_config,
			(void *) buf,
			16);
	CPRINTS("STIMIM: written_count = %u\n", (unsigned) written_count);
}

#endif  /* USB_IFACE_TOUCHPAD_PASSTHRU */
