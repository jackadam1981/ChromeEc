/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Persistence module for emulator */

/* Enable Linux-specific O_TMPFILE extension. */
#define _GNU_SOURCE

#include <linux/limits.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/*
 * Linked list of known persistence tags, each having a file descriptor
 * referring to an inode that is not accessible by any name in the file system.
 */
struct persistent_record_t {
	char *tag;
	int fd;
	struct persistent_record_t *next;
};
static struct persistent_record_t *persistent_record_head = NULL;

FILE *get_persistent_storage(const char *tag, const char *mode)
{
	struct persistent_record_t *record;
	for (record = persistent_record_head; record; record = record->next) {
		/* If we already have the tag, create and return a FILE object
		 * from the file descriptor.  Duplicate so that we retain the
		 * original descriptor, even after the fclose() closes the file
		 * descriptor provided to fdopen().
		 */
		if (strcmp(record->tag, tag) == 0)
			return fdopen(dup(record->fd), mode);
	}

	/* Create a new linked list record. */
	record = (struct persistent_record_t *)malloc(sizeof(struct persistent_record_t));
	record->next = persistent_record_head;
	persistent_record_head = record;
	record->tag = strdup(tag);

	/*
	 * Open a file descriptor and inode on given file system, without
	 * creating a directory entry.
	 */
	record->fd = open("/dev/shm", O_CREAT | O_TMPFILE | O_RDWR, 0600);

	/* Create FILE from the new file descriptor. */
	return fdopen(dup(record->fd), mode);
}

void release_persistent_storage(FILE *ps)
{
	fclose(ps);
}

void remove_persistent_storage(const char *tag)
{
	/*
	 * Remove the linked entry that matches the given tag.
	 */
	struct persistent_record_t **record;
	for (record = &persistent_record_head; *record; record = &(*record)->next) {
		if (strcmp((*record)->tag, tag) == 0) {
			struct persistent_record_t *next = (*record)->next;
			free((*record)->tag);
			close((*record)->fd);
			free(*record);
			*record = next;
			return;
		}
	}
}
