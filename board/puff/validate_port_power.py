# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import itertools
import pygraphviz as pgv
from collections import defaultdict, deque
from dataclasses import dataclass, replace
from enum import Enum


class TypeCCurrentLimit(Enum):
  RP_1A5 = 2090
  RP_3A0 = 3740


@dataclass(frozen=True)
class TypeC:
  limit: TypeCCurrentLimit = TypeCCurrentLimit.RP_3A0
  in_use: bool = False

  def successors(self):
    yield replace(self, in_use=not self.in_use)

  @property
  def power(self):
    if not self.in_use:
      return 0
    return self.limit.value

  def __str__(self):
    return '{: >3} {}({: >4})'.format(
        'on' if self.in_use else 'off',
        self.limit.name,
        self.power
    )


@dataclass(frozen=True)
class TypeAFront:
  PORT_COUNT = 2
  LOW_POWER = 963
  HIGH_POWER = 1603

  limited: bool = False
  in_use: int = 0

  def successors(self):
    if self.in_use > 0:
      yield replace(self, in_use=self.in_use - 1)
    if self.in_use < self.PORT_COUNT:
      yield replace(self, in_use=self.in_use + 1)

  @property
  def power(self):
    if not self.limited:
      return {
          0: 0,
          1: self.HIGH_POWER,
          2: self.HIGH_POWER + self.LOW_POWER
      }[self.in_use]
    else:
      return self.LOW_POWER * self.in_use

  def __str__(self):
    return '{} {}({: >4})'.format(
        self.in_use, 'LP' if self.limited else 'HP',
        self.power
    )


@dataclass(frozen=True)
class TypeARear:
  PORT_COUNT = 3
  POWER = 1075

  in_use: int = 0

  def successors(self):
    if self.in_use > 0:
      yield replace(self, in_use=self.in_use - 1)
    if self.in_use < self.PORT_COUNT:
      yield replace(self, in_use=self.in_use + 1)

  @property
  def power(self):
    return self.POWER * self.in_use

  def __str__(self):
    return f'{self.in_use}({self.power: >4})'


@dataclass(frozen=True)
class Hdmi:
  PORT_COUNT = 2
  POWER = 562

  in_use: int = 0

  def successors(self):
    if self.in_use > 0:
      yield replace(self, in_use=self.in_use - 1)
    if self.in_use < self.PORT_COUNT:
      yield replace(self, in_use=self.in_use + 1)

  @property
  def power(self):
    return self.POWER * self.in_use

  def __str__(self):
    return f'{self.in_use}({self.power: >4})'


@dataclass(frozen=True)
class State:
  POWER_LIMIT = 10000

  type_c: TypeC = TypeC()
  type_a_front: TypeAFront = TypeAFront()
  type_a_rear: TypeARear = TypeARear()
  hdmi: Hdmi = Hdmi()

  def successors(self):
    yield from (replace(self, type_c=tc) for tc in self.type_c.successors())
    yield from (replace(self, type_a_front=ta) for ta in
                self.type_a_front.successors())
    yield from (replace(self, type_a_rear=ta) for ta in
                self.type_a_rear.successors())
    yield from (replace(self, hdmi=hdmi) for hdmi in self.hdmi.successors())

  def apply_ec_control(self, states):
    limit_front_a: bool
    limit_c: TypeCCurrentLimit

    headroom = self.POWER_LIMIT - self.total_power
    if not self.type_c.in_use:
      if (headroom > TypeCCurrentLimit.RP_3A0.value
          + (TypeAFront.HIGH_POWER - TypeAFront.LOW_POWER)):
        limit_front_a = False
        limit_c = TypeCCurrentLimit.RP_3A0
      else:
        limit_front_a = (
            headroom < TypeCCurrentLimit.RP_1A5.value
            + (TypeAFront.HIGH_POWER - TypeAFront.LOW_POWER)
        )
        limit_c = TypeCCurrentLimit.RP_1A5
    else:
      if headroom > (TypeCCurrentLimit.RP_3A0.value
                     - TypeCCurrentLimit.RP_1A5.value
                     + TypeAFront.HIGH_POWER):
        limit_front_a = False
        limit_c = TypeCCurrentLimit.RP_3A0
      elif headroom > (TypeCCurrentLimit.RP_3A0.value
                       - TypeCCurrentLimit.RP_1A5.value
                       + TypeAFront.LOW_POWER):
        limit_front_a = True
        limit_c = TypeCCurrentLimit.RP_3A0
      else:
        limit_front_a = True
        limit_c = TypeCCurrentLimit.RP_1A5

    # Take possible port changes and EC-applied policy
    states = set(
      replace(s, type_a_front=replace(s.type_a_front, limited=limit_front_a),
              type_c=replace(s.type_c, limit=limit_c))
      for s in itertools.chain((self,), (s for s in states))
    )
    yield from states


  @property
  def total_power(self):
    return (
        1335 + self.type_c.power + self.type_a_front.power
        + self.type_a_rear.power + self.hdmi.power
    )

  @property
  def is_legal(self):
    return self.total_power < self.POWER_LIMIT

  @property
  def color(self):
    return 'black' if self.is_legal else 'red'

  def __str__(self):
    return (
        f'{self.total_power} C {self.type_c}, A-F {self.type_a_front}, '
        f'A-R {self.type_a_rear}, HDMI {self.hdmi}'
    )


def main():
  seen = set()
  edges = defaultdict(set)
  q = deque([State()])
  forbidden_edges = defaultdict(set)

  # Walk states reachable according to controls, marking edges that lead to
  # illegal states.
  while len(q) > 0:
    state = q.popleft()
    for next in state.apply_ec_control(state.successors()):
      if next.is_legal and next not in seen:
        q.append(next)
      seen.add(next)
      edges[state].add(next)
      if not next.is_legal:
        forbidden_edges[state].add(next)

  print(len(seen), 'reachable states with',
        sum(map(len, edges.values())), 'edges')
  # Dangerous states are those that have outgoing forbidden edges; a forbidden
  # edge leads to an illegal state.
  print(len(forbidden_edges), 'dangerous states with',
        sum(map(len, forbidden_edges.values())), 'forbidden edges')

  # Generate a picture of dangerous states
  display_nodes = {
      node: n
      for (n, node) in enumerate(
          itertools.chain(forbidden_edges.keys(), *forbidden_edges.values())
      )
  }
  G = pgv.AGraph(directed=True)
  for node, id in display_nodes.items():
    G.add_node(id, label=str(node), color=node.color)
  for (source, dests) in forbidden_edges.items():
    for dest in dests:
      G.add_edge(display_nodes[source], display_nodes[dest], color=dest.color)
  G.draw('states.svg', prog='fdp')


if __name__ == '__main__':
  main()
