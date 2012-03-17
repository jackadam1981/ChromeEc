#!/usr/bin/env python
#
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Generate task ID list from task list XML

import sys
from xml.dom import minidom

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

tasklist = find_name(doc, "tasklist")
assert tasklist is not None

print '''
/**
 * enumerate all tasks in the priority order
 *
 * the identifier of a task can be retrieved using the following constant:
 * task_id_<taskname> where <taskname> is the name specified in task.xml.
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
