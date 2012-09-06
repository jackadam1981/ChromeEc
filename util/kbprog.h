
/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Functions for kbprog.c.  These are only needed as forward-references.  Since
 * it's a one-file program, they are in here to pass the style checks.
 */

void print(struct object *x);
void intern_builtin(struct object *s);
Object read_object(const char **s);

