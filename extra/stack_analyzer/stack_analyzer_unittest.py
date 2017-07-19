#!/usr/bin/env python3
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for Stack Analyzer classes and functions."""

from __future__ import print_function
from unittest.mock import MagicMock, patch, call # pylint: disable=E0611, F0401
from stack_analyzer import ArmAnalyzer, StackAnalyzer, StackAnalyzerError
from stack_analyzer import Task, Function, Symbol, Callsite
from stack_analyzer import ParseSymbolFile, ParseTasklistFile
import stack_analyzer
import subprocess
import tempfile
import unittest


class ArmAnalyzerTest(unittest.TestCase):
  """Tests for class ArmAnalyzer."""

  def AppendConditionCode(self, opcodes):
    condition_codes = ['', 'eq', 'ne', 'cs', 'hs', 'cc', 'lo', 'mi', 'pl', 'vs',
                       'vc', 'hi', 'ls', 'ge', 'lt', 'gt', 'le']
    rets = []
    for opcode in opcodes:
      rets.extend(opcode + cc for cc in condition_codes)

    return rets

  def testInstructionMatching(self):
    jump_list = self.AppendConditionCode(['b', 'bx']) + ['cbz', 'cbnz']
    jump_list += (list(opcode + '.n' for opcode in jump_list) +
                  list(opcode + '.w' for opcode in jump_list))
    for opcode in jump_list:
      self.assertIsNotNone(ArmAnalyzer.JUMP_OPCODE_RE.match(opcode))

    self.assertIsNone(ArmAnalyzer.JUMP_OPCODE_RE.match('bl'))
    self.assertIsNone(ArmAnalyzer.JUMP_OPCODE_RE.match('blx'))

    call_list = self.AppendConditionCode(['bl', 'blx'])
    call_list += list(opcode + '.n' for opcode in call_list)
    for opcode in call_list:
      self.assertIsNotNone(ArmAnalyzer.CALL_OPCODE_RE.match(opcode))

    self.assertIsNone(ArmAnalyzer.CALL_OPCODE_RE.match('ble'))

    result = ArmAnalyzer.CALL_OPERAND_RE.match('53f90 <get_time+0x18>')
    self.assertIsNotNone(result)
    self.assertEqual(result.group(1), '53f90')
    self.assertEqual(result.group(2), 'get_time+0x18')

    self.assertIsNotNone(ArmAnalyzer.PUSH_OPCODE_RE.match('push'))
    self.assertIsNone(ArmAnalyzer.PUSH_OPCODE_RE.match('pushal'))
    self.assertIsNotNone(ArmAnalyzer.STM_OPCODE_RE.match('stmdb'))
    self.assertIsNone(ArmAnalyzer.STM_OPCODE_RE.match('lstm'))
    self.assertIsNotNone(ArmAnalyzer.SUB_OPCODE_RE.match('sub'))
    self.assertIsNotNone(ArmAnalyzer.SUB_OPCODE_RE.match('subs'))
    self.assertIsNotNone(ArmAnalyzer.SUB_OPCODE_RE.match('subw'))
    self.assertIsNotNone(ArmAnalyzer.SUB_OPCODE_RE.match('sub.w'))
    self.assertIsNotNone(ArmAnalyzer.SUB_OPCODE_RE.match('subs.w'))

    result = ArmAnalyzer.SUB_OPERAND_RE.match('sp, sp, #1668   ; 0x684')
    self.assertIsNotNone(result)
    self.assertEqual(result.group(1), '1668')
    result = ArmAnalyzer.SUB_OPERAND_RE.match('sp, #1668')
    self.assertIsNotNone(result)
    self.assertEqual(result.group(1), '1668')
    self.assertIsNone(ArmAnalyzer.SUB_OPERAND_RE.match('sl, #1668'))

  def testAnalyzeFunction(self):
    analyzer = ArmAnalyzer()
    symbol = Symbol(0x10, 'F', 0x100, 'foo')
    instructions = [
        (0x10, 'push', '{r4, r5, r6, r7, lr}'),
        (0x12, 'subw', 'sp, sp, #16	; 0x10'),
        (0x16, 'movs', 'lr, r1'),
        (0x18, 'beq.n', '26 <foo+0x26>'),
        (0x1a, 'bl', '30 <foo+0x30>'),
        (0x1e, 'bl', 'deadbeef <bar>'),
        (0x22, 'blx', '0 <woo>'),
        (0x26, 'push', '{r1}'),
        (0x28, 'stmdb', 'sp!, {r4, r5, r6, r7, r8, r9, lr}'),
        (0x2c, 'stmdb', 'sp!, {r4}'),
        (0x30, 'stmdb', 'sp, {r4}'),
        (0x34, 'bx.n', '10 <foo>'),
    ]
    (size, callsites) = analyzer.AnalyzeFunction(symbol, instructions)
    self.assertEqual(size, 72)
    expect_callsites = [
        Callsite(0x1e, 0xdeadbeef, False),
        Callsite(0x22, 0x0, False),
        Callsite(0x34, 0x10, True),
    ]
    self.assertEqual(len(callsites), len(expect_callsites))
    for callsite_a, callsite_b in zip(callsites, expect_callsites):
      self.assertEqual(callsite_a.address, callsite_b.address)
      self.assertEqual(callsite_a.target, callsite_b.target)
      self.assertEqual(callsite_a.is_tail, callsite_b.is_tail)
      self.assertIsNone(callsite_a.callee)


class StackAnalyzerTest(unittest.TestCase):
  """Tests for class StackAnalyzer."""

  def setUp(self):
    symbols = [Symbol(0x1000, 'F', 0x15C, 'hook_task'),
               Symbol(0x2000, 'F', 0x51C, 'console_task')]
    tasklist = [Task('HOOKS', 'hook_task', '2048', 0x1000),
                Task('CONSOLE', 'console_task', 'STACK_SIZE', 0x2000)]
    options = MagicMock(elf_path='./ec.RW.elf',
                        tasklist_path='./ec.RW.tasklist',
                        objdump='objdump',
                        addr2line='addr2line')
    self.analyzer = StackAnalyzer(options, symbols, tasklist)

  def testParseSymbolFile(self):
    with tempfile.TemporaryFile(mode='w+') as temp:
      temp.write(
          '0 g     F .text  e8 Foo\n'
          '0000dead  w    F .text  000000e8 .hidden Bar\n'
          'deadbeef l     O .bss   00000004 .hidden Woooo\n'
          'deadbee g     O .rodata        00000008 __Hooo_ooo\n'
          'deadbee g       .rodata        00000000 __foo_doo_coo_end\n'
      )
      temp.flush()
      temp.seek(0)
      symbols = ParseSymbolFile(temp)
      expect_symbols = [
          Symbol(0x0, 'F', 0xe8, 'Foo'),
          Symbol(0xdead, 'F', 0xe8, 'Bar'),
          Symbol(0xdeadbeef, 'O', 0x4, 'Woooo'),
          Symbol(0xdeadbee, 'O', 0x8, '__Hooo_ooo'),
          Symbol(0xdeadbee, 'O', 0x0, '__foo_doo_coo_end'),
      ]
      self.assertEqual(len(symbols), len(expect_symbols))
      for symbol_a, symbol_b in zip(symbols, expect_symbols):
        self.assertEqual(symbol_a.address, symbol_b.address)
        self.assertEqual(symbol_a.symtype, symbol_b.symtype)
        self.assertEqual(symbol_a.size, symbol_b.size)
        self.assertEqual(symbol_a.name, symbol_b.name)

  def testParseTasklist(self):
    with tempfile.TemporaryFile(mode='w+') as temp:
      temp.write('("HOOKS", hook_task, 2048) '
                 '("WOOKS", hook_task, 4096) '
                 '("CONSOLE", console_task, STACK_SIZE)')
      temp.flush()
      temp.seek(0)
      tasklist = ParseTasklistFile(temp, self.analyzer.symbols)
      expect_tasklist = [
          Task('HOOKS', 'hook_task', '2048', 0x1000),
          Task('WOOKS', 'hook_task', '4096', 0x1000),
          Task('CONSOLE', 'console_task', 'STACK_SIZE', 0x2000),
      ]
      self.assertEqual(len(tasklist), len(expect_tasklist))
      for task_a, task_b in zip(tasklist, expect_tasklist):
        self.assertEqual(task_a.name, task_b.name)
        self.assertEqual(task_a.routine_name, task_b.routine_name)
        self.assertEqual(task_a.stack_config, task_b.stack_config)
        self.assertEqual(task_a.routine_address, task_b.routine_address)

  def testAnalyzeDisassembly(self):
    with tempfile.TemporaryFile(mode='w+') as temp:
      temp.write(
          '\n'
          'Disassembly of section .text:\n'
          '\n'
          '00000900 <wook_task>:\n'
          '	...\n'
          '00001000 <hook_task>:\n'
          '   1000:	dead beef\tfake\n'
          '   1004:	4770\t\tbx	lr\n'
          '   1006:	00015cfc\t.word	0x00015cfc\n'
          '00002000 <console_task>:\n'
          '   2000:	b508\t\tpush	{r3, lr}\n'
          '   2002:	f00e fcc5\tbl	1000 <hook_task>\n'
          '   2006:	f00e bd3b\tb.w	53968 <get_program_memory_addr>\n'
          '   200a:	dead beef\tfake'
      )
      temp.flush()
      temp.seek(0)
      function_map = self.analyzer.AnalyzeDisassembly(temp)
      func_hook_task = Function(0x1000, 'hook_task', 0, [])
      expect_funcmap = {
          0x1000: func_hook_task,
          0x2000: Function(0x2000, 'console_task', 8,
                           [Callsite(0x2002, 0x1000, False, func_hook_task),
                            Callsite(0x2006, 0x53968, True, None)]),
      }
      self.assertEqual(set(function_map.keys()), set(expect_funcmap.keys()))
      for address, func_a in function_map.items():
        func_b = expect_funcmap[address]
        self.assertEqual(func_a.address, func_b.address)
        self.assertEqual(func_a.name, func_b.name)
        self.assertEqual(func_a.stack_frame, func_b.stack_frame)
        self.assertEqual(len(func_a.callsites), len(func_b.callsites))
        for callsite_a, callsite_b in zip(func_a.callsites, func_b.callsites):
          self.assertEqual(callsite_a.address, callsite_b.address)
          self.assertEqual(callsite_a.target, callsite_b.target)
          self.assertEqual(callsite_a.is_tail, callsite_b.is_tail)
          if callsite_a.callee is None:
            self.assertIsNone(callsite_b.callee)
          else:
            self.assertIsNotNone(callsite_b.callee)
            self.assertEqual(callsite_a.callee.address,
                             callsite_b.callee.address)

  def testAnalyzeCallGraph(self):
    funcs = {
        0x1000: Function(0x1000, 'hook_task', 0, []),
        0x2000: Function(0x2000, 'console_task', 8, []),
        0x3000: Function(0x3000, 'task_a', 12, []),
        0x4000: Function(0x4000, 'task_b', 96, []),
        0x5000: Function(0x5000, 'task_c', 32, []),
        0x6000: Function(0x6000, 'task_d', 100, []),
        0x7000: Function(0x7000, 'task_e', 24, []),
        0x8000: Function(0x8000, 'task_f', 20, []),
        0x9000: Function(0x9000, 'task_g', 20, []),
    }
    funcs[0x1000].callsites = [Callsite(0x1002, 0x3000, False, funcs[0x3000]),
                               Callsite(0x1006, 0x4000, False, funcs[0x4000])]
    funcs[0x2000].callsites = [Callsite(0x2002, 0x5000, False, funcs[0x5000])]
    funcs[0x3000].callsites = [Callsite(0x3002, 0x4000, False, funcs[0x4000])]
    funcs[0x4000].callsites = [Callsite(0x4002, 0x6000, True, funcs[0x6000]),
                               Callsite(0x4006, 0x7000, False, funcs[0x7000]),
                               Callsite(0x400a, 0x8000, False, funcs[0x8000])]
    funcs[0x5000].callsites = [Callsite(0x5002, 0x4000, False, funcs[0x4000])]
    funcs[0x7000].callsites = [Callsite(0x7002, 0x7000, False, funcs[0x7000])]
    funcs[0x8000].callsites = [Callsite(0x8002, 0x9000, False, funcs[0x9000])]
    funcs[0x9000].callsites = [Callsite(0x9002, 0x4000, False, funcs[0x4000])]

    scc_group = self.analyzer.AnalyzeCallGraph(funcs)

    expect_func_stack = {
        0x1000: (148, funcs[0x3000], set()),
        0x2000: (176, funcs[0x5000], set()),
        0x3000: (148, funcs[0x4000], set()),
        0x4000: (136, funcs[0x8000], {funcs[0x4000],
                                      funcs[0x8000],
                                      funcs[0x9000]}),
        0x5000: (168, funcs[0x4000], set()),
        0x6000: (100, None, set()),
        0x7000: (24, None, {funcs[0x7000]}),
        0x8000: (40, funcs[0x9000], {funcs[0x4000],
                                     funcs[0x8000],
                                     funcs[0x9000]}),
        0x9000: (20, None, {funcs[0x4000], funcs[0x8000], funcs[0x9000]}),
    }
    for func in funcs.values():
      (stack_max, stack_successor, scc) = expect_func_stack[func.address]
      self.assertEqual(func.stack_max, stack_max)
      self.assertEqual(func.stack_successor, stack_successor)
      self.assertEqual(set(scc_group[func.cycle_index]), scc)

  @patch('subprocess.Popen')
  def testAddressToLine(self, popen_mock):
    process_mock = MagicMock()
    process_mock.communicate = MagicMock(return_value=(b'test.c [1]', None))
    process_mock.returncode = 0
    popen_mock.return_value = process_mock
    self.assertEqual(self.analyzer.AddressToLine(0x1000), 'test.c [1]')
    popen_mock.assert_called_once_with(
        ['addr2line', '-e', './ec.RW.elf', '1000'], stderr=-1, stdout=-1)

    with self.assertRaisesRegex(StackAnalyzerError,
                                'addr2line failed to resolve lines.'):
      process_mock.returncode = 1
      self.analyzer.AddressToLine(0x1000)

    with self.assertRaisesRegex(StackAnalyzerError, 'Failed to run addr2line.'):
      popen_mock.side_effect = OSError()
      self.analyzer.AddressToLine(0x1000)

  @patch('subprocess.Popen')
  def testAnalyze(self, popen_mock):
    def fakeObjdumpDisasm(args, stdout):
      # pylint: disable=unused-argument
      stdout.write(
          '\n'
          'Disassembly of section .text:\n'
          '\n'
          '00000900 <wook_task>:\n'
          '	...\n'
          '00001000 <hook_task>:\n'
          '   1000:	4770\t\tbx	lr\n'
          '   1004:	00015cfc\t.word	0x00015cfc\n'
          '00002000 <console_task>:\n'
          '   2000:	b508\t\tpush	{r3, lr}\n'
          '   2002:	f00e fcc5\tbl	1000 <hook_task>\n'
          '   2006:	f00e bd3b\tb.w	53968 <get_program_memory_addr>\n'
      )
      stdout.flush()

    with patch('subprocess.check_call') as checkall_mock:
      with patch('builtins.print') as print_mock:
        checkall_mock.side_effect = fakeObjdumpDisasm
        process_mock = MagicMock()
        process_mock.communicate = MagicMock(return_value=(b'?', None))
        process_mock.returncode = 0
        popen_mock.return_value = process_mock
        self.analyzer.Analyze()
        print_mock.assert_has_calls([
            call('Task: HOOKS, Max size: 64 (0 + 64), Allocated size: 2048'),
            call('Call Trace:'),
            call('\thook_task (0) 1000 [?]'),
            call('Task: CONSOLE, Max size: 72 (8 + 64), Allocated size: '
                 'STACK_SIZE'),
            call('Call Trace:'),
            call('\tconsole_task (8) 2000 [?]'),
        ])

    with self.assertRaisesRegex(StackAnalyzerError, 'Failed to run objdump.'):
      popen_mock.side_effect = OSError()
      self.analyzer.Analyze()

    with self.assertRaisesRegex(StackAnalyzerError,
                                'objdump failed to disassemble.'):
      popen_mock.side_effect = subprocess.SubprocessError()
      self.analyzer.Analyze()

  @patch('subprocess.check_call')
  @patch('stack_analyzer.ParseArgs')
  def testMain(self, parseargs_mock, checkall_mock):
    def fakeObjdumpSymbol(args, stdout):
      # pylint: disable=unused-argument
      stdout.write('1000 g     F .text  0000015c .hidden hook_task\n'
                   '2000 g     F .text  0000051c .hidden console_task\n')
      stdout.flush()

    parseargs_mock.return_value = MagicMock(elf_path='./ec.RW.elf',
                                            tasklist_path='',
                                            objdump='objdump',
                                            addr2line='addr2line')

    with patch('builtins.print') as print_mock:
      checkall_mock.side_effect = subprocess.SubprocessError()
      stack_analyzer.main()
      print_mock.assert_called_once_with(
          'Error: objdump failed to dump symbol table.')

    with patch('builtins.print') as print_mock:
      checkall_mock.side_effect = OSError()
      stack_analyzer.main()
      print_mock.assert_called_once_with('Error: Failed to run objdump.')

    with patch('builtins.print') as print_mock:
      checkall_mock.side_effect = fakeObjdumpSymbol
      stack_analyzer.main()
      print_mock.assert_called_once_with('Error: Failed to open tasklist.')


if __name__ == '__main__':
  unittest.main()
