#!/usr/bin/env python2
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Statically analyze stack usage of EC firmware.

  Example:
    extra/stack_analyzer/stack_analyzer.py ./build/elm/RW/ec.RW.elf \
        ./build/elm/RW/ec.RW.taskinfo
"""

from __future__ import print_function
import argparse
import subprocess
import re


# The size of extra stack frame needed by interrupts.
INTERRUPT_EXTRA_STACK_FRAME = 64


class StackAnalyzerError(Exception):
  """Exception class for stack analyzer utility."""


class Task(object):
  """Task information.

  Attributes:
    name: Task name.
    routine_name: Routine function name.
    routine_address: Resolved routine address. None if it hasn't been resolved.
    stack_max_size: Max stack size.
  """

  def __init__(self, name, routine_name, stack_max_size, routine_address=None):
    """Constructor.

    Args:
      name: Task name.
      routine_name: Routine function name.
      stack_max_size: Max stack size.
      routine_address: Resolved routine address.
    """
    self.name = name
    self.routine_name = routine_name
    self.routine_address = routine_address
    self.stack_max_size = stack_max_size


class Symbol(object):
  """Symbol information.

  Attributes:
    address: Symbol address.
    symtype: Symbol type, 'O' (data, object) or 'F' (function).
    size: Symbol size.
    name: Symbol name.
  """

  def __init__(self, address, symtype, size, name):
    """Constructor.

    Args:
      address: Symbol address.
      symtype: Symbol type.
      size: Symbol size.
      name: Symbol name.
    """
    assert symtype == 'O' or symtype == 'F'
    self.address = address
    self.symtype = symtype
    self.size = size
    self.name = name


class Callsite(object):
  """Function callsite.

  Attributes:
    address: Address of callsite location.
    target: Callee address.
    is_tail: Is a tailing call?
    callee: Resolved callee function. None if it hasn't been resolved.
  """

  def __init__(self, address, target, is_tail, callee=None):
    """Constructor.

    Args:
      address: Address of callsite location.
      target: Callee address.
      is_tail: Is a tailing call? (function jump to another function without
               restoring the stack frame)
      callee: Resolved callee function.
    """
    self.address = address
    self.target = target
    self.is_tail = is_tail
    self.callee = callee


class Function(object):
  """Function.

  Attributes:
    address: Address of function.
    name: Name of function from its symbol.
    stack_frame: Size of stack frame.
    callsites: Callsite list.
    stack_max_usage: Max stack usage. None if it hasn't been analyzed.
    stack_successor: Successor on the max stack usage path. None if it hasn't
                     been analyzed or it's the end.
    cycle_index: Index of the cycle group. None if it hasn't been analyzed.
  """

  def __init__(self, address, name, stack_frame, callsites):
    """Constructor.

    Args:
      address: Address of function.
      name: Name of function from its symbol.
      stack_frame: Size of stack frame.
      callsites: Callsite list.
    """
    self.address = address
    self.name = name
    self.stack_frame = stack_frame
    self.callsites = callsites
    self.stack_max_usage = None
    self.stack_successor = None
    # Node attributes for Tarjan's strongly connected components algorithm.
    self.scc_depth = None
    self.scc_done = False
    self.cycle_index = None


class ArmAnalyzer(object):
  """Disassembly analyzer for ARM architecture.

  Public Methods:
    AnalyzeFunction: Analyze stack frame and callsites of the function.
  """

  # Fuzzy regular expressions for instruction and operand parsing.
  # Possible condition code suffixes.
  CONDITION_CODES_RE = '((eq|ne|cs|hs|cc|lo|mi|pl|vs|vc|hi|ls|ge|lt|gt|le)?)'
  # Branch instructions.
  JUMP_OPCODE_RE = re.compile(
      r'^(b{0}|bx{0}|cbz|cbnz)(\.\w)?$'.format(CONDITION_CODES_RE))
  # Call instructions.
  CALL_OPCODE_RE = re.compile(
      r'^(bl{0}|blx{0})(\.\w)?$'.format(CONDITION_CODES_RE))
  # Assume there is no function name containing ">".
  CALL_OPERAND_RE = re.compile(r'^([0-9A-Fa-f]+)\s+<([^>]+)>$')
  # TODO(cheyuw@google.com): Handle conditional versions of following
  # instructions.
  # TODO(cheyuw@google.com): Handle other kinds of stm instructions.
  PUSH_OPCODE_RE = re.compile(r'^push$')
  STM_OPCODE_RE = re.compile(r'^stmdb$')
  # Stack subtraction instructions.
  SUB_OPCODE_RE = re.compile(r'^sub(s|w)?(\.\w)?$')
  SUB_OPERAND_RE = re.compile(r'^sp[^#]+#(\d+)')

  def AnalyzeFunction(self, function_symbol, instructions):
    """Analyze function, resolve the size of stack frame and callsites.

    Args:
      function_symbol: Function symbol.
      instructions: Instruction list.

    Returns:
      (stack_frame, callsites): Size of stack frame and callsite list.
    """
    def DetectCallsite(operand_text):
      """Check if the instruction is a callsite.

      Args:
        operand_text: Text of instruction operands.

      Returns:
        target_address: Target address. None if it isn't a callsite.
      """
      result = self.CALL_OPERAND_RE.match(operand_text)
      if result is None:
        return None

      target_address = int(result.group(1), 16)

      if (function_symbol.size > 0 and
          target_address > function_symbol.address and
          target_address < (function_symbol.address + function_symbol.size)):
        # Filter out the in-function target (branches and in-function calls,
        # which are actually branches).
        return None

      return target_address

    stack_frame = 0
    callsites = []
    for address, opcode, operand_text in instructions:
      is_jump_opcode = self.JUMP_OPCODE_RE.match(opcode) is not None
      is_call_opcode = self.CALL_OPCODE_RE.match(opcode) is not None
      if is_jump_opcode or is_call_opcode:
        target_address = DetectCallsite(operand_text)
        if target_address is not None:
          # Maybe it's a tailing call.
          callsite = Callsite(address, target_address, is_jump_opcode)
          if callsite is not None:
            callsites.append(callsite)

      elif self.PUSH_OPCODE_RE.match(opcode) is not None:
        # Example: "{r4, r5, r6, r7, lr}"
        stack_frame += len(operand_text.split(',')) * 4
      elif self.SUB_OPCODE_RE.match(opcode) is not None:
        result = self.SUB_OPERAND_RE.match(operand_text)
        if result is not None:
          stack_frame += int(result.group(1))
        else:
          # Unhandled stack register subtraction.
          assert not operand_text.startswith('sp')

      elif self.STM_OPCODE_RE.match(opcode) is not None:
        if operand_text.startswith('sp!'):
          # Subtract and writeback to stack register.
          # Example: "sp!, {r4, r5, r6, r7, r8, r9, lr}"
          # Get the text of pushed register list.
          parameter_text = operand_text.split(',', 1)[1]
          stack_frame += len(parameter_text.split(',')) * 4

    return (stack_frame, callsites)


class StackAnalyzer(object):
  """Class to analyze stack usage.

  Public Methods:
    Analyze: Run the stack analysis.
  """

  def __init__(self, options, symbols, tasklist):
    """Constructor.

    Args:
      options: Namespace from argparse.parse_args().
      symbols: Symbol list.
      tasklist: Task list.
    """
    self.options = options
    self.symbols = symbols
    self.tasklist = tasklist

  def AddressToLine(self, address):
    """Convert address to line.

    Args:
      address: Target address.

    Returns:
      line: The corresponding line.

    Raises:
      StackAnalyzerError: If addr2line is failed.
    """
    try:
      process = subprocess.Popen([self.options.addr2line,
                                  '-e',
                                  self.options.elf_path,
                                  '{:x}'.format(address)],
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE)
      (line_text, _) = process.communicate()
      if process.returncode != 0:
        raise StackAnalyzerError('addr2line failed to resolve lines.')

    except OSError:
      raise StackAnalyzerError('Failed to run addr2line.')

    line = line_text.strip()
    return line

  def AnalyzeDisassembly(self, disasm_text):
    """Parse the disassembly text, analyze, and build a map of all functions.

    Args:
      disasm_text: Disassembly text.

    Returns:
      function_map: Dict of functions.
    """
    # TODO(cheyuw@google.com): Selecting analyzer based on architecture.
    analyzer = ArmAnalyzer()

    # Example: "08028c8c <motion_lid_calc>:"
    function_signature_regex = re.compile(
        r'^(?P<address>[0-9A-Fa-f]+)\s+<(?P<name>[^>]+)>:$')
    # Example: "44d94:	f893 0068 	ldrb.w	r0, [r3, #104]	; 0x68"
    # Assume there is always a "\t" after the hex data.
    disasm_regex = re.compile(r'^(?P<address>[0-9A-Fa-f]+):\s+[0-9A-Fa-f ]+'
                              r'\t\s*(?P<opcode>\S+)(?P<operand>\s+[^;]*)?')

    def DetectFunctionHead(line):
      """Check if the line is a function head.

      Args:
        line: Text of disassembly.

      Returns:
        symbol: Function symbol. None if it isn't a function head.
      """
      result = function_signature_regex.match(line)
      if result is None:
        return None

      address = int(result.group('address'), 16)
      symbol = symbol_map.get(address)

      # Check if the function exists and matches.
      if symbol is None or symbol.symtype != 'F':
        return None

      return symbol

    def ParseInstruction(line, function_end):
      """Parse the line of instruction.

      Args:
        line: Text of disassembly.
        function_end: End address of the current function. None if unknown.

      Returns:
        (address, opcode, operand_text): The instruction address, opcode,
                                         and the text of operands. None if it
                                         isn't an instruction line.
      """
      result = disasm_regex.match(line)
      if result is None:
        return None

      address = int(result.group('address'), 16)
      # Check if it's out of bound.
      if function_end is not None and address >= function_end:
        return None

      opcode = result.group('opcode').strip()
      operand_text = result.group('operand')
      if operand_text is None:
        operand_text = ''
      else:
        operand_text = operand_text.strip()

      return (address, opcode, operand_text)

    # Build symbol map, indexed by symbol address.
    symbol_map = {}
    for symbol in self.symbols:
      # If there are multiple symbols with same address, keeping any of them is
      # good enough.
      symbol_map[symbol.address] = symbol

    # Parse the disassembly text. We update the variable "line" to next line
    # when needed. There are two steps of parser:
    #
    # Step 1: Searching for the function head. Once reach the function head,
    # move to the next line, which is the first line of function body.
    #
    # Step 2: Parsing each instruction line of function body. Once reach a
    # non-instruction line, stop parsing and analyze the parsed instructions.
    #
    # Finally turn back to the step 1 without updating the line, because the
    # current non-instruction line can be another function head.
    function_map = {}
    # The following three variables are the states of the parsing processing.
    # They will be initialized properly during the state changes.
    function_symbol = None
    function_end = None
    instructions = []

    # Remove heading and tailing spaces for each line.
    disasm_lines = list(line.strip() for line in disasm_text.splitlines())
    line_index = 0
    while line_index < len(disasm_lines):
      # Get the current line.
      line = disasm_lines[line_index]

      if function_symbol is None:
        # Step 1: Search for the function head.

        function_symbol = DetectFunctionHead(line)
        if function_symbol is not None:
          # Assume there is no empty function. If the function head is followed
          # by EOF, it is an empty function.
          assert line_index + 1 < len(disasm_lines)

          # Found the function head, initialize and turn to the step 2.
          instructions = []
          # If symbol size exists, use it as a hint of function size.
          if function_symbol.size > 0:
            function_end = function_symbol.address + function_symbol.size
          else:
            function_end = None

      else:
        # Step 2: Parse the function body.

        instruction = ParseInstruction(line, function_end)
        if instruction is not None:
          instructions.append(instruction)

        if instruction is None or line_index + 1 == len(disasm_lines):
          # Either the invalid instruction or EOF indicates the end of the
          # function, finalize the function analysis.

          # Assume there is no empty function.
          assert len(instructions) > 0

          (stack_frame, callsites) = analyzer.AnalyzeFunction(function_symbol,
                                                              instructions)
          # Assume the function addresses are unique in the disassembly.
          assert function_symbol.address not in function_map
          function_map[function_symbol.address] = Function(
              function_symbol.address,
              function_symbol.name,
              stack_frame,
              callsites)

          # Initialize and turn back to the step 1.
          function_symbol = None

          # If the current line isn't an instruction, it can be another function
          # head, skip moving to the next line.
          if instruction is None:
            continue

      # Move to the next line.
      line_index += 1

    # Resolve callees of functions.
    for function in function_map.values():
      for callsite in function.callsites:
        # Remain the callee as None if we can't resolve it.
        callsite.callee = function_map.get(callsite.target)

    return function_map

  def AnalyzeCallGraph(self, function_map):
    """Analyze call graph.

    It will update the max stack size and path for each function.

    Args:
      function_map: Function map.

    Returns:
      SCC groups of the call graph.
    """
    def BuildSCC(function, scc_depth, scc_stack, cycle_groups):
      """Tarjan's strongly connected components algorithm.

      It also calculate the max stack size and path for the function.
      For cycle, we only count the stack size following the traversal order.

      Args:
        function: Current function.
        scc_depth: Depth of recursive traversal.
        scc_stack: Pending functions in the cycle.
        cycle_groups: Groups of function cycles.

      Returns:
        low_depth: The lowest depth can be reached from self and descendants.
      """

      function.scc_depth = scc_depth
      low_depth = function.scc_depth
      scc_stack.append(function)

      # Max stack usage is at least euqal to the stack frame.
      max_stack_usage = function.stack_frame
      max_callee = None
      self_loop = False
      for callsite in function.callsites:
        callee = callsite.callee
        if callee is None:
          continue

        if callee.scc_depth is None:
          # Unvisited descendant.
          low_depth = min(low_depth, BuildSCC(callee,
                                              scc_depth + 1,
                                              scc_stack,
                                              cycle_groups))
        elif not callee.scc_done:
          # Reaches a parent node or self.
          low_depth = min(low_depth, callee.scc_depth)
          if callee is function:
            self_loop = True

        # If the callee is a parent, stack_max_usage will be None.
        callee_stack_usage = callee.stack_max_usage
        if callee_stack_usage is not None:
          if callsite.is_tail:
            # For tailing call, since the callee reuses the stack frame of the
            # caller, choose which one is larger directly.
            stack_usage = max(function.stack_frame, callee_stack_usage)
          else:
            stack_usage = function.stack_frame + callee_stack_usage

          if stack_usage > max_stack_usage:
            max_stack_usage = stack_usage
            max_callee = callee

      if low_depth == function.scc_depth:
        # Group the functions to a new cycle group.
        group_index = len(cycle_groups)
        group = []
        while scc_stack[-1] is not function:
          scc_func = scc_stack[-1]
          scc_stack.pop()
          scc_func.cycle_index = group_index
          group.append(scc_func)

        scc_stack.pop()
        function.cycle_index = group_index

        # If the function is in any cycle (include self loop), add itself to
        # the cycle group. Otherwise its cycle group is empty.
        if len(group) > 0 or self_loop:
          # The function is in a cycle.
          group.append(function)

        cycle_groups.append(group)

      function.scc_done = True

      # Update stack analysis result.
      function.stack_max_usage = max_stack_usage
      function.stack_successor = max_callee

      return low_depth

    cycle_groups = []
    # First use task routines as start points of traversal, so we may get a
    # little more accuracy on max stack size of routines when there are cycles.
    for task in self.tasklist:
      routine_func = function_map[task.routine_address]
      if not routine_func.scc_done:
        BuildSCC(routine_func, 0, [], cycle_groups)

    # Analyze remaining functions which can't be reached from task routines.
    for function in function_map.values():
      if not function.scc_done:
        BuildSCC(function, 0, [], cycle_groups)

    return cycle_groups

  def Analyze(self):
    """Run the stack analysis."""
    # Analyze disassembly.
    try:
      disasm_text = subprocess.check_output([self.options.objdump,
                                             '-d',
                                             self.options.elf_path])
    except subprocess.CalledProcessError:
      raise StackAnalyzerError('objdump failed to disassemble.')
    except OSError:
      raise StackAnalyzerError('Failed to run objdump.')

    function_map = self.AnalyzeDisassembly(disasm_text)
    cycle_groups = self.AnalyzeCallGraph(function_map)

    # Print the results of task-aware stack analysis.
    for task in self.tasklist:
      routine_func = function_map[task.routine_address]
      print('Task: {}, Max size: {} ({} + {}), Allocated size: {}'.format(
          task.name,
          routine_func.stack_max_usage + INTERRUPT_EXTRA_STACK_FRAME,
          routine_func.stack_max_usage,
          INTERRUPT_EXTRA_STACK_FRAME,
          task.stack_max_size))

      print('Call Trace:')
      curr_func = routine_func
      while curr_func is not None:
        line = self.AddressToLine(curr_func.address)
        output = '\t{} ({}) {:x} [{}]'.format(curr_func.name,
                                              curr_func.stack_frame,
                                              curr_func.address,
                                              line)
        if len(cycle_groups[curr_func.cycle_index]) > 0:
          # If its cycle group isn't empty, it is in a cycle.
          output += ' [cycle]'

        print(output)
        curr_func = curr_func.stack_successor


def ParseArgs():
  """Parse commandline arguments.

  Returns:
    options: Namespace from argparse.parse_args().
  """
  parser = argparse.ArgumentParser(description="EC firmware stack analyzer.")
  parser.add_argument('elf_path', help="the path of EC firmware ELF")
  parser.add_argument('taskinfo_path',
                      help="the path of EC taskinfo generated by Makefile")
  parser.add_argument('--objdump', default='objdump',
                      help='the path of objdump')
  parser.add_argument('--addr2line', default='addr2line',
                      help='the path of addr2line')

  # TODO(cheyuw@google.com): Add a option for dumping stack usage of all
  #                          functions.

  return parser.parse_args()


def ParseSymbolFile(symbol_text):
  """Parse the content of the symbol file.

  Args:
    symbol_text: Text of the symbol file.

  Returns:
    symbols: Symbol list.
  """
  # Example: "10093064 g     F .text  0000015c .hidden hook_task"
  symbol_regex = re.compile(r'^(?P<address>[0-9A-Fa-f]+)\s+[lwg]\s+'
                            r'((?P<type>[OF])\s+)?\S+\s+'
                            r'(?P<size>[0-9A-Fa-f]+)\s+'
                            r'(\S+\s+)?(?P<name>\S+)$')

  symbols = []
  for line in symbol_text.splitlines():
    line = line.strip()
    result = symbol_regex.match(line)
    if result is not None:
      address = int(result.group('address'), 16)
      symtype = result.group('type')
      if symtype is None:
        symtype = 'O'

      size = int(result.group('size'), 16)
      name = result.group('name')
      symbols.append(Symbol(address, symtype, size, name))

  return symbols


def ParseTasklistFile(taskinfo_text, symbols):
  """Parse the task information generated by Makefile.

  Args:
    taskinfo_text: Text of the taskinfo file.
    symbols: Symbol list.

  Returns:
    tasklist: Task list.
  """
  # Example: ("HOOKS",hook_task,LARGER_TASK_STACK_SIZE) ("USB_CHG_P0", ...
  results = re.findall(r'\("([^"]+)", ([^,]+), ([^\)]+)\)', taskinfo_text)
  tasklist = []
  for name, routine_name, stack_max_size in results:
    tasklist.append(Task(name, routine_name, stack_max_size))

  # Resolve routine address for each task. It's more efficient to resolve all
  # routine addresses of tasks together.
  routine_map = dict((task.routine_name, None) for task in tasklist)

  for symbol in symbols:
    # Resolve task routine address.
    if symbol.name in routine_map:
      # Assume the symbol of routine is unique.
      assert routine_map[symbol.name] is None
      routine_map[symbol.name] = symbol.address

  for task in tasklist:
    address = routine_map[task.routine_name]
    # Assume we have resolved all routine addresses.
    assert address is not None
    task.routine_address = address

  return tasklist


def main():
  """Main function."""
  try:
    options = ParseArgs()

    # Generate and parse the symbol file.
    try:
      symbol_text = subprocess.check_output([options.objdump,
                                             '-t',
                                             options.elf_path])
    except subprocess.CalledProcessError:
      raise StackAnalyzerError('objdump failed to dump symbol table.')
    except OSError:
      raise StackAnalyzerError('Failed to run objdump.')

    symbols = ParseSymbolFile(symbol_text)

    # Parse the taskinfo file.
    try:
      with open(options.taskinfo_path, 'r') as taskinfo_file:
        taskinfo_text = taskinfo_file.read()
        tasklist = ParseTasklistFile(taskinfo_text, symbols)

    except IOError:
      raise StackAnalyzerError('Failed to open taskinfo.')

    analyzer = StackAnalyzer(options, symbols, tasklist)
    analyzer.Analyze()
  except StackAnalyzerError as e:
    print('Error: {}'.format(e))


if __name__ == '__main__':
  main()
