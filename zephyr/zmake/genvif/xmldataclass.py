import io
import xml.sax.saxutils

from contextlib import contextmanager
from enum import Enum
from typing import TextIO

import dataclasses

class XmlGenerator:
  def __init__(self, output: TextIO):
    self.output = output
    self.depth = 0

  @property
  def indent(self):
    return '  ' * self.depth

  @contextmanager
  def tag(self, name: str, **attrs):
    if attrs:
      attrs = ''.join(
          ' {}="{}"'.format(name, xml.sax.saxutils.escape(value))
          for name, value in attrs.items()
      )
    else:
      attrs = ''

    print('{}<{}{}>'.format(self.indent, name, attrs), file=self.output)
    self.depth += 1
    yield
    self.depth -= 1
    print('{}</{}>'.format(self.indent, name), file=self.output)

  def text(self, text: str):
    print(self.indent, file=self.output, end='')
    print(xml.sax.saxutils.escape(text), file=self.output)


class XmlDataclass:
  def _output_value(self, name, value, out):
    if isinstance(value, XmlDataclass):
      with out.tag(name):
        value.to_xml(out)
    elif isinstance(value, list):
      for v in value:
        self._output_value(name, v, out)
    elif isinstance(value, bool):
      with out.tag(name, value=('false', 'true')[value]):
        # Textual content may be included to provide context for values,
        # but is not required.
        pass
    elif isinstance(value, int):
      with out.tag(name, value=str(value)):
        # Same as bool, text content is optional.
        pass
    elif isinstance(value, Enum):
      # Enum values are written, not the enum representation. Context in
      # text content is optional.
      with out.tag(name, value=str(value.value)):
        pass
    elif isinstance(value, str):
      with out.tag(name):
        out.text(value)
    elif value is not None:
      raise TypeError('field {} has unsupported type {}'.format(name, type(value).__name__))

  def to_xml(self, out: XmlGenerator):
    for field in dataclasses.fields(self):
      self._output_value(field.name, getattr(self, field.name), out)

