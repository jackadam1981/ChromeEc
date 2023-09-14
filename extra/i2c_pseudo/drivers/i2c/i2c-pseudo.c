// SPDX-License-Identifier: GPL-2.0
/*
 * i2c-pseudo.c - userspace I2C adapters
 *
 * Copyright 2023 Google LLC
 *
 * This allows for userspace implementations of functionality such as tunneling
 * I2C through another communication channel, or mocking of real I2C devices for
 * driver tests.
 */

// TODO: trim imports
#include <linux/build_bug.h>
#include <linux/cdev.h>
#include <linux/compat.h>
#include <linux/completion.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/time64.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <uapi/linux/i2c-pseudo.h>

/* Maximum number of concurrent userspace I2C adapters. */
static unsigned short max_adapters = 128;
module_param(max_adapters, ushort, 0444);

/* Maximum number of I2C messages per master_xfer transaction. */
static unsigned short max_msgs_per_xfer = 128;
module_param(max_msgs_per_xfer, ushort, 0444);

/* Maximum total size of all buffers per master_xfer transaction. */
static unsigned short max_total_data_per_xfer = USHRT_MAX;
module_param(max_total_data_per_xfer, ushort, 0444);

/* Default I2C transaction timeout, in milliseconds. 0 for subsystem default. */
static unsigned int default_timeout_ms = 3 * MSEC_PER_SEC;
module_param(default_timeout_ms, uint, 0444);

// TODO: Merge struct i2cp_counters into struct i2cp_device?
/* This tracks all I2C pseudo adapters. */
struct i2cp_counters {
	/* This must be held while accessing any fields. */
	struct mutex lock;
	unsigned int count;
	/*
	 * This is used to make a strong attempt at avoiding ID reuse,
	 * especially during the lifetime of a userspace i2c-dev client. This
	 * can wrap by design, and thus makes no perfect guarantees.
	 */
	u64 next_ctrlr_id;  /* same type as struct i2cp_controller.id field */
	/* list of struct i2cp_controller */
	struct list_head all_controllers;
};

// TODO: consider consolidating i2cp_{class,cdev_{num,count},device} into one struct
static struct class *i2cp_class;
static dev_t i2cp_cdev_num;
static const unsigned int i2cp_cdev_count = 1;

struct i2cp_device {
	struct i2cp_counters counters;
	struct cdev cdev;
	struct device device;
};

static struct i2cp_device *i2cp_device;

/*
 * All values must be >= 0. This should not contain any error values.
 *
 * The state for a new controller must have a zero value, so that
 * zero-initialized memory results in the correct default value.
 */
enum i2cp_state {
	I2CP_STATE_NEW = 0,
	I2CP_STATE_WAIT_FOR_XFER,
	I2CP_STATE_WAIT_FOR_REQ,
	I2CP_STATE_WAIT_FOR_REPLY,
	I2CP_STATE_XFER_RETURN,
	I2CP_STATE_RETURN_THEN_SHUTDOWN,
	I2CP_STATE_SHUTDOWN,
};

struct i2cp_xfer {
};

struct i2cp_controller {
	u64 id;
	u32 functionality;
	struct i2c_adapter i2c_adapter;
	wait_queue_head_t poll_wait_queue;
	/*
	 * This is for i2cp_device->counters.all_controllers list entry,
	 * only modify while holding i2cp_device->counters.lock.
	 */
	struct list_head all_ctrlrs_item;

	/* Fields below this lock are protected by it. */
	struct mutex lock;
	enum i2cp_state state;
	struct i2cp_ioctl_xfer_counters xfer_counters;
	struct completion i2c_xfer_queued;
	struct completion i2c_xfer_done;
	u64 i2c_xfer_id;
	struct i2c_msg *i2c_xfer_msgs;
	u16 i2c_xfer_num_msgs;
	int i2c_xfer_ret;
};

static bool i2cp_sum_buf_lens(const struct i2c_msg *msgs, size_t num_msgs,
	size_t *ret)
{
	for (size_t i = 0; i < num_msgs; ++i)
		// TODO: blah
		//if (check_add_overflow(msgs[i].len, *ret, ret))
		if (unlikely(__builtin_add_overflow(msgs[i].len, *ret, ret)))
			return false;
	return (*ret <= max_total_data_per_xfer);
}

static inline bool i2cp_check_buf_lens(const struct i2c_msg *msgs,
	size_t num_msgs)
{
	size_t total_buf_len;
	return i2cp_sum_buf_lens(msgs, num_msgs, &total_buf_len);
}

static int i2cp_adapter_master_xfer(struct i2c_adapter *adap,
	struct i2c_msg *msgs, int num)
{
	int ret = -ENOTRECOVERABLE;
	long wait_ret;
	struct i2cp_controller *pdata;
	pdata = adap->algo_data;
	mutex_lock(&pdata->lock);
	if (unlikely(num <= 0)) {
		++pdata->xfer_counters.unknown_failure;
		ret = -EINVAL;
		goto unlock;
	}
	if ((unsigned int)num > min(max_msgs_per_xfer, U16_MAX)) {
		++pdata->xfer_counters.too_many_msgs;
		ret = -EMSGSIZE;
		goto unlock;
	}
	switch (pdata->state) {
	case I2CP_STATE_WAIT_FOR_XFER:
		break;
	case I2CP_STATE_SHUTDOWN:
		++pdata->xfer_counters.after_shutdown;
		ret = -ESHUTDOWN;
		goto unlock;
	default:
		++pdata->xfer_counters.unknown_failure;
		goto unlock;
	}
	if (!i2cp_check_buf_lens(msgs, num)) {
		++pdata->xfer_counters.too_much_data;
		ret = -ENOBUFS;
		goto unlock;
	}
	++pdata->i2c_xfer_id;
	pdata->i2c_xfer_msgs = msgs;
	pdata->i2c_xfer_num_msgs = num;
	pdata->i2c_xfer_ret = 0;
	pdata->state = I2CP_STATE_WAIT_FOR_REQ;
	complete(&pdata->i2c_xfer_queued);
	mutex_unlock(&pdata->lock);

	wake_up_interruptible_sync_poll(&pdata->poll_wait_queue, POLLIN);
	wait_ret = wait_for_completion_killable_timeout(
		&pdata->i2c_xfer_done, adap->timeout);

	mutex_lock(&pdata->lock);
	/* dequeue if i2cp_cdev_ioctl_xfer_req() never did */
	(void)try_wait_for_completion(&pdata->i2c_xfer_queued);
	/* dequeue if i2cp_cdev_ioctl_xfer_reply() queued after our timeout */
	(void)try_wait_for_completion(&pdata->i2c_xfer_done);
	switch (pdata->state) {
	case I2CP_STATE_XFER_RETURN:
		pdata->state = I2CP_STATE_WAIT_FOR_XFER;
		++pdata->xfer_counters.controller_replied;
		ret = pdata->i2c_xfer_ret;
		goto unlock;
	case I2CP_STATE_RETURN_THEN_SHUTDOWN:
		pdata->state = I2CP_STATE_SHUTDOWN;
		++pdata->xfer_counters.controller_replied;
		ret = pdata->i2c_xfer_ret;
		goto unlock;
	case I2CP_STATE_WAIT_FOR_REQ:
		if (wait_ret == 0)
			++pdata->xfer_counters.timed_out_before_req;
		else
			++pdata->xfer_counters.interrupted_before_req;
		break;
	case I2CP_STATE_WAIT_FOR_REPLY:
		if (wait_ret == 0)
			++pdata->xfer_counters.timed_out_before_reply;
		else
			++pdata->xfer_counters.interrupted_before_reply;
		break;
	case I2CP_STATE_SHUTDOWN:
		++pdata->xfer_counters.after_shutdown;
		ret = -ESHUTDOWN;
		goto unlock;
	default:
		++pdata->xfer_counters.unknown_failure;
		goto unlock;
	}
	pdata->state = I2CP_STATE_WAIT_FOR_XFER;
	if (wait_ret == 0)
		ret = -ETIMEDOUT;
	else if (wait_ret == -ERESTARTSYS)
		ret = -EINTR;
 unlock:
	mutex_unlock(&pdata->lock);
	return ret;
}

static u32 i2cp_adapter_functionality(struct i2c_adapter *adap)
{
	struct i2cp_controller *pdata;
	pdata = container_of(adap, struct i2cp_controller, i2c_adapter);
	return pdata->functionality;
}

static const struct i2c_algorithm i2cp_algorithm = {
	.master_xfer = i2cp_adapter_master_xfer,
	.functionality = i2cp_adapter_functionality,
};

/* this_pseudo->counters.lock must _not_ be held when calling this. */
static void i2cp_remove_from_counters(struct i2cp_controller *pdata,
	struct i2cp_device *this_pseudo)
{

	mutex_lock(&this_pseudo->counters.lock);
	list_del(&pdata->all_ctrlrs_item);
	--this_pseudo->counters.count;
	mutex_unlock(&this_pseudo->counters.lock);
}

static int i2cp_cdev_open(struct inode *inodep, struct file *filep)
{
	int ret;
	u64 ctrlr_id;
	struct i2cp_controller *pdata;
	struct i2cp_controller *other_ctrlr;
	struct i2cp_device *this_pseudo;

	// TODO: Is there any way to find this through @inodep?
	//this_pseudo = i2cp_device;
	this_pseudo = container_of(inodep->i_cdev, struct i2cp_device, cdev);
	// TODO: delete this
	if (this_pseudo != i2cp_device) return -ENOTRECOVERABLE;  /* bug */

	/* I2C pseudo adapter controllers are not seekable. */
	stream_open(inodep, filep);
	/* Refuse fsnotify events. Modeled after /dev/ptmx implementation. */
	filep->f_mode |= FMODE_NONOTIFY;

	/* Allocate the I2C adapter. */
	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	init_waitqueue_head(&pdata->poll_wait_queue);
	mutex_init(&pdata->lock);
	init_completion(&pdata->i2c_xfer_queued);
	init_completion(&pdata->i2c_xfer_done);

	mutex_lock(&this_pseudo->counters.lock);

	if (this_pseudo->counters.count >= max_adapters) {
		mutex_unlock(&this_pseudo->counters.lock);
		ret = -ENOSPC;
		goto fail_after_pdata_alloc;
	}

	// TODO: move pdata->id or whole this_pseudo->counters initialization to separate function (for readability)
	/* Find the next available pseudo controller ID. */
	for (ctrlr_id = this_pseudo->counters.next_ctrlr_id;;) {
		/* Determine whether ctrlr_id is already in use. */
		list_for_each_entry(other_ctrlr,
			            &this_pseudo->counters.all_controllers,
		                    all_ctrlrs_item)
			if (ctrlr_id == other_ctrlr->id)
				goto id_already_in_use;
		break;
 id_already_in_use:
		/* Increment ctrlr_id, and check for wrapping. */
		if (++ctrlr_id == this_pseudo->counters.next_ctrlr_id) {
			mutex_unlock(&this_pseudo->counters.lock);
			ret = -ENOSPC;
			goto fail_after_pdata_alloc;
		}
	}

	pdata->id = ctrlr_id;
	this_pseudo->counters.next_ctrlr_id = ctrlr_id + 1;
	++this_pseudo->counters.count;
	list_add_tail(&pdata->all_ctrlrs_item,
		&this_pseudo->counters.all_controllers);

	mutex_unlock(&this_pseudo->counters.lock);

	/* Initialize the I2C adapter. */
	pdata->i2c_adapter.owner = THIS_MODULE;
	pdata->i2c_adapter.class = I2C_CLASS_HWMON | I2C_CLASS_SPD;
	pdata->i2c_adapter.algo = &i2cp_algorithm;
	pdata->i2c_adapter.algo_data = pdata;
	pdata->i2c_adapter.dev.parent = &this_pseudo->device;
	ret = snprintf(pdata->i2c_adapter.name, sizeof(pdata->i2c_adapter.name),
		"I2C pseudo ID %llu", (unsigned long long)pdata->id);
	if (ret < 0)
		goto fail_after_counters_update;

	/* Return success. */
	filep->private_data = pdata;
	return 0;

 fail_after_counters_update:
	i2cp_remove_from_counters(pdata, this_pseudo);
 fail_after_pdata_alloc:
	kfree(pdata);
	return ret;
}

static int i2cp_cdev_release(struct inode *inodep, struct file *filep)
{
	bool adapter_was_added = false;
	struct i2cp_controller *pdata;
	struct i2cp_device *this_pseudo;

	pdata = filep->private_data;
	this_pseudo = container_of(pdata->i2c_adapter.dev.parent,
		struct i2cp_device, device);

	mutex_lock(&pdata->lock);
	filep->private_data = NULL;
	if (pdata->state != I2CP_STATE_NEW) {
		/*
		 * Defer deleting the adapter until after releasing
		 * pdata->state. This avoids deadlocking with any
		 * overlapping i2cp_adapter_master_xfer() calls, which also
		 * acquire the lock in order to check the state.
		 */
		adapter_was_added = true;
		pdata->state = (pdata->state == I2CP_STATE_XFER_RETURN) ?
			I2CP_STATE_RETURN_THEN_SHUTDOWN : I2CP_STATE_SHUTDOWN;
	}
	mutex_unlock(&pdata->lock);

	/* wake up any buggy pollers */
	wake_up_interruptible_all(&pdata->poll_wait_queue);
	/* wake up blocked master_xfer */
	complete_all(&pdata->i2c_xfer_done);
	/* wake up blocked I2CP_IOCTL_XFER_REQ */
	complete_all(&pdata->i2c_xfer_queued);

	if (adapter_was_added)
		i2c_del_adapter(&pdata->i2c_adapter);

	i2cp_remove_from_counters(pdata, this_pseudo);
	kfree(pdata);
	return 0;
}

static long i2cp_set_suffix(const char __user *user_suffix,
	struct i2cp_controller *pdata)
{
	long ret;
	char suffix[sizeof(pdata->i2c_adapter.name)];
	BUILD_BUG_ON(sizeof(suffix) > INT_MAX);
	ret = strncpy_from_user(suffix, user_suffix, sizeof(suffix));
	if (ret < 0)
		return ret;
	return snprintf(
		pdata->i2c_adapter.name, sizeof(pdata->i2c_adapter.name),
		"I2C pseudo ID %llu %.*s", (unsigned long long)pdata->id,
		(int)ret, suffix);
}

static long i2cp_cdev_ioctl_start(struct file *filep, unsigned long arg)
{
	long ret;
	struct i2cp_ioctl_start_arg __user *user_arg;
	struct i2cp_ioctl_start_arg arg_copy;
	struct i2cp_controller *pdata;

	pdata = filep->private_data;
	mutex_lock(&pdata->lock);
	if (pdata->state != I2CP_STATE_NEW) {
		ret = -EINVAL;
		goto unlock;
	}

	user_arg = (void __user *)arg;
	if (copy_from_user(&arg_copy, user_arg, sizeof(arg_copy))) {
		ret = -EFAULT;
		goto unlock;
	}

	if ((arg_copy.functionality & I2C_FUNC_I2C) != I2C_FUNC_I2C ||
	    (arg_copy.functionality & ~(I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL))) {
		ret = -EINVAL;
		goto unlock;
	}
	pdata->functionality = arg_copy.functionality;
	pdata->i2c_adapter.timeout = msecs_to_jiffies(
		arg_copy.timeout_ms ? arg_copy.timeout_ms : default_timeout_ms);
	if (arg_copy.suffix) {
		ret = i2cp_set_suffix(arg_copy.suffix, pdata);
		if (ret < 0)
			goto unlock;
	}

	ret = i2c_add_adapter(&pdata->i2c_adapter);
	if (ret < 0)
		goto unlock;

	pdata->state = I2CP_STATE_WAIT_FOR_XFER;
	arg_copy.output.adapter_num = pdata->i2c_adapter.nr;
	arg_copy.output.pseudo_id = pdata->id;
	BUILD_BUG_ON((void *)&arg_copy.output != (void *)&arg_copy);
	ret = copy_to_user(user_arg, &arg_copy.output, sizeof(arg_copy.output))
		? -EFAULT : 0;

 unlock:
	mutex_unlock(&pdata->lock);
	return ret;
}

static void i2cp_null_bufs(struct i2c_msg *msgs, u16 num_msgs)
{
	for (u16 i = 0; i < num_msgs; ++i)
		msgs[i].buf = NULL;
}

static void i2cp_fill_bufs(struct i2c_msg *msgs, u16 num_msgs,
	u8 *data_buf_copy, u8 __user *user_data_buf)
{
	u16 i;
	size_t pos;
	for (i = 0, pos = 0; i < num_msgs; pos += msgs[i++].len) {
		/*
		 * The data buffer is always copied, even for reads, to
		 * faithfully pass on to userspace exactly what this I2C adapter
		 * received from the I2C subsystem.
		 */
		memcpy(&data_buf_copy[pos], msgs[i].buf,
		       sizeof(*msgs[i].buf) * msgs[i].len);
		/* Set buf address for userspace. */
		msgs[i].buf = &user_data_buf[pos];
	}
}

/* Returns true if *msgs_copy should be copied to user, false if not. */
/* When false is returned, *ret _will_ be set to a negative errno value. */
/* When true is returned, *ret _may_ be set to a negative errno value. */
static bool i2cp_xfer_req_copy_data(struct i2c_msg *msgs_copy, u16 num_msgs,
	u8 __user *user_data_buf, u32 data_buf_len, long *ret)
{
	bool copy_msgs = true;
	size_t total_buf_len;
	u8 *data_buf_copy;

	if (!i2cp_sum_buf_lens(msgs_copy, num_msgs, &total_buf_len) ||
	    total_buf_len > data_buf_len) {
		i2cp_null_bufs(msgs_copy, num_msgs);
		*ret = -ENOBUFS;
		return true;
	}

	// TODO: GFP_USER instead of GFP_KERNEL?
	data_buf_copy = kzalloc(sizeof(*data_buf_copy) * total_buf_len,
	                        GFP_KERNEL);
	if (!data_buf_copy) {
		*ret = -ENOMEM;
		return false;
	}

	i2cp_fill_bufs(msgs_copy, num_msgs, data_buf_copy, user_data_buf);
	if (copy_to_user(user_data_buf, data_buf_copy,
	    sizeof(*data_buf_copy) * total_buf_len)) {
		*ret = -EFAULT;
		copy_msgs = false;
	}
	kfree(data_buf_copy);
	return copy_msgs;
}

static long i2cp_xfer_req_copy_msgs(struct i2c_msg *xfer_msgs, u16 num_msgs,
	struct i2cp_ioctl_xfer_req_arg *arg_copy)
{
	long ret = 0;
	struct i2c_msg *msgs_copy;
	// TODO: GFP_USER instead of GFP_KERNEL?
	msgs_copy = kmemdup(xfer_msgs, sizeof(*xfer_msgs) * num_msgs,
	                    GFP_KERNEL);
	if (!msgs_copy)
		return -ENOMEM;
	if (i2cp_xfer_req_copy_data(msgs_copy, num_msgs, arg_copy->data_buf,
	                            arg_copy->data_buf_len, &ret) &&
	    copy_to_user(arg_copy->msgs, msgs_copy,
	                 sizeof(*msgs_copy) * num_msgs))
		ret = -EFAULT;
	kfree(msgs_copy);
	return ret;
}

static long i2cp_cdev_ioctl_xfer_req(struct file *filep, unsigned long arg)
{
	long ret = 0;
	struct i2cp_ioctl_xfer_req_arg arg_copy;
	struct i2cp_ioctl_xfer_req_arg __user *user_arg;
	struct i2cp_controller *pdata;
	pdata = filep->private_data;
	user_arg = (void __user *)arg;
	if (copy_from_user(&arg_copy, user_arg, sizeof(arg_copy)))
		return -EFAULT;

 wait_for_next_xfer:
	if (filep->f_flags & O_NONBLOCK) {
		if (!try_wait_for_completion(&pdata->i2c_xfer_queued))
			return -EAGAIN;
	} else {
		ret = wait_for_completion_killable(&pdata->i2c_xfer_queued);
		if (ret == -ERESTARTSYS)
			return -EINTR;
		if (ret != 0)
			return -ENOTRECOVERABLE;
		/* ret == 0 */
	}

	mutex_lock(&pdata->lock);
	switch (pdata->state) {
	case I2CP_STATE_WAIT_FOR_REQ:
		break;
	case I2CP_STATE_WAIT_FOR_XFER:
	case I2CP_STATE_WAIT_FOR_REPLY:
	case I2CP_STATE_XFER_RETURN:
		mutex_unlock(&pdata->lock);
		goto wait_for_next_xfer;
	case I2CP_STATE_RETURN_THEN_SHUTDOWN:
	case I2CP_STATE_SHUTDOWN:
		ret = -ESHUTDOWN;
		goto unlock;
	default:
		ret = -ENOTRECOVERABLE;
		goto recomplete;
	}

	arg_copy.output.xfer_id = pdata->i2c_xfer_id;
	arg_copy.output.num_msgs = pdata->i2c_xfer_num_msgs;
	BUILD_BUG_ON((void *)&arg_copy.output != (void *)&arg_copy);
	if (copy_to_user(user_arg, &arg_copy.output, sizeof(arg_copy.output))) {
		ret = -EFAULT;
		goto recomplete;
	}
	if (arg_copy.msgs_len < pdata->i2c_xfer_num_msgs) {
		ret = -EMSGSIZE;
		goto recomplete;
	}
	ret = i2cp_xfer_req_copy_msgs(
		pdata->i2c_xfer_msgs, pdata->i2c_xfer_num_msgs, &arg_copy);
	if (ret >= 0) {
		pdata->state = I2CP_STATE_WAIT_FOR_REPLY;
		goto unlock;
	}

 recomplete:
	complete(&pdata->i2c_xfer_queued);
 unlock:
	mutex_unlock(&pdata->lock);
	return ret;
}

static long i2cp_cdev_ioctl_xfer_reply(struct file *filep, unsigned long arg)
{
	long ret = 0;
	struct i2c_msg *msgs_copy;
	struct i2cp_ioctl_xfer_reply_arg arg_copy;
	struct i2cp_ioctl_xfer_reply_arg __user *user_arg;
	struct i2cp_controller *pdata;
	user_arg = (void __user *)arg;
	if (copy_from_user(&arg_copy, user_arg, sizeof(arg_copy)))
		return -EFAULT;
	pdata = filep->private_data;

	mutex_lock(&pdata->lock);
	switch (pdata->state) {
	case I2CP_STATE_WAIT_FOR_REPLY:
	case I2CP_STATE_WAIT_FOR_REQ:
		break;
	case I2CP_STATE_WAIT_FOR_XFER:
		/* master_xfer already returned for pdata->i2c_xfer_id */
		ret = arg_copy.xfer_id <= pdata->i2c_xfer_id ? -ETIME : -EINVAL;
		goto unlock;
	case I2CP_STATE_XFER_RETURN:
		/* master_xfer has not yet returned for pdata->i2c_xfer_id */
		ret = arg_copy.xfer_id < pdata->i2c_xfer_id ? -ETIME : -EINVAL;
		goto unlock;
	case I2CP_STATE_RETURN_THEN_SHUTDOWN:
	case I2CP_STATE_SHUTDOWN:
		ret = -ESHUTDOWN;
		goto unlock;
	case I2CP_STATE_NEW:
		ret = -EINVAL;
		goto unlock;
	default:
		ret = -ENOTRECOVERABLE;
		goto unlock;
	}
	if (arg_copy.xfer_id != pdata->i2c_xfer_id) {
		ret = arg_copy.xfer_id < pdata->i2c_xfer_id ? -ETIME : -EINVAL;
		goto unlock;
	}
	if (arg_copy.num_msgs > pdata->i2c_xfer_num_msgs) {
		ret = -EINVAL;
		goto unlock;
	}
	msgs_copy = kcalloc(arg_copy.num_msgs, sizeof(*msgs_copy), GFP_KERNEL);
	if (!msgs_copy) {
		ret = -ENOMEM;
		goto unlock;
	}
	if (copy_from_user(msgs_copy, arg_copy.msgs,
	                   sizeof(*msgs_copy) * arg_copy.num_msgs)) {
		ret = -EFAULT;
		goto unlock;
	}
	for (u16 i = 0; i < arg_copy.num_msgs; ++i) {
		if ((msgs_copy[i].flags & I2C_M_RD) && copy_from_user(
			pdata->i2c_xfer_msgs[i].buf, msgs_copy[i].buf,
			pdata->i2c_xfer_msgs[i].len *
			sizeof(*(msgs_copy[i].buf))))
		{
			ret = -EFAULT;
			goto unlock;
		}
	}
	pdata->i2c_xfer_num_msgs = arg_copy.num_msgs;
	if (arg_copy.error == 0)
		pdata->i2c_xfer_ret = arg_copy.num_msgs;
	else
		pdata->i2c_xfer_ret = max(INT_MIN, -((s32)arg_copy.error));
	pdata->state = I2CP_STATE_XFER_RETURN;
	complete(&pdata->i2c_xfer_done);
unlock:
	mutex_unlock(&pdata->lock);
	return ret;
}

static long i2cp_cdev_ioctl_get_counters(struct file *filep, unsigned long arg)
{
	long ret;
	struct i2cp_ioctl_xfer_counters __user *user_arg;
	struct i2cp_controller *pdata;
	pdata = filep->private_data;
	user_arg = (void __user *)arg;

	mutex_lock(&pdata->lock);
	ret = copy_to_user(user_arg, &pdata->xfer_counters,
	                   sizeof(pdata->xfer_counters)) ? -EFAULT : 0;
	mutex_unlock(&pdata->lock);
	return ret;
}

static long i2cp_cdev_ioctl_shutdown(struct file *filep, unsigned long arg)
{
	struct i2cp_controller *pdata;
	if (arg)
		return -EINVAL;
	pdata = filep->private_data;

	mutex_lock(&pdata->lock);
	pdata->state = (pdata->state == I2CP_STATE_XFER_RETURN) ?
		I2CP_STATE_RETURN_THEN_SHUTDOWN : I2CP_STATE_SHUTDOWN;
	mutex_unlock(&pdata->lock);
	/* wake up any pollers */
	wake_up_interruptible_all(&pdata->poll_wait_queue);
	/* wake up blocked master_xfer */
	complete_all(&pdata->i2c_xfer_done);
	/* wake up blocked I2CP_IOCTL_XFER_REQ */
	complete_all(&pdata->i2c_xfer_queued);
	return 0;
}

static long i2cp_cdev_unlocked_ioctl(struct file *filep, unsigned int cmd,
		unsigned long arg)
{
	switch (cmd) {
	case I2CP_IOCTL_XFER_REQ:
		return i2cp_cdev_ioctl_xfer_req(filep, arg);
	case I2CP_IOCTL_XFER_REPLY:
		return i2cp_cdev_ioctl_xfer_reply(filep, arg);
	case I2CP_IOCTL_GET_COUNTERS:
		return i2cp_cdev_ioctl_get_counters(filep, arg);
	case I2CP_IOCTL_START:
		return i2cp_cdev_ioctl_start(filep, arg);
	case I2CP_IOCTL_SHUTDOWN:
		return i2cp_cdev_ioctl_shutdown(filep, arg);
	}
	return -ENOTTY;
}

#if 0
#ifdef CONFIG_COMPAT
static long i2cp_cdev_compat_ioctl(struct file *filep, unsigned int cmd,
		unsigned long arg)
{
	// TODO: implementation
}
#endif
#endif

/*
 * POLLIN indicates xfer request waiting for I2CP_IOCTL_XFER_REQ.  This is what
 * pollers will normally want to wait for.
 *
 * POLLOUT indicates I2CP_IOCTL_XFER_REPLY is allowed, which is always the case
 * between I2CP_IOCTL_START and I2CP_IOCTL_SHUTDOWN.  Waiting for this before
 * I2CP_IOCTL_XFER_REPLY is functionally safe but completely unnecessary.
 *
 * POLLHUP indicates I2CP_IOCTL_SHUTDOWN was called.
 */
static __poll_t i2cp_cdev_poll(struct file *filep, poll_table *ptp)
{
	__poll_t poll_ret = 0;
	struct i2cp_controller *pdata;

	pdata = filep->private_data;
	poll_wait(filep, &pdata->poll_wait_queue, ptp);

	mutex_lock(&pdata->lock);
	switch (pdata->state) {
	case I2CP_STATE_WAIT_FOR_REQ:
		poll_ret |= POLLOUT | POLLIN;
		break;
	case I2CP_STATE_WAIT_FOR_XFER:
	case I2CP_STATE_WAIT_FOR_REPLY:
	case I2CP_STATE_XFER_RETURN:
		poll_ret |= POLLOUT;
		break;
	case I2CP_STATE_RETURN_THEN_SHUTDOWN:
	case I2CP_STATE_SHUTDOWN:
		poll_ret |= POLLHUP;
		break;
	default:
		break;
	}
	mutex_unlock(&pdata->lock);

	return poll_ret;
}

static const struct file_operations i2cp_fileops = {
	.owner = THIS_MODULE,
	.open = i2cp_cdev_open,
	.release = i2cp_cdev_release,
	.unlocked_ioctl = i2cp_cdev_unlocked_ioctl,
#if 0
#ifdef CONFIG_COMPAT
	.compat_ioctl = i2cp_cdev_compat_ioctl,
#endif
#endif
	.poll = i2cp_cdev_poll,
	.llseek = no_llseek,
};

static ssize_t i2cp_count_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int count, ret;
	struct i2cp_device *this_pseudo;
	this_pseudo = container_of(dev, struct i2cp_device, device);

	mutex_lock(&this_pseudo->counters.lock);
	count = this_pseudo->counters.count;
	mutex_unlock(&this_pseudo->counters.lock);

	ret = snprintf(buf, PAGE_SIZE, "%u\n", count);
	if (ret >= PAGE_SIZE)
		return -ERANGE;
	return ret;
}

static struct device_attribute i2cp_count_dev_attr = {
	.attr = {
		.name = "count",
		.mode = 0444,
	},
	.show = i2cp_count_show,
};

static struct attribute *i2cp_device_sysfs_attrs[] = {
	&i2cp_count_dev_attr.attr,
	NULL,
};

static const struct attribute_group i2cp_device_sysfs_group = {
	.attrs = i2cp_device_sysfs_attrs,
};

static const struct attribute_group *i2cp_device_sysfs_groups[] = {
	&i2cp_device_sysfs_group,
	NULL,
};

static inline void i2c_p_device_release(struct device *dev)
{
	struct i2cp_device *this_pseudo;
	this_pseudo = container_of(dev, struct i2cp_device, device);
	kfree(this_pseudo);
}

static inline void i2c_p_class_destroy(void)
{
	struct class *class;
	class = i2cp_class;
	i2cp_class = NULL;
	class_destroy(class);
}

static int __init i2cp_init(void)
{
	int ret;
	// TODO: backports will need:
	//i2cp_class = class_create(THIS_MODULE, "i2c-pseudo");
	i2cp_class = class_create("i2c-pseudo");
	if (IS_ERR(i2cp_class))
		return PTR_ERR(i2cp_class);
	i2cp_class->dev_groups = i2cp_device_sysfs_groups;

	ret = alloc_chrdev_region(&i2cp_cdev_num, 0, i2cp_cdev_count,
		"i2c_pseudo");
	if (ret < 0)
		goto fail_after_class_create;

	i2cp_device = kzalloc(sizeof(*i2cp_device), GFP_KERNEL);
	if (!i2cp_device) {
		ret = -ENOMEM;
		goto fail_after_chrdev_register;
	}

	i2cp_device->device.devt = i2cp_cdev_num;
	i2cp_device->device.class = i2cp_class;
	i2cp_device->device.release = i2c_p_device_release;
	device_initialize(&i2cp_device->device);
	ret = dev_set_name(&i2cp_device->device, "%s", "i2c-pseudo");
	if (ret < 0)
		goto fail_after_device_init;

	mutex_init(&i2cp_device->counters.lock);
	INIT_LIST_HEAD(&i2cp_device->counters.all_controllers);
	cdev_init(&i2cp_device->cdev, &i2cp_fileops);
	i2cp_device->cdev.owner = THIS_MODULE;

	ret = cdev_device_add(&i2cp_device->cdev, &i2cp_device->device);
	if (ret >= 0)
		return 0;

 fail_after_device_init:
	put_device(&i2cp_device->device);
 fail_after_chrdev_register:
	unregister_chrdev_region(i2cp_cdev_num, i2cp_cdev_count);
 fail_after_class_create:
	i2c_p_class_destroy();
	return ret;
}

static void __exit i2cp_exit(void)
{
	cdev_device_del(&i2cp_device->cdev, &i2cp_device->device);
	put_device(&i2cp_device->device);
	unregister_chrdev_region(i2cp_cdev_num, i2cp_cdev_count);
	i2c_p_class_destroy();
}

MODULE_AUTHOR("Matthew Blecker <matthewb@chromium.org");
MODULE_DESCRIPTION("Driver for userspace I2C adapter implementations.");
MODULE_LICENSE("GPL");

module_init(i2cp_init);
module_exit(i2cp_exit);
