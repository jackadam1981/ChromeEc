#!/usr/bin/python2
# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

class HtmlTag(object):
  """Class that represents a single html tag

  Attributes:
    tag: String holding text of the tag
    empty: boolean indicating whether tag is empty (i.e. <br>) and
      has no closing tag
    content: List of content inside this tag. Meaningless if empty is true
    attr: Dictionary of tag attributes
  """
  def __init__(self, tag, empty=False, contents=None, attr=None):
    """Initializes the HtmlTag object
      Args:
        tag: String holding the actual text of the tag, i.e. 'body', 'html', etc.
        empty: boolean, true if tag holds no content
        contents: list of the contents of the tag or string or HtmlTag obj to put
          in contents
        """
    self.tag = tag
    self.empty = empty
    if type(contents) == type([]):
      self.contents = contents
    elif type(contents) == type('') or type(contents) == type(HtmlTag):
      self.contents = [contents]
    else:
      self.contents = []
    self.attr = attr if type(attr) == type({}) else {}

  def appendContent(self, content):
    """Append content to end of tag's content list

    Args:
      content: Either a String or an HtmlTag object,
         or a list of those to append one by one"""
    if type(content) == type(self) or type(content) == type(''):
      self.contents.append(content)
    elif type(content) == type([]):
      self.contents = self.contents + content
    else:
      raise ValueError("Could not add content, wrong type")

  def toString(self):
    """Returns a string created from this tag and its contents recursively

      Return: String containing this tag and inner contents"""
    res = '<' + self.tag
    for att, val in self.attr.items():
      res = res + ' ' + att
      if val != None:
        res = res + '=\"' + val + '\"'
    res = res + '>'
    if self.empty:
      return res
    for item in self.contents:
      if type(item) == type(''):
        res = res + item
      else:
        res = res + item.toString()
    res = res + '</' + self.tag + '>'
    return res

class HtmlDoc(object):
  """Class that represents a basic html doc

  Attributes:
    tags: List of outermost tags in the HtmlDoc interface
  """
  def __init__(self, tags=[]):
    self.tags = tags

  def setTags(self, tags):
    if type(tags) != type([]):
      raise ValueError("Please insert list only")
    else:
      self.tags = tags

  def getTags(self):
    return self.tags

  def appendTag(self, tag):
    if type(tag) != type(HtmlTag):
      raise ValueError("Please insert HtmlTag object only")
    else:
      self.tags.append(tag)

  def toString(self):
    res = ''
    for item in self.tags:
      res = res + item.toString()
    return res

  @staticmethod
  def basicDoc(content):
    """Wraps content with basic html and body tags, adds doctype

    Args:
      content: list of HtmlTags or Strings to put in body,
        or a single one either"""
    dt = HtmlTag('!DOCTYPE', empty=True, attr={'html':None})
    ht = HtmlTag('html')
    body = HtmlTag('body')
    body.appendContent(content)
    ht.appendContent(body)
    doc = HtmlDoc([dt, ht])
    return doc
