/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Feature set module for Chrome EC */

#include "atomic.h"
#include "common.h"
#include "console.h"
#include "feature_set.h"
#include "host_command.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_FEATURES, outstr)
#define CPRINTF(format, args...) cprintf(CC_FEATURES, format, ## args)

/* Each bit represents one feature. It only accepts maximum 32 features. */
static uint32_t supported_features;
static uint32_t features;

/* Boolean to lock all the features at onetime. Not support bitwise lock. */
static int features_locked;


uint32_t features_are_supported(uint32_t mask)
{
	return supported_features & mask;
}

uint32_t features_are_enabled(uint32_t mask)
{
	return features & mask;
}

static uint32_t enable_features(uint32_t mask)
{
	/* Only print if something's about to change */
	if ((features & mask) != mask)
		CPRINTF("[%T feature set 0x%08x]\n", mask);

	atomic_or(&features, mask);
	return features;
}

static uint32_t disable_features(uint32_t mask)
{
	/* Only print if something's about to change */
	if (features & mask)
		CPRINTF("[%T feature clear 0x%08x]\n", mask);

	atomic_clear(&features, mask);
	return features;
}

int features_are_locked(void)
{
	return features_locked;
}

static void lock_features(void)
{
	CPRINTF("[%T all features locked]\n");
	features_locked = 1;
}

void unlock_features(void)
{
	CPRINTF("[%T all features unlocked]\n");
	features_locked = 0;
}

/*****************************************************************************/
/* Host commands */

static int host_cmd_feature_lock(struct host_cmd_handler_args *args)
{
	lock_features();
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_FEATURE_LOCK,
		     host_cmd_feature_lock,
		     EC_VER_MASK(0));

static int host_cmd_feature_get_supported(struct host_cmd_handler_args *args)
{
	struct ec_response_feature_mask *r =
		(struct ec_response_feature_mask *)args->response;

	r->mask = features_are_supported(-1UL);
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_FEATURE_GET_SUPPORTED,
		     host_cmd_feature_get_supported,
		     EC_VER_MASK(0));

static int host_cmd_feature_get_enabled(struct host_cmd_handler_args *args)
{
	struct ec_response_feature_mask *r =
		(struct ec_response_feature_mask *)args->response;

	r->mask = features_are_enabled(-1UL);
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_FEATURE_GET_ENABLED,
		     host_cmd_feature_get_enabled,
		     EC_VER_MASK(0));

static int host_cmd_feature_enable(struct host_cmd_handler_args *args)
{
	const struct ec_params_feature_mask *p =
		(const struct ec_params_feature_mask *)args->params;
	struct ec_response_feature_mask *r =
		(struct ec_response_feature_mask *)args->response;

	if (features_are_supported(p->mask) != p->mask)
		return EC_RES_ACCESS_DENIED;

	if (features_are_locked())
		return EC_RES_ACCESS_DENIED;

	r->mask = enable_features(p->mask);
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_FEATURE_ENABLE,
		     host_cmd_feature_enable,
		     EC_VER_MASK(0));

static int host_cmd_feature_disable(struct host_cmd_handler_args *args)
{
	const struct ec_params_feature_mask *p =
		(const struct ec_params_feature_mask *)args->params;
	struct ec_response_feature_mask *r =
		(struct ec_response_feature_mask *)args->response;

	if (features_are_supported(p->mask) != p->mask)
		return EC_RES_ACCESS_DENIED;

	if (features_are_locked())
		return EC_RES_ACCESS_DENIED;

	r->mask = disable_features(p->mask);
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_FEATURE_DISABLE,
		     host_cmd_feature_disable,
		     EC_VER_MASK(0));

