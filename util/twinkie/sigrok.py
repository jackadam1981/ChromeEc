# Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Python-to-C libsigrok interface using the ctypes module."""

from __future__ import print_function

import sys
import imp
import importlib

from ctypes import cdll, CFUNCTYPE, POINTER, byref, cast, Structure, \
                   c_void_p, c_char_p, c_int, c_uint16, c_uint64, py_object
from utils import Log

DRVNAME = "chromium-twinkie"

# Standard Decoders installation path
DECODER_PATH="/usr/share/libsigrokdecode/decoders"

# Hardcoded Twinkie sample rate
TW_SAMPLERATE = 24000000

##### Compatibility layer with libsigrokdecode #####

# Emulate the python resources provided by libsigrokdecode to the decoders
def createSigrokdecodeModule():
  srd = imp.new_module("sigrokdecode")

  #define sigrokdecode module
  srd.Decoder = Decoder
  # enum srd_configkey
  srd.SRD_CONF_SAMPLERATE = 10000
  # enum srd_output_type
  srd.OUTPUT_ANN = 0
  srd.OUTPUT_PYTHON = 1
  srd.OUTPUT_BINARY = 2
  srd.OUTPUT_META = 3

  return srd

class Decoder(object):
  """libsigrokdecode base class for protocol decoders."""
  def register(self, outtype, meta=None):
    self._log.Debug("%s|Register: %s (%s)" % (self.id, outtype, meta))

  def put(self, ss, es, outtype, data):
    self._log("%s|PUT %d->%d %s = %s" % (self.id, ss, es, outtype, data))

  def __start__(self, log=None):
    self._log = log if log else Log()
    self.start()
    key = sys.modules['sigrokdecode'].SRD_CONF_SAMPLERATE
    self.metadata(key, TW_SAMPLERATE)

def getDecoder(name):
  # The decoders needs the support from the 'sigrokdecode' module
  if 'sigrokdecode' not in sys.modules:
    sys.modules['sigrokdecode'] = createSigrokdecodeModule()

  fp, pathname, description = imp.find_module(name,[DECODER_PATH])
  decMod = imp.load_module(name, fp, pathname, description)
  return decMod.Decoder()
  #sys.path.append(DECODER_PATH)
  #pkg = importlib.import_module(name)
  #return name.Decoder()

##### interface with the libsigrok dynamic library using ctypes #####

class GSList(Structure):
  """GSList C structure definition from glib."""
  pass
GSList._fields_ = [("data", c_void_p),
                   ("next_", POINTER(GSList))]

nullGSList = POINTER(GSList)()
class Struct_sr_dev_driver(Structure):
  """struct sr_dev_driver C structure definition from libsigrok.h"""
  _fields_ = [("name", c_char_p),
              ("longname", c_char_p),
              ("api_version", c_int),
              ("init", c_void_p),
              ("cleanup", c_void_p),
              ("scan", c_void_p),
              ("dev_list", c_void_p),
              ("config_get", c_void_p),
              ("config_set", c_void_p),
              ("config_channel_set", c_void_p),
              ("config_commit", c_void_p),
              ("config_list", c_void_p),
              ("dev_open", c_void_p),
              ("dev_close", c_void_p),
              ("dev_acquisition_start", c_void_p),
              ("dev_acquisition_stop", c_void_p),
              ("priv", c_void_p)]

class Struct_sr_dev_inst(Structure):
  """struct sr_dev_inst C structure definition from libsigrok.h"""
  _fields_ = [("driver", POINTER(Struct_sr_dev_driver)),
              ("status", c_int),
              ("inst_type", c_int),
              ("vendor", c_char_p),
              ("model", c_char_p),
              ("version", c_char_p),
              ("serial_num", c_char_p),
              ("connection_id", c_char_p),
              ("channels", POINTER(GSList)),
              ("channel_groups", POINTER(GSList)),
              ("conn", c_void_p),
              ("priv", c_void_p),
              ("session", c_void_p)]

class Struct_sr_datafeed_packet(Structure):
  """struct sr_datafeed_packet C structure definition from libsigrok.h"""
  _fields_ = [("type", c_uint16),
              ("payload", c_void_p)]

class Struct_sr_datafeed_logic(Structure):
  """struct sr_datafeed_logic C structure definition from libsigrok.h"""
  _fields_ = [("length", c_uint64),
              ("unitsize", c_uint16),
              ("data", c_void_p)]

Sr_datafeed_callback = CFUNCTYPE(None, POINTER(Struct_sr_dev_inst),
                                 POINTER(Struct_sr_datafeed_packet), py_object)

DF_LOGIC = 10004

class SigrokCapture(object):
  """Capture a USB Power delivery trace using Twinkie through Sigrok."""
  def __init__(self, log=None, decoders=None):
    self.log = log if log else Log()

    self._lib = cdll.LoadLibrary("libsigrok.so.2")
    self._majr = self._lib.sr_package_version_major_get()
    self._minr = self._lib.sr_package_version_minor_get()
    self._micr = self._lib.sr_package_version_micro_get()
    self.log.Debug("VER %d.%d.%d" % (self._majr, self._minr, self._micr))
    self._lib.sr_log_loglevel_set(5)

    self._ctxt = c_void_p()
    ret = self._lib.sr_init(byref(self._ctxt))
    if ret:
      self.log.Error("Invalid Sigrok handle")
      return

    self._drv = None
    drv_list_type = POINTER(POINTER(Struct_sr_dev_driver) * 10)
    self._lib.sr_driver_list.restype = drv_list_type
    drvp = self._lib.sr_driver_list()
    drvs = drvp.contents
    for d in drvs:
      s = d.contents
      if s.name == DRVNAME:
        self._drv = d
    if not self._drv:
      self.log.Error("Cannot find %s driver" % (DRVNAME))
      return
    res = self._lib.sr_driver_init(self._ctxt, self._drv)
    if res:
      self.log.Error("cannot init driver")
      return

    self._lib.sr_driver_scan.restype = POINTER(GSList)
    devl = self._lib.sr_driver_scan(self._drv, nullGSList)
    self._sdi = None
    n = devl
    while n:
      n = n.contents
      self._sdi = cast(n.data, POINTER(Struct_sr_dev_inst))
      break
#      n = n.next_
    if not  self._sdi:
      self.log.Error("NO DEVICE FOUND")
      return

    res = self._lib.sr_dev_open(self._sdi)
    if res:
      self.log.Error("cannot open instance")
      return

    self._sess = c_void_p()
    ret = self._lib.sr_session_new(byref(self._sess))
    if ret:
      self.log.Error("session new FAILED")
      return
    ret = self._lib.sr_session_dev_add(self._sess, self._sdi)
    if ret:
      self.log.Error("session dev add FAILED")
      return

    obj = py_object(self)
    callbk_add = self._lib.sr_session_datafeed_callback_add
    ret = callbk_add(self._sess, Sr_datafeed_callback(self.__datafeed_cb), obj)

    ret = self._lib.sr_session_start(self._sess)

    # instantiate decoders
    for d in decoders:
      d.__start__(log=self.log)

  def private_data_callback(self, _, ppacket):
    if ppacket:
      packet = ppacket.contents
      if packet.type == DF_LOGIC:
        payloadp = cast(packet.payload, POINTER(Struct_sr_datafeed_logic))
        if payloadp:
          payload = payloadp.contents
          self.log.Debug("got %d" % payload.length) #TODO remove me
          #total += payload.length
          #if total > 1000000:
          self.stop()

  @staticmethod
  def __datafeed_cb(sdi, ppacket, cb_data):
    cb_data.private_data_callback(sdi, ppacket)

  def capture(self):
    return self._lib.sr_session_run(self._sess)

  def stop(self):
    return self._lib.sr_session_stop(self._sess)

  def release(self):
    self._lib.sr_exit(self._ctxt)



if __name__ == "__main__":
  bmc = getDecoder("usb_pd_bmc")
  pdpkt = getDecoder("usb_pd_packet")
  #bmc.__start__()
  #pdpkt.__start__()
  #bmc.decode(1,2,[(0,(0,)),(1,(0,)),(2,(0,)),(3,(1,))])

  sr = SigrokCapture(decoders=[bmc, pdpkt])
  sr.capture()
