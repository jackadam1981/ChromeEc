/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/*
 * i2c-pseudo.h - I2C userspace adapter char device interface
 *
 * Copyright 2023 Google LLC
 */

#ifndef _UAPI_LINUX_I2C_PSEUDO_H
#define _UAPI_LINUX_I2C_PSEUDO_H

#include <linux/types.h>
#include <linux/compiler.h>

/* /dev/i2c-pseudo ioctl commands */
/*
 * Create the I2C adapter device,
 * arg is pointer to struct i2cp_ioctl_start_arg.
 * May only be used once per fd.
 */
#define I2CP_IOCTL_START		0x0701
/*
 * Take in the next requested I2C transfer transaction,
 * arg is pointer to struct i2cp_ioctl_xfer_arg, which will have its fields
 * overwritten upon successful return of this ioctl, at which point the caller
 * owns all resulting memory allocations and is responsible for freeing them,
 * including nested buffers pointed to from each struct i2c_msg.
 * May only be used between I2CP_IOCTL_START and I2CP_IOCTL_SHUTDOWN.
 * Must be followed by I2CP_IOCTL_XFER_REPLY.
 * If no I2C transfer requests are pending will either block or set errno
 * (to EAGAIN or EWOULDBLOCK) based on O_NONBLOCK flag.
 */
#define I2CP_IOCTL_XFER_REQ		0x0702
/*
 * Reply to the I2C transfer request from I2CP_IOCTL_XFER_REQ,
 * arg is pointer to struct i2cp_ioctl_xfer_arg, which is expected to typically
 * be the same instance at the same address as received by I2CP_IOCTL_XFER_REQ,
 * but does not have to be, though unless an error is indicated the contents
 * must match, with the struct i2c_msg buffers for all reads filled in to their
 * full length.
 * May only be used once after each I2CP_IOCTL_XFER_REQ.
 */
#define I2CP_IOCTL_XFER_REPLY		0x0703
// TODO: document I2CP_IOCTL_GET_COUNTERS
#define I2CP_IOCTL_GET_COUNTERS		0x0704
/*
 * Unblock all pseudo controller I/O and refuse further I2C transfer requests.
 * Unused arg must be 0 or NULL and is reserved for possible future use.
 * Use of this is optional for userspace convenience to unblock any threads
 * waiting/polling for I2C transfer requests.
 */
#define I2CP_IOCTL_SHUTDOWN		0x0705

struct i2cp_ioctl_start_output {
	/* I2C adapter number from I2C subsystem */
	__u64 adapter_num;
	/*
	 * I2C pseudo adapter ID.
	 * Will never be reused while i2c-pseudo module is loaded unless an
	 * internal counter wraps.
	 * Guaranteed unique among active pseudo controllers.
	 * I2C clients can use this to track specific instances of
	 * pseudo adapters in the face of I2C adapter number reuse.
	 */
	__u64 pseudo_id;
};

struct i2cp_ioctl_start_arg {
	/* output fields (kernel -> userspace controller), must be first item */
	struct i2cp_ioctl_start_output output;
	/*
	 * Bitmask of I2C_FUNC_* flags.
	 * I2C_FUNC_I2C is currently mandatory (this may change).
	 * I2C_FUNC_SMBUS_EMUL or any subset is optional.
	 * No other functionality flags are supported yet.
	 * Additional flags may be supported in the future.
	 * Setting unsupported flags is an error.
	 */
	__u32 functionality;
	/* I2C transaction timeout in milliseconds, 0 for default timeout */
	__u32 timeout_ms;
	/* I2C adapter name suffix, NULL for no suffix. Null-terminated. */
	const char __user *suffix;
};

struct i2cp_ioctl_xfer_counters {
	/* I2CP_IOCTL_XFER_REPLY received */
	__u64 controller_replied;
	/* kernel bug */
	__u64 unknown_failure;
	/* master_xfer after I2CP_IOCTL_SHUTDOWN */
	__u64 after_shutdown;
	/* master_xfer with num_msgs >= max_msgs_per_xfer */
	__u64 too_many_msgs;
	/* master_xfer with sum(i2c_msg.len) > max_total_data_per_xfer */
	__u64 too_much_data;
	/* interrupted before I2CP_IOCTL_XFER_REQ */
	__u64 interrupted_before_req;
	/* interrupted after I2CP_IOCTL_XFER_REQ before I2CP_IOCTL_XFER_REPLY */
	__u64 interrupted_before_reply;
	/* timed out before I2CP_IOCTL_XFER_REQ */
	__u64 timed_out_before_req;
	/* timed out after I2CP_IOCTL_XFER_REQ before I2CP_IOCTL_XFER_REPLY */
	__u64 timed_out_before_reply;
};

struct i2cp_ioctl_xfer_req_output {
	__u64 xfer_id;
	__u16 num_msgs;
};

struct i2cp_ioctl_xfer_req_arg {
	/* output fields (kernel -> userspace controller), must be first item */
	struct i2cp_ioctl_xfer_req_output output;
	struct i2c_msg __user *msgs;
	__u8 __user *data_buf;
	__u32 msgs_len;
	__u32 data_buf_len;
};

struct i2cp_ioctl_xfer_reply_arg {
	struct i2c_msg __user *msgs;
	__u64 xfer_id;
	__u16 num_msgs;
	__u16 error;
};

#endif /* _UAPI_LINUX_I2C_PSEUDO_H */
