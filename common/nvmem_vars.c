/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * SPI flash driver for Chrome EC.
 */

#include "common.h"
#include "nvmem.h"
#include "nvmem_vars.h"
#include "printf.h"
#include "util.h"

/* RAM copy of persistent variable store */
static char rbuf[CONFIG_FLASH_NVMEM_VARS_USER_SIZE];
static int rbuf_in_use;

test_mockable_static
void release_local_copy(void)
{
	rbuf_in_use = 0;
}

test_mockable_static
int get_local_copy(void)
{
	int rv;

	if (rbuf_in_use)
		return EC_SUCCESS;

	rbuf_in_use = 1;

	rv = nvmem_read(0, CONFIG_FLASH_NVMEM_VARS_USER_SIZE,
			rbuf, CONFIG_FLASH_NVMEM_VARS_USER_NUM);
	if (rv == EC_SUCCESS) {
		/* Should always end with two \0's; this just makes sure */
		rbuf[CONFIG_FLASH_NVMEM_VARS_USER_SIZE - 1] = '\0';
		rbuf[CONFIG_FLASH_NVMEM_VARS_USER_SIZE - 2] = '\0';
	} else {
		release_local_copy();
	}

	return rv;
}

/*
 * If rbuf+kidx matches keystr, point vidx at the value and return true.
 * If it doesn't match, return false (and *vidx is meaningless).
 */
static int match_key_at(const char *keystr, uint32_t kidx, uint32_t *vidx)
{
	uint8_t cbuf, ckey;
	int rv = 0;

	while (kidx < CONFIG_FLASH_NVMEM_VARS_USER_SIZE) {
		cbuf = rbuf[kidx++];
		ckey = *keystr++;
		if (ckey && ckey != cbuf)	/* mismatch */
			goto nope;
		if (!ckey) {			/* end of keystr */
			if (cbuf == '=')	/*   and end of key in rbuf */
				break;
			goto nope;		/*   more key in rbuf */
		}
	}
	/* match: ckey is '\0' and cbuf is '=', kidx points at value */
	rv = 1;
nope:
	*vidx = kidx;
	return rv;
}

/*
 * Find the start of the next key in rbuf. Return false if there isn't one. The
 * idx arg tracks where to start looking and where the next key was found.
 */
static int next_key(uint32_t *idx)
{
	uint32_t i;

	for (i = *idx; i < CONFIG_FLASH_NVMEM_VARS_USER_SIZE; i++)
		if (rbuf[i] == '\0')
			break;
	/*
	 * Unless the rbuf is completely empty, the next char is either the
	 * start of a new key or the '\0' at the end of all the variables.
	 */
	if (i)
		*idx = ++i;

	return rbuf[i] != '\0';
}

/*
 * Look for the key in rbuf. If the key is found, set the key index to the
 * start of the key and the value index to the start of the value and return
 * true. If the key is not found, set both indices to the location where a new
 * key=val string should be added (0 if no variables exist at all, else at the
 * second '\0' at the end of the variables) and return false.
 */
test_mockable_static
int getvar_kv(const char *key, uint32_t *kidx, uint32_t *vidx)
{
	uint32_t k = 0;

	do {
		if (match_key_at(key, k, vidx)) {
			*kidx = k;
			return 1;
		}
	} while (next_key(&k));

	*kidx = *vidx = k;
	return 0;
}

static int bogus_key(const char *key)
{
	if (!key || *key == '\0')
		return 1;

	for (; *key; key++)
		if (*key == '=')
			return 1;
	return 0;
}

int initvars(void)
{
	int rv, i = 0;

	rv = get_local_copy();
	if (rv != EC_SUCCESS)
		return rv;

	do {
		/* Should be looking at a key */
		if (rbuf[i] == '\0' || rbuf[i] == '=')
			goto fixit;

		while (rbuf[i] != '\0' && rbuf[i] != '=')
			i++;

		if (rbuf[i] == '\0')
			goto fixit;
		i++;

		/* Should be looking at a value */
		if (rbuf[i] == '\0')
			goto fixit;

		while (rbuf[i] != '\0')
			i++;

		/* End of the value */
		i++;

	} while (rbuf[i]);

	return EC_SUCCESS;

fixit:
	rbuf[0] = '\0';
	rbuf[1] = '\0';
	return writevars();
}

const char *getvar(const char *key)
{
	uint32_t k, v;

	if (bogus_key(key))
		return 0;

	if (get_local_copy() != EC_SUCCESS)
		return 0;

	if (getvar_kv(key, &k, &v))
		return rbuf + v;

	return 0;
}

int setvar(const char *key, const char *val)
{
	int k, v, klen, vlen;
	int rv;

	if (bogus_key(key))
		return EC_ERROR_INVAL;

	rv = get_local_copy();
	if (rv != EC_SUCCESS)
		return rv;

	/*
	 * If the key exists in rbuf, k and v will be updated with the indices
	 * where the key and value are located. If not, they'll tell where to
	 * write the new entry (ie, after all the other variables).
	 */
	if (getvar_kv(key, &k, &v)) {
		/* Found the match at [k]=[v] */
		if (next_key(&v)) {
			/*
			 * Now [v] is the start of the variable after ours.
			 * Delete our entry by shifting left from there to the
			 * end of rbuf, so that it covers ours up.
			 *
			 * Before:
			 *           [k]      [v]
			 *   foo=bar\0KEY=VAL\0hey=splat\0\0
			 *
			 * After:
			 *           [k]
			 *   foo=bar\0hey=splat\0\0
			 */
			memmove(rbuf + k, rbuf + v,
				CONFIG_FLASH_NVMEM_VARS_USER_SIZE - v);
			/* Advance k to point to the end of all variables */
			while (next_key(&k))
				;
		}
		/* Whether we found a match or not, it's not there now */
	}
	/*
	 * Now [k] is where the new string should be written.
	 *
	 * Either this:
	 *                      [k]
	 *   foo=bar\0hey=splat\0\0
	 *
	 * Or this:
	 *
	 *  [k]
	 *   \0\0
	 */

	/* How long are the strings we need to write? */
	vlen = val ? strlen(val) : 0;
	if (!vlen) {
		/*
		 * No value to save. We've already deleted the variable, but we
		 * still need to mark the end of the list.
		 */
		rbuf[k] = '\0';
		if (k == 0)
			rbuf[1] = '\0';
		return EC_SUCCESS;
	}
	klen = strlen(key);

	/*
	 * We'll always write the updated entry at the end of any existing
	 * variables, so we mark the end with an additional '\0' and write
	 * 'KEY=VAL\0\0'. The '=' and two '\0's adds three more characters to
	 * the length. Make sure it will fit.
	 */
	if (k + klen + vlen + 3 > CONFIG_FLASH_NVMEM_VARS_USER_SIZE)
		return EC_ERROR_OVERFLOW;

	/*
	 * snprintf() returns an error code if the output is truncated, but it
	 * won't be because we counted carefully.
	 */
	return snprintf(rbuf + k,
			CONFIG_FLASH_NVMEM_VARS_USER_SIZE - k,
			"%s=%s\0", key, val);
}

int writevars(void)
{
	int rv = get_local_copy();

	if (rv != EC_SUCCESS)
		return rv;

	rv = nvmem_write(0, CONFIG_FLASH_NVMEM_VARS_USER_SIZE,
			 rbuf, CONFIG_FLASH_NVMEM_VARS_USER_NUM);
	if (rv != EC_SUCCESS)
		return rv;

	rv = nvmem_commit();
	if (rv != EC_SUCCESS)
		return rv;

	release_local_copy();

	return rv;
}

#ifdef TEST_BUILD
#include "console.h"

static int command_get(int argc, char **argv)
{
	const char *val;

	if (argc != 2)
		return EC_ERROR_PARAM_COUNT;

	val = getvar(argv[1]);
	if (!val)
		return EC_SUCCESS;

	ccprintf("%s\n", val);
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(get, command_get,
			"VARIABLE",
			"Show the value of the specified variable");

static int command_set(int argc, char **argv)
{
	int rc;

	if (argc != 2 && argc != 3)
		return EC_ERROR_PARAM_COUNT;

	rc =  setvar(argv[1], argc > 2 ? argv[2] : 0);
	if (rc)
		return rc;

	return writevars();
}
DECLARE_CONSOLE_COMMAND(set, command_set,
			"VARIABLE [VALUE]",
			"Set/clear the value of the specified variable");

static int command_print(int argc, char **argv)
{
	char c;
	int i = 0, rv;

	rv = get_local_copy();
	if (rv)
		return rv;

	if (rbuf[0] == '\0')
		return EC_SUCCESS;

	while (i < CONFIG_FLASH_NVMEM_VARS_USER_SIZE) {
		c = rbuf[i++];
		if (c == '\0') {
			ccprintf("\n");
			if (rbuf[i] == '\0')
				break;
		} else {
			ccprintf("%c", c);
		}
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(print, command_print,
			"",
			"Print all defined variables");
#endif
