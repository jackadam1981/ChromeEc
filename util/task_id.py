#!/usr/bin/env python
#
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Generate task list and implementation from task list XML

import sys
import xml.dom.minidom as minidom

doc = minidom.parse(sys.argv[1])

def find_name(node, name):
	for n in node.childNodes:
		if n.nodeType == n.ELEMENT_NODE and n.localName == name:
			return n
	return None

def find_name_iter(node, name):
	for n in node.childNodes:
		if n.nodeType == n.ELEMENT_NODE and n.localName == name:
			yield n

def get_attr(task, attr):
	return find_name(task, attr).childNodes[0].nodeValue

def print_impl(name, routine, arg):
	print ('\t{.context[0] = (uint32_t)(tasks + TASK_ID_%s + 1) - 64,'
			% name)
	print '\t .context[TASK_SIZE/4 - 8/*r0*/] = (uint32_t)%s,' % arg
	print '\t .context[TASK_SIZE/4 - 3/*lr*/] = (uint32_t)task_exit_trap,'
	print '\t .context[TASK_SIZE/4 - 2/*pc*/] = (uint32_t)%s,' % routine
	print '\t .context[TASK_SIZE/4 - 1/*psr*/] = 0x01000000 },'

def print_task_impl(task):
	name = get_attr(task, "name")
	routine = get_attr(task, "routine")
	arg = get_attr(task, "arg")
	print_impl(name, routine, arg)

tasklist = find_name(doc, "tasklist")
if tasklist is None:
	print "Cannot find task list."

print '''
/**
 * enumerate all tasks in the priority order
 *
 * the identifier of a task can be retrieved using the following constant:
 * task_id_<taskname> where <taskname> is the first parameter passed to the
 * task macro in the task_list file.
 */
'''

print 'enum {'
print '\tTASK_ID_IDLE,'
for n in find_name_iter(tasklist, "task"):
	print '\tTASK_ID_%s,' % get_attr(n, "name")
print '''
	/* Number of tasks */
	TASK_ID_COUNT,
	/* Special task identifiers */
	TASK_ID_MUTEX   = 0x1e, /* signal mutex unlocking */
	TASK_ID_TIMER   = 0x1f, /* message from an expired timer */
	TASK_ID_CURRENT = 0xfe, /* the currently running task */
	TASK_ID_INVALID = 0xff  /* unable to find the task */
};
'''
