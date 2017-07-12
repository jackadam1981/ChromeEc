#!/usr/bin/env python3
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Statically analyze stack usage of EC firmware.

  Example:
    extra/stack_analyzer/stack_analyzer.py ./build/elm/RW/ec.RW.elf \
        ./build/elm/RW/ec.RW.tasklist
"""

from __future__ import print_function
import argparse
import subprocess
import tempfile
import re


class StackAnalyzerError(Exception):
  """Exception class for stack analyzer utility."""


class Task(object):
  """Task information.

  Attributes:
    name: Task name.
    routine_name: Routine function name.
    routine_address: Resolved routine address. None if it hasn't been resolved.
    stack_config: Stack config.
  """

  def __init__(self, name, routine_name, stack_config, routine_address=None):
    """Constructor.

    Args:
      name: Task name.
      routine_name: Routine function name.
      stack_config: Stack config.
      routine_address: Resolved routine address.
    """
    self.name = name
    self.routine_name = routine_name
    self.routine_address = routine_address
    self.stack_config = stack_config


class Symbol(object):
  """Symbol information.

  Attributes:
    address: Symbol address.
    symtype: Symbol type ('O' or 'F').
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
      is_tail: Is a tailing call?
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
    stack_max: Max stack usage. None if it hasn't been analyzed.
    stack_successor: Successor on the max stack usage path. None if it hasn't
                     been analyzed or it's the end.
    scc_index: Index of the SCC group. None if it hasn't been analyzed.
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
    self.stack_max = None
    self.stack_successor = None
    # Node attributes for Tarjan's strongly connected components algorithm.
    self.scc_depth = None
    self.scc_done = False
    self.scc_index = None


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
  # Stack substraction instructions.
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
      if self.JUMP_OPCODE_RE.match(opcode) is not None:
        target_address = DetectCallsite(operand_text)
        if target_address is not None:
          # Maybe it's a tailing call.
          callsite = Callsite(address, target_address, True)
          if callsite is not None:
            callsites.append(callsite)

      elif self.CALL_OPCODE_RE.match(opcode) is not None:
        target_address = DetectCallsite(operand_text)
        if target_address is not None:
          callsite = Callsite(address, target_address, False)
          if callsite is not None:
            callsites.append(callsite)

      elif self.PUSH_OPCODE_RE.match(opcode) is not None:
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
          # Example: "stmdb sp!, {r4, r5, r6, r7, r8, r9, lr}"
          stack_frame += (len(operand_text.split(',')) - 1) * 4

    return (stack_frame, callsites)


class StackAnalyzer(object):
  """Class to analyze stack usage.

  Public Methods:
    Analyze: Run the stack analysis.
  """

  def __init__(self, options):
    """Constructor.

    It will parse the symbol file and tasklist file to initialize the symbol
    lsit and task list.

    Args:
      options: Namespace from argparse.parse_args().

    Raises:
      StackAnalyzerError: If initialization is failed.
    """
    self.options = options

    with tempfile.TemporaryFile(mode='w+') as symbol_file:
      try:
        subprocess.check_call([options.objdump, '-t', options.elf_path],
                              stdout=symbol_file)
      except subprocess.SubprocessError:
        raise StackAnalyzerError('objdump failed to dump symbol table.')
      except OSError:
        raise StackAnalyzerError('Failed to run objdump.')

      symbol_file.seek(0)
      self.symbols = self.ParseSymbolFile(symbol_file)

    try:
      with open(options.tasklist_path, 'r') as tasklist_file:
        self.tasklist = self.ParseTasklistFile(tasklist_file, self.symbols)

    except OSError:
      raise StackAnalyzerError('Failed to open tasklist.')

  @staticmethod
  def ParseSymbolFile(symbol_file):
    """Parse the symbol file.

    Args:
      symbol_file: Symbol file.

    Returns:
      symbols: Symbol list.
    """
    # Example: "10093064 g     F .text  0000015c .hidden hook_task"
    symbol_regex = re.compile(r'^(?P<address>[0-9A-Fa-f]+)\s+[lwg]\s+'
                              r'((?P<type>[OF])\s+)?\S+\s+'
                              r'(?P<size>[0-9A-Fa-f]+)\s+'
                              r'(\S+\s+)?(?P<name>\S+)$')

    symbols = []
    for line in symbol_file:
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

  @staticmethod
  def ParseTasklistFile(tasklist_file, symbols):
    """Parse the tasklist file generated by Makefile.

    Args:
      tasklist_file: Tasklist file.
      symbols: Symbol list.

    Returns:
      tasklist: Task list.
    """
    tasklist_data = tasklist_file.read()
    # Example: ("HOOKS",hook_task,LARGER_TASK_STACK_SIZE) ("USB_CHG_P0", ...
    results = re.findall(r'\("([^"]+)", ([^,]+), ([^\)]+)\)', tasklist_data)
    tasklist = []
    for name, routine_name, stack_config in results:
      tasklist.append(Task(name, routine_name, stack_config))

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
      (line_data, _) = process.communicate()
      if process.returncode != 0:
        raise StackAnalyzerError('addr2line failed to resolve lines.')

    except OSError:
      raise StackAnalyzerError('Failed to run addr2line.')

    line = line_data.decode('utf-8').strip()
    return line

  def AnalyzeDisassembly(self, disasm_file):
    """Parse the disassembly file, analyze, and build a map of all functions.

    Args:
      disasm_file: Disassembly file.

    Returns:
      function_map: Dict of functions.
    """
    # TODO(cheyuw@google.com): Selecting analyzer bases on architecture.
    analyzer = ArmAnalyzer()

    # Example: "08028c8c <motion_lid_calc>:"
    function_signature_regex = re.compile(
        r'^(?P<address>[0-9A-Fa-f]+)\s+<(?P<name>[^>]+)>:$')
    # Example: "44d94:	f893 0068 	ldrb.w	r0, [r3, #104]	; 0x68"
    # Assume there is always a "\t" after the hex data.
    disasm_regex = re.compile(r'^(?P<address>[0-9A-Fa-f]+):\s+[0-9A-Fa-f ]+'
                              r'\t\s*(?P<opcode>\S+)(?P<operand>\s+.*)?$')

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
      # If there're multiple symbols with same address, keep any one is ok.
      symbol_map[symbol.address] = symbol

    # Parse the disassembly file. We update the variable "line" to next line
    # when needed. There are two steps of parser:
    #
    # Step 1: Searching for the function head. Once reach the function head,
    # move to the next line, which is the first line of function body.
    #
    # Step 2: Parsing each instruction line of function body. Once reach a
    # non-instruction line, stop parsing and analyze the parsed instructions.
    #
    # Finally turn back to the step 1 without update the line, because the
    # current non-instruction line can be another function head.
    function_map = {}
    line = next(disasm_file, None)
    while line is not None:
      function_symbol = None

      # Search for the function head.
      while line is not None:
        line = line.strip()
        symbol = DetectFunctionHead(line)
        if symbol is not None:
          function_symbol = symbol
          break

        line = next(disasm_file, None)

      # No more function.
      if function_symbol is None:
        break

      # Found the function head.
      function_address = function_symbol.address

      # If symbol size exists, use it as a hint of function size.
      if function_symbol.size > 0:
        function_end = function_address + function_symbol.size
      else:
        function_end = None

      # Move to the next line of function head.
      line = next(disasm_file, None)
      instructions = []
      # Parse the function body.
      while line is not None:
        line = line.strip()
        instruction = ParseInstruction(line, function_end)
        if instruction is None:
          break

        instructions.append(instruction)
        line = next(disasm_file, None)

      (stack_frame, callsites) = analyzer.AnalyzeFunction(function_symbol,
                                                          instructions)
      # Assume there are no two functions at the same address in disassembly.
      assert function_address not in function_map
      function_map[function_address] = Function(function_address,
                                                function_symbol.name,
                                                stack_frame,
                                                callsites)

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

      max_substack = 0
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

        # If the callee is a parent, stack_max will be None.
        substack = callee.stack_max
        if substack is not None:
          if callsite.is_tail:
            # Calibrate the stack size of tailing call.
            substack = max(0, substack - function.stack_frame)

          if substack > max_substack:
            max_substack = substack
            max_callee = callee

      if low_depth == function.scc_depth:
        # Group the functions to a new cycle group.
        group_index = len(cycle_groups)
        group = []
        while scc_stack[-1] is not function:
          func = scc_stack[-1]
          scc_stack.pop()
          func.scc_index = group_index
          group.append(func)

        scc_stack.pop()
        function.scc_index = group_index

        # If the function is in any cycle (include self loop), add itself to
        # the cycle group. Otherwise its cycle group is empty.
        if len(group) > 0 or self_loop:
          # The function is in a cycle.
          group.append(function)

        cycle_groups.append(group)

      function.scc_done = True

      # Update stack analysis result.
      function.stack_max = function.stack_frame + max_substack
      function.stack_successor = max_callee

      return low_depth

    cycle_groups = []
    # First use task routines as start points of traversal, so we may get a
    # little more accuracy on max stack size of routines when there are cycles.
    for task in self.tasklist:
      routine_func = function_map[task.routine_address]
      if not routine_func.scc_done:
        BuildSCC(routine_func, 0, [], cycle_groups)

    # Analyze remaining functions.
    for function in function_map.values():
      if not function.scc_done:
        BuildSCC(function, 0, [], cycle_groups)

    return cycle_groups

  def Analyze(self):
    """Run the stack analysis."""

    # Analyze disassembly.
    with tempfile.TemporaryFile(mode='w+') as disasm_file:
      try:
        subprocess.check_call([self.options.objdump,
                               '-d',
                               self.options.elf_path],
                              stdout=disasm_file)
      except subprocess.SubprocessError:
        raise StackAnalyzerError('objdump failed to disassemble.')
      except OSError:
        raise StackAnalyzerError('Failed to run objdump.')

      disasm_file.seek(0)
      function_map = self.AnalyzeDisassembly(disasm_file)

    cycle_groups = self.AnalyzeCallGraph(function_map)

    # Print the results of task-aware stack analysis.
    for task in self.tasklist:
      routine_func = function_map[task.routine_address]
      print('Task: {}, Max size: {} ({} + 64), Allocated size: {}'.format(
          task.name, routine_func.stack_max + 64, routine_func.stack_max,
          task.stack_config))

      print('Call Trace:')
      curr_func = routine_func
      while curr_func is not None:
        line = self.AddressToLine(curr_func.address)
        output = '\t{} ({}) {:x} [{}]'.format(curr_func.name,
                                              curr_func.stack_frame,
                                              curr_func.address,
                                              line)
        if len(cycle_groups[curr_func.scc_index]) > 0:
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
  parser.add_argument('tasklist_path',
                      help="the path of EC tasklist generated by Makefile")
  parser.add_argument('--objdump', default='objdump',
                      help='the path of objdump')
  parser.add_argument('--addr2line', default='addr2line',
                      help='the path of addr2line')

  # TODO(cheyuw@google.com): Add a option for dumping stack usage of all
  #                          functions.

  return parser.parse_args()


def main():
  """Main function."""
  try:
    options = ParseArgs()
    analyzer = StackAnalyzer(options)
    analyzer.Analyze()
  except StackAnalyzerError as e:
    print('Error: {}'.format(e))


if __name__ == '__main__':
  main()
