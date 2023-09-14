=================
i2c-pseudo driver
=================

Usually I2C adapters are implemented in a kernel driver. It is also possible to
implement an adapter in userspace, through the /dev/i2c-pseudo interface. Load
module i2c-pseudo for this.

Use cases for userspace I2C adapters include:

- Using local I2C device drivers, particularly i2c-dev, with I2C busses on
  remote systems. For example, interacting with a Device Under Test (DUT)
  connected to a Linux host through a debug interface, or interacting with a
  remote host over a network.

- Implementing I2C device driver tests that are impractical with the i2c-stub
  module. For example, when simulating an I2C device where the value at a
  certain address must change on its own during the test sequence (not from the
  device driver writing to it).

This is not intended to replace kernel drivers for local I2C busses inside the
host machine.


Details
=======

Open /dev/i2c-pseudo and call ioctl ``I2CP_IOCTL_START`` to create a new
I2C adapter on the system. The adapter will live until its file descriptor is
closed. Multiple pseudo adapters can co-exist simultaneously, controlled by the
same or different userspace processes.

When an I2C device driver sends an I2C transaction to a pseudo adapter, the
transaction becomes available via ioctl ``I2CP_IOCTL_XFER_REQ``. If a reply is
issued via ioctl ``I2CP_IOCTL_XFER_REPLY`` before the adapter timeout expires,
that reply will be sent back to the I2C device driver.

By default ``I2CP_IOCTL_XFER_REQ`` will block indefinitely for an
I2C transaction request to come in from an I2C device driver. Use ``O_NONBLOCK``
for non-blocking behavior. Other ioctl commands are unaffected by ``O_NONBLOCK``
and never block indefinitely.

Polling is supported, in blocking or non-blocking mode. ``POLLIN`` indicates an
I2C transaction is available for ``I2CP_IOCTL_XFER_REQ``. ``POLLOUT`` indicates
``I2CP_IOCTL_XFER_REPLY`` may be valid. ``POLLHUP`` indicates
``I2CP_IOCTL_SHUTDOWN`` has been called. (The current implementation always
indicates ``POLLOUT`` between ``I2CP_IOCTL_START`` and ``I2CP_IOCTL_SHUTDOWN``,
however this may become more precise in the future.)

While polling is functional in blocking mode, it cannot be used to avoid
blocking. If a pending I2C transaction request times out between receiving
``POLLIN`` and issuing ``I2CP_IOCTL_XFER_REQ`` the latter can block.
``O_NONBLOCK`` is needed to avoid this.

The ioctl ``I2CP_IOCTL_SHUTDOWN`` will unblock any pollers or blocked
``I2CP_IOCTL_XFER_REQ``, as a convenience for a multi-threaded or multi-process
program that wants to exit. Use of ``I2CP_IOCTL_SHUTDOWN`` is purely optional.

See ``include/uapi/linux/i2c-pseudo.h`` for a detailed description of ioctl
args, usage, and behavior.


Example userspace controller code
=================================

In C, a simple exchange between i2c-pseudo and userspace might look like the
example below. Note that for brevity this lacks any ioctl error checking and
handling, which a real userspace controller implementation should have!

::

  #include <errno.h>

  #include <linux/i2c.h>
  #include <linux/i2c-pseudo.h>

  void start_adapter(int fd)
  {
  	struct i2cp_ioctl_start_arg start_arg = {
  		.functionality = I2C_FUNC_I2C,
  		.timeout_ms = 5000,
  		.suffix = "demo adapter",
  	};
  	ioctl(fd, I2CP_IOCTL_START, &start_arg);
  	printf("I2C adapter number: %llu\n", start_arg.output.adapter_num);
  	printf("I2C pseudo controller ID: %llu\n", start_arg.output.pseudo_id);
  }

  void handle_xfers(int fd)
  {
  	struct i2c_msg msgs[2];
  	__u8 data_buf[5];
  	struct i2cp_ioctl_xfer_req_arg req_arg = {
  		.msgs = &msgs,
  		.msgs_len = 2,
  		.data_buf = &data_buf,
  		.data_buf_len = 5,
  	};
  	struct i2cp_ioctl_xfer_reply_arg reply_arg = {
  		.msgs = &msgs,
  	};

  	/*
  	 * Expected I2C transfer:
  	 *   $ i2ctransfer "$adapter_num" w5@0x70 0xC2 0xFF=
  	 */
  	ioctl(fd, I2CP_IOCTL_XFER_REQ, &req_arg);
  	reply_arg.xfer_id = req_arg.output.xfer_id;
  	if (req_arg.output.num_msgs == 1 &&
  	    msgs[0].addr == 0x70 &&
  	    !(msgs[0].flags & I2C_M_RD) &&
  	    msgs[0].len == 5 &&
  	    !memcmp(msgs[0].buf, "\xC2\xFF\xFF\xFF\xFF", 5)) {
  		reply_arg.num_msgs = req_arg.output.num_msgs;
  		reply_arg.error = 0;
  	} else {
  		reply_arg.num_msgs = 0;
  		reply_arg.error = EINVAL;
  	}

  	/*
  	 * Expected I2C transfer:
  	 *   $ i2ctransfer "$adapter_num" w3@0x20 0x1A+ r2
  	 *   0x33 0x55
  	 */
  	ioctl(fd, I2CP_IOCTL_XFER_REQ, &req_arg);
  	reply_arg.xfer_id = req_arg.output.xfer_id;
  	if (req_arg.output.num_msgs == 2 &&
  	    msgs[0].addr == 0x20 &&
  	    !(msgs[0].flags & I2C_M_RD) &&
  	    msgs[0].len == 3 &&
  	    !memcmp(msgs[0].buf, "\x1A\x1B\x1C", 3) &&
  	    msgs[1].addr == 0x20 &&
  	    msgs[1].flags & I2C_M_RD &&
  	    msgs[1].len == 2) {
  		memcpy(msgs[1].buf, "\x33\x55", 2);
  		reply_arg.num_msgs = req_arg.output.num_msgs;
  		reply_arg.error = 0;
  	} else {
  		reply_arg.num_msgs = 0;
  		reply_arg.error = EINVAL;
  	}
  }

  void run(void)
  {
  	int fd = open("/dev/i2c-pseudo", O_RDWR);
  	/* Create the I2C adapter. */
  	start_adapter(fd);
  	/* Expect and reply to hardcoded demonstration I2C transfer requests. */
  	handle_xfers(fd);
  	/* Destroy the I2C adapter. */
  	close(fd);
  }
