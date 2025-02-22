/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "ap_power/ap_pwrseq.h"
#include "ap_power/ap_pwrseq_sm.h"
#include "emul/emul_ap_callbacks.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>

LOG_MODULE_REGISTER(ap_pwrseq, CONFIG_AP_PWRSEQ_LOG_LEVEL);

struct ap_pwrseq_data {
	/* State machine data reference. */
	void *sm_data;
	/* State entry notification list. */
        struct ap_pwrseq_cb_list entry_list;
        /* State exit notification list. */
        struct ap_pwrseq_cb_list exit_list;
};

static struct ap_pwrseq_data emul_ap_pwrseq_data;

static int ap_pwrseq_driver_init(const struct device *dev);

DEVICE_DEFINE(ap_pwrseq_dev, "ap_pwrseq_drv", ap_pwrseq_driver_init, NULL,
	      &emul_ap_pwrseq_data, NULL, POST_KERNEL,
	      CONFIG_APPLICATION_INIT_PRIORITY, NULL);

/**
 * State machine prototypes
 **/
void *ap_pwrseq_sm_get_instance(void);

int ap_pwrseq_sm_init(void *const data, k_tid_t tid,
		      enum ap_pwrseq_state init_state);

int ap_pwrseq_sm_run_state(void *const data, uint32_t events);

enum ap_pwrseq_state ap_pwrseq_sm_get_cur_state(void *const data);

/**
 *  Private functions definition.
 **/
static int ap_pwrseq_driver_init(const struct device *dev)
{
	struct ap_pwrseq_data *const data = dev->data;

	data->sm_data = ap_pwrseq_sm_get_instance();

	return 0;
}

/**
 *  Global functions definition.
 **/
const struct device *ap_pwrseq_get_instance(void)
{
	return DEVICE_GET(ap_pwrseq_dev);
}

int ap_pwrseq_start(const struct device *dev, enum ap_pwrseq_state init_state)
{
	struct ap_pwrseq_data *const data = dev->data;

	return ap_pwrseq_sm_init(data->sm_data, NULL, init_state);
}

static void ap_pwrseq_add_state_callback(struct ap_pwrseq_cb_list *cb_list,
                                         sys_snode_t *node)
{
        if (!sys_slist_is_empty(&cb_list->list)) {
                sys_slist_find_and_remove(&cb_list->list, node);
        }

        sys_slist_prepend(&cb_list->list, node);
}

static int
ap_pwrseq_register_state_callback(struct ap_pwrseq_state_callback *state_cb,
                                  struct ap_pwrseq_cb_list *cb_list)
{
        if (!(state_cb->states_bit_mask & AP_PWRSEQ_STATES_MASK)) {
                return -EINVAL;
        }

        __ASSERT(state_cb->cb, "Callback pointer should not be NULL");

        k_spinlock_key_t key = k_spin_lock(&cb_list->lock);

        ap_pwrseq_add_state_callback(cb_list, &state_cb->node);

        cb_list->states |= AP_PWRSEQ_STATES_MASK & state_cb->states_bit_mask;
        k_spin_unlock(&cb_list->lock, key);

        return 0;
}

static void ap_pwrseq_send_callback(const struct device *dev,
                                    const enum ap_pwrseq_state entry,
                                    const enum ap_pwrseq_state exit,
                                    bool is_entry)
{
        struct ap_pwrseq_data *const data = dev->data;
        struct ap_pwrseq_cb_list *cb_list = is_entry ? &data->entry_list :
                                                       &data->exit_list;
        const enum ap_pwrseq_state *state = is_entry ? &entry : &exit;
        struct ap_pwrseq_state_callback *state_cb, *tmp;

        if (!(cb_list->states & BIT(*state))) {
                return;
        }
        SYS_SLIST_FOR_EACH_CONTAINER_SAFE(&cb_list->list, state_cb, tmp, node)
        {
                if (state_cb->states_bit_mask & BIT(*state)) {
                        state_cb->cb(dev, entry, exit);
                }
        }
}

void ap_pwrseq_emul_entry_callback(const struct device *dev,
				   enum ap_pwrseq_state new,
				   enum ap_pwrseq_state curr)

{
	ap_pwrseq_send_callback(dev, new, curr, true);
}

void ap_pwrseq_emul_exit_callback(const struct device *dev,
                                   enum ap_pwrseq_state new,
                                   enum ap_pwrseq_state curr)

{
        ap_pwrseq_send_callback(dev, curr, new, false);
}


void ap_pwrseq_post_event(const struct device *dev, enum ap_pwrseq_event event)
{
	struct ap_pwrseq_data *const data = dev->data;
	enum ap_pwrseq_state cur_state, new_state;

	if (event >= AP_PWRSEQ_EVENT_COUNT) {
		return;
	}

	while (true) {
		cur_state = ap_pwrseq_sm_get_cur_state(data->sm_data);
		/**
		 * Given that thread is not created for emulator, run functions
		 * will be executed whenever an event is posted.
		 **/
		ap_pwrseq_sm_run_state(data->sm_data, (uint32_t)BIT(event));
		new_state = ap_pwrseq_sm_get_cur_state(data->sm_data);

		if (cur_state == new_state) {
			return;
		}
	}
}

enum ap_pwrseq_state ap_pwrseq_get_current_state(const struct device *dev)
{
	struct ap_pwrseq_data *const data = dev->data;
	enum ap_pwrseq_state ret_state;

	ret_state = ap_pwrseq_sm_get_cur_state(data->sm_data);

	return ret_state;
}

int ap_pwrseq_register_state_entry_callback(
        const struct device *dev, struct ap_pwrseq_state_callback *state_cb)
{
        struct ap_pwrseq_data *data = dev->data;

        return ap_pwrseq_register_state_callback(state_cb, &data->entry_list);
}

int ap_pwrseq_register_state_exit_callback(
        const struct device *dev, struct ap_pwrseq_state_callback *state_cb)
{
        struct ap_pwrseq_data *data = dev->data;

        return ap_pwrseq_register_state_callback(state_cb, &data->exit_list);
}

