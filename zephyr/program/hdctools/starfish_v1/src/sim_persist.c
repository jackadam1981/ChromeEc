
#include "console_util.h"
#include "sim_persist.h"

#include <stdio.h>
#include <string.h>

#include <zephyr/fs/fcb.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>

LOG_MODULE_DECLARE(sim, LOG_LEVEL_DBG);

static int sim_setting_cb(const char *key, size_t len, settings_read_cb read_cb,
			  void *cb_arg, void *param)
{
	struct storage_result *result = (struct storage_result *)param;
	result->size =
		read_cb(cb_arg, result->buffer, result->bufferSize);
	LOG_DBG("Setting Load key:%s, len:%d, size:%d", key, len,
		result->size);
	return 0;
}

bool sim_persist_load(const char *key, struct storage_result *result)
{
	int err = settings_subsys_init();

	if (err) {
		LOG_ERR("Setting init failed %d", err);
	}
	err = settings_load_subtree_direct(key, sim_setting_cb, result);
	if (err) {
		LOG_DBG("Load %s failed with error %d", key, err);
	} else if (result->size < result->minSize ||
			result->maxSize < result->size) {
		LOG_DBG("Load %s out of data range actual:%d min:%d max:%d",
			key, result->size, result->minSize, result->maxSize);
	} else {
		LOG_DBG("Load %s with size %u", key, result->size);
		return true;
	}
	return false;
}

bool sim_persist_save(const char *key, const void *src, size_t len)
{
	int err = settings_subsys_init();

	if (err) {
		LOG_ERR("Setting init failed %d", err);
	}
	if (len) {
		err = settings_save_one(key, src, len);
	} else {
		err = settings_delete(key);
	}
	if (err) {
		LOG_DBG("Save %s len %d failed with error %d", key, len, err);
	} else {
		LOG_DBG("Save %s len %d", key, len);
	}
	return err == 0;
}

bool sim_persist_erase() {
	return true;
}

void sim_persist_status() {
	
}
