/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <ap_power/ap_pwrseq_sm.h>
#include <logging/log.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>

#define DT_DRV_COMPAT ap_pwrseq_state

static K_KERNEL_STACK_DEFINE(ap_pwrseq_thread_stack,
			    CONFIG_AP_PWRSEQ_STACK_SIZE);

struct ap_pwrseq_data {
	struct ap_pwrseq_sm_data *sm;
	struct k_thread thread;
	struct k_sem sem;
	atomic_t events;
	sys_slist_t cb_list;
};

static struct ap_pwrseq_data ap_pwrseq_data_0;

void ap_pwrseq_post_event(const struct device *dev,
			 enum ap_pwrseq_event event)
{
	struct ap_pwrseq_data *const data = dev->data;

	atomic_set_bit(&data->events, event);
	k_sem_give(&data->sem);
}

int ap_pwrseq_get_current_state(const struct device *dev,
				enum ap_pwrseq_state *state)
{
	struct ap_pwrseq_data *const data = dev->data;

	if ((*state = ap_pwrseq_sm_get_cur_state(data->sm)) ==
	     AP_POWER_STATE_UNDEF) {
		return -EIO;
	}
	return 0;
}

static int ap_pwrseq_add_state_callback(sys_slist_t *list, sys_snode_t *node)
{
	if (!sys_slist_is_empty(list)) {
		if (!sys_slist_find_and_remove(list, node)) {
			return -EINVAL;
		}
	}

	sys_slist_prepend(list, node);

	return 0;
}

int ap_pwrseq_register_state_callback(const struct device *dev,
				      struct ap_pwrseq_state_callback
				      *state_cb)
{
	struct ap_pwrseq_data *data = dev->data;

	return ap_pwrseq_add_state_callback(&data->cb_list, &state_cb->node);
}

static void ap_pwrseq_send_callbacks(const struct device *dev,
				     enum ap_pwrseq_state state,
				     uint8_t action)
{
	struct ap_pwrseq_data *const data = dev->data;
	struct ap_pwrseq_state_callback *state_cb, *tmp;

	SYS_SLIST_FOR_EACH_CONTAINER_SAFE(&data->cb_list, state_cb, tmp, node) {
		if (state_cb->state == state &&
		    state_cb->action & action &&
		    state_cb->cb) {
			state_cb->cb(dev, state, action);
		}
	}
}

static inline void ap_pwrseq_event_wait(const struct device *dev)
{
	struct ap_pwrseq_data *const data = dev->data;

	k_sem_take(&data->sem, K_FOREVER);
}

static void ap_pwrseq_thread(void *arg, void *unused1, void *unused2)
{
	struct device *const dev = (struct device *)arg;
	struct ap_pwrseq_data *const data = dev->data;
	enum ap_pwrseq_state cur_state;

	while(true) {
		ap_pwrseq_sm_set_events(data->sm, atomic_set(&data->events, 0));
		cur_state = ap_pwrseq_sm_get_cur_state(data->sm);

		if (ap_pwrseq_sm_run_state(data->sm)) {
			break;
		}

		/* Current state generates callbacks to run action */
		ap_pwrseq_send_callbacks(dev, cur_state,
			AP_POWER_STATE_ACTION_RUN);

		if (cur_state == ap_pwrseq_sm_get_cur_state(data->sm)) {
			/* No state transition, wait for any event */
			ap_pwrseq_event_wait(dev);
		} else {
			/* Previous state generates callbacks for exit actions */
			ap_pwrseq_send_callbacks(dev,
				ap_pwrseq_sm_get_prev_state(data->sm),
				AP_POWER_STATE_ACTION_EXIT);

			/* New state generates callbacks for entry actions */
			ap_pwrseq_send_callbacks(dev,
				ap_pwrseq_sm_get_cur_state(data->sm),
				AP_POWER_STATE_ACTION_ENTRY);
		}
	}
}

static int ap_pwrseq_driver_init(const struct device *dev)
{
	struct ap_pwrseq_data *const data = dev->data;

	data->sm = ap_pwrseq_sm_get_instance();
	k_sem_init(&data->sem, 0, 1);
	ap_pwrseq_sm_init(data->sm, AP_POWER_STATE_G3);

	k_thread_create(&data->thread,
		ap_pwrseq_thread_stack,
		K_KERNEL_STACK_SIZEOF(ap_pwrseq_thread_stack),
		(k_thread_entry_t)ap_pwrseq_thread,
		(void *)dev, NULL, NULL,
		CONFIG_AP_PWRSEQ_THREAD_PRIORITY, 0,
		IS_ENABLED(CONFIG_AP_PWRSEQ_AUTOSTART) ? K_NO_WAIT
						       : K_FOREVER);

	k_thread_name_set(&data->thread, "ap_pwrseq_task");

	return 0;
}

void ap_pwrseq_start(const struct device *dev)
{
	struct ap_pwrseq_data *const data = dev->data;

	if (!IS_ENABLED(CONFIG_AP_PWRSEQ_AUTOSTART)) {
		k_thread_start(&data->thread);
	}
}

DEVICE_DT_INST_DEFINE(0,
		      ap_pwrseq_driver_init, NULL,
		      &ap_pwrseq_data_0, NULL,
		      POST_KERNEL,
		      50, NULL);
