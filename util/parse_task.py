#!/usr/bin/env python
#
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Generate task list and implementation from task list XML

import sys
from xml.dom import minidom

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
  print '\t{.context[0] = (uint32_t)(tasks + TASK_ID_%s + 1) - 64,' % name
  print '\t .context[TASK_SIZE/4 - 8/*r0*/] = (uint32_t)%s,' % arg
  print '\t .context[TASK_SIZE/4 - 3/*lr*/] = (uint32_t)task_exit_trap,'
  print '\t .context[TASK_SIZE/4 - 2/*pc*/] = (uint32_t)%s,' % routine
  print '\t .context[TASK_SIZE/4 - 1/*psr*/] = 0x01000000 },'


def print_task_impl(task):
  name = get_attr(task, "name")
  routine = get_attr(task, "routine")
  arg = get_attr(task, "arg")
  print_impl(name, routine, arg)

if __name__ == "__main__":
  doc = minidom.parse(sys.argv[1])

  tasklist = find_name(doc, "tasklist")
  assert tasklist is not None

  print 'static void task_exit_trap(void);'
  print ''

  print '/* Declare task routine prototypes */'
  print 'void __idle(void);'
  for n in find_name_iter(tasklist, "task"):
    print 'void %s(void);' % get_attr(n, "routine")
  print ''

  print '/* Store the task names for easier debugging */'
  print 'static const char * const task_names[] = {'
  print '\t"<< idle >>",'
  for n in find_name_iter(tasklist, "task"):
    print '\t"%s",' % get_attr(n, "name")
  print '};'
  print ''

  print '/* Declare and fill the contexts for all the tasks */'
  print 'static task_ tasks[] __attribute__((section(".data.tasks")))'
  print '\t\t__attribute__((aligned(TASK_SIZE))) = {'
  print_impl("IDLE", "__idle", "0")
  for n in find_name_iter(tasklist, "task"):
    print_task_impl(n)
  print '};'
