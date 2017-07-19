#!/usr/bin/env python2
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for Stack Analyzer classes and functions."""

from __future__ import print_function
from mock import MagicMock, patch, call # pylint: disable=E0611, F0401
from stack_analyzer import ArmAnalyzer, StackAnalyzer, StackAnalyzerError
from stack_analyzer import Task, Function, Symbol, Callsite
from stack_analyzer import ParseSymbolFile, ParseTasklistFile
import stack_analyzer
import subprocess
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
    self.assertEqual(callsites, expect_callsites)


class StackAnalyzerTest(unittest.TestCase):
  """Tests for class StackAnalyzer."""

  def setUp(self):
    symbols = [Symbol(0x1000, 'F', 0x15C, 'hook_task'),
               Symbol(0x2000, 'F', 0x51C, 'console_task'),
               Symbol(0x3200, 'O', 0x124, '__just_data'),
               Symbol(0x4000, 'F', 0x11C, 'touchpad_calc')]
    tasklist = [Task('HOOKS', 'hook_task', '2048', 0x1000),
                Task('CONSOLE', 'console_task', 'STACK_SIZE', 0x2000)]
    options = MagicMock(elf_path='./ec.RW.elf',
                        taskinfo_path='./ec.RW.taskinfo',
                        objdump='objdump',
                        addr2line='addr2line')
    self.analyzer = StackAnalyzer(options, symbols, tasklist)

  def testParseSymbolFile(self):
    symbol_text = (
        '0 g     F .text  e8 Foo\n'
        '0000dead  w    F .text  000000e8 .hidden Bar\n'
        'deadbeef l     O .bss   00000004 .hidden Woooo\n'
        'deadbee g     O .rodata        00000008 __Hooo_ooo\n'
        'deadbee g       .rodata        00000000 __foo_doo_coo_end\n'
    )
    symbols = ParseSymbolFile(symbol_text)
    expect_symbols = [
        Symbol(0x0, 'F', 0xe8, 'Foo'),
        Symbol(0xdead, 'F', 0xe8, 'Bar'),
        Symbol(0xdeadbeef, 'O', 0x4, 'Woooo'),
        Symbol(0xdeadbee, 'O', 0x8, '__Hooo_ooo'),
        Symbol(0xdeadbee, 'O', 0x0, '__foo_doo_coo_end'),
    ]
    self.assertEqual(symbols, expect_symbols)

  def testParseTasklist(self):
    taskinfo_text = (
        '("HOOKS", hook_task, 2048) '
        '("WOOKS", hook_task, 4096) '
        '("CONSOLE", console_task, STACK_SIZE)'
    )
    tasklist = ParseTasklistFile(taskinfo_text, self.analyzer.symbols)
    expect_tasklist = [
        Task('HOOKS', 'hook_task', '2048', 0x1000),
        Task('WOOKS', 'hook_task', '4096', 0x1000),
        Task('CONSOLE', 'console_task', 'STACK_SIZE', 0x2000),
    ]
    self.assertEqual(tasklist, expect_tasklist)

  def testAnalyzeDisassembly(self):
    disasm_text = (
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
        '   2000:	b508\t\tpush	{r3, lr} ; malformed comments,; r0, r1 \n'
        '   2002:	f00e fcc5\tbl	1000 <hook_task>\n'
        '   2006:	f00e bd3b\tb.w	53968 <get_program_memory_addr>\n'
        '   200a:	dead beef\tfake\n'
        '00004000 <touchpad_calc>:\n'
        '   4000:	4770\t\tbx	lr\n'
        '00010000 <look_task>:'
    )
    function_map = self.analyzer.AnalyzeDisassembly(disasm_text)
    func_hook_task = Function(0x1000, 'hook_task', 0, [])
    expect_funcmap = {
        0x1000: func_hook_task,
        0x2000: Function(0x2000, 'console_task', 8,
                         [Callsite(0x2002, 0x1000, False, func_hook_task),
                          Callsite(0x2006, 0x53968, True, None)]),
        0x4000: Function(0x4000, 'touchpad_calc', 0, []),
    }
    self.assertEqual(function_map, expect_funcmap)

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
      (stack_max_usage, stack_successor, scc) = expect_func_stack[func.address]
      self.assertEqual(func.stack_max_usage, stack_max_usage)
      self.assertEqual(func.stack_successor, stack_successor)
      self.assertEqual(set(scc_group[func.cycle_index]), scc)

  @patch('subprocess.check_output')
  def testAddressToLine(self, checkoutput_mock):
    checkoutput_mock.return_value = 'test.c [1]'
    self.assertEqual(self.analyzer.AddressToLine(0x1000), 'test.c [1]')
    checkoutput_mock.assert_called_once_with(
        ['addr2line', '-e', './ec.RW.elf', '1000'])

    with self.assertRaisesRegexp(StackAnalyzerError,
                                 'addr2line failed to resolve lines.'):
      checkoutput_mock.side_effect = subprocess.CalledProcessError(1, '')
      self.analyzer.AddressToLine(0x1000)

    with self.assertRaisesRegexp(StackAnalyzerError,
                                 'Failed to run addr2line.'):
      checkoutput_mock.side_effect = OSError()
      self.analyzer.AddressToLine(0x1000)

  @patch('subprocess.check_output')
  def testAnalyze(self, checkoutput_mock):
    disasm_text = (
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

    with patch('__builtin__.print') as print_mock:
      checkoutput_mock.side_effect = [disasm_text, '?', '?', '?']
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

    with self.assertRaisesRegexp(StackAnalyzerError, 'Failed to run objdump.'):
      checkoutput_mock.side_effect = [OSError(), '?', '?', '?']
      self.analyzer.Analyze()

    with self.assertRaisesRegexp(StackAnalyzerError,
                                 'objdump failed to disassemble.'):
      checkoutput_mock.side_effect = [subprocess.CalledProcessError(1, ''), '?',
                                      '?', '?']
      self.analyzer.Analyze()

  @patch('subprocess.check_output')
  @patch('stack_analyzer.ParseArgs')
  def testMain(self, parseargs_mock, checkoutput_mock):
    symbol_text = ('1000 g     F .text  0000015c .hidden hook_task\n'
                   '2000 g     F .text  0000051c .hidden console_task\n')

    parseargs_mock.return_value = MagicMock(elf_path='./ec.RW.elf',
                                            taskinfo_path='',
                                            objdump='objdump',
                                            addr2line='addr2line')

    with patch('__builtin__.print') as print_mock:
      checkoutput_mock.return_value = symbol_text
      stack_analyzer.main()
      print_mock.assert_called_once_with('Error: Failed to open taskinfo.')

    with patch('__builtin__.print') as print_mock:
      checkoutput_mock.side_effect = subprocess.CalledProcessError(1, '')
      stack_analyzer.main()
      print_mock.assert_called_once_with(
          'Error: objdump failed to dump symbol table.')

    with patch('__builtin__.print') as print_mock:
      checkoutput_mock.side_effect = OSError()
      stack_analyzer.main()
      print_mock.assert_called_once_with('Error: Failed to run objdump.')


if __name__ == '__main__':
  unittest.main()
