import io

from enum import Enum
from typing import Optional

from dataclasses import dataclass

from genvif.xmldataclass import XmlDataclass, XmlGenerator

def test_xml_output():
  out = io.StringIO()
  gen = XmlGenerator(out)
  with gen.tag('test', value='foo', duck='&='):
    with gen.tag('sound'):
      gen.text('quack')

  assert out.getvalue() == '''<test value="foo" duck="&amp;=">
  <sound>
    quack
  </sound>
</test>
'''


@dataclass
class InnerDataclass(XmlDataclass):
  answer: int


class MyEnum(Enum):
  A = 0
  B = 1


@dataclass
class MyDataclass(XmlDataclass):
  list: 'list[int]'
  inner: InnerDataclass = InnerDataclass(answer=42)
  bool: bool = True
  int: int = 128
  enum: MyEnum = MyEnum.B
  str: str = 'Test string'
  maybe_int: 'Optional[int]' = None


def test_xmldataclass():
  out = io.StringIO()
  instance = MyDataclass(list=[1, 2, 3])
  instance.to_xml(XmlGenerator(out))

  assert out.getvalue() == '''\
<list value="1">
</list>
<list value="2">
</list>
<list value="3">
</list>
<inner>
  <answer value="42">
  </answer>
</inner>
<bool value="true">
</bool>
<int value="128">
</int>
<enum value="1">
</enum>
<str>
  Test string
</str>
'''
