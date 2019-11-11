import itertools
from collections import namedtuple

PortState = namedtuple('PortState', ('power', 'name'))

STATES = (
    # USB-C
    (PortState(0, 'unused'), PortState(2090, 'RP_1A5'), PortState(3740, 'RP_3A0')),
    # Front USB-A
    (PortState(0, 'none'), PortState(1600, '1 HP'), PortState(1600 + 960, '2 HP'), PortState(960 * 2, '2 LP')),
    (PortState(0, 'none'), PortState(1075, '1'), PortState(1075 * 2, '2'), PortState(1075 * 3, 'all')),
    (PortState(0, 'none'), PortState(550, '1'), PortState(1100, 'both')),
)

print('Overcurrent states:')
print(''.join('{: ^12}'.format(port) for port in ('USB-C', 'Front USB-A', 'Rear USB-A', 'HDMI')))

for state in itertools.product(*STATES):
    if sum(s.power for s in state) > (10000 - (550 + 285 + 500)):
        print(''.join('{: >12}'.format(s.name) for s in state))
