#!/usr/bin/env python

# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# A script to pack EC binary into SPI flash image for MEC1322
# Based on MEC1322_ROM_Doc_Rev0.5.pdf.
from __future__ import print_function

import argparse
import hashlib
import os
import struct
import subprocess
import tempfile




# MEC1701 has 256KB SRAM from 0xE0000 - 0x120000
# SRAM is divided into contiguous CODE & DATA
# CODE at [0xE0000, 0x117FFF] DATA at [0x118000, 0x11FFFF]
# SPI flash size for board is 512KB
# Boot-ROM TAG is located at SPI offset 0 (two 4-byte tags)
#

LFW_SIZE = 0x1000
LOAD_ADDR = 0x0E0000
LOAD_ADDR_RW = 0xE1000
HEADER_SIZE = 0x40
SPI_CLOCK_LIST = [48, 24, 16, 12]
SPI_READ_CMD_LIST = [0x3, 0xb, 0x3b, 0x6b]

CRC_TABLE = [0x00, 0x07, 0x0e, 0x09, 0x1c, 0x1b, 0x12, 0x15,
             0x38, 0x3f, 0x36, 0x31, 0x24, 0x23, 0x2a, 0x2d]

def Crc8(crc, data):
  """Update CRC8 value."""
  data_bytes = map(lambda b: ord(b) if isinstance(b, str) else b, data)
  for v in data_bytes:
    crc = ((crc << 4) & 0xff) ^ (CRC_TABLE[(crc >> 4) ^ (v >> 4)]);
    crc = ((crc << 4) & 0xff) ^ (CRC_TABLE[(crc >> 4) ^ (v & 0xf)]);
  return crc ^ 0x55

def GetEntryPoint(payload_file):
  """Read entry point from payload EC image."""
  with open(payload_file, 'rb') as f:
    f.seek(4)
    s = f.read(4)
  return struct.unpack('<I', s)[0]

def GetPayloadFromOffset(payload_file,offset):
  """Read payload and pad it to 64-byte aligned."""
  with open(payload_file, 'rb') as f:
    f.seek(offset)
    payload = bytearray(f.read())
  rem_len = len(payload) % 64
  if rem_len:
    payload += '\0' * (64 - rem_len)
  return payload

def GetPayload(payload_file):
  """Read payload and pad it to 64-byte aligned."""
  return GetPayloadFromOffset(payload_file, 0)

def GetPublicKey(pem_file):
  """Extract public exponent and modulus from PEM file."""
  s = subprocess.check_output(['openssl', 'rsa', '-in', pem_file,
                               '-text', '-noout'])
  modulus_raw = []
  in_modulus = False
  for line in s.split('\n'):
    if line.startswith('modulus'):
      in_modulus = True
    elif not line.startswith(' '):
      in_modulus = False
    elif in_modulus:
      modulus_raw.extend(line.strip().strip(':').split(':'))
    if line.startswith('publicExponent'):
      exp = int(line.split(' ')[1], 10)
  modulus_raw.reverse()
  modulus = bytearray(''.join(map(lambda x: chr(int(x, 16)),
                                  modulus_raw[0:256])))
  return struct.pack('<Q', exp), modulus

def GetSpiClockParameter(args):
  assert args.spi_clock in SPI_CLOCK_LIST, \
         "Unsupported SPI clock speed %d MHz" % args.spi_clock
  return SPI_CLOCK_LIST.index(args.spi_clock)

def GetSpiReadCmdParameter(args):
  assert args.spi_read_cmd in SPI_READ_CMD_LIST, \
         "Unsupported SPI read command 0x%x" % args.spi_read_cmd
  return SPI_READ_CMD_LIST.index(args.spi_read_cmd)

def PadZeroTo(data, size):
  data.extend('\0' * (size - len(data)))

def BuildHeader(args, payload_len, load_addr, rorofile):
  # Identifier and header version
  header = bytearray(['P', 'H', 'C', 'M', '\0'])

  # byte[5]
  b = GetSpiClockParameter(args)
  b |= (1 << 2)
  header.append(b)

  # byte[6]
  b = 0
  header.append(b)

  # byte[7]
  header.append(GetSpiReadCmdParameter(args))

  # bytes 0x08 - 0x0b
  header.extend(struct.pack('<I', load_addr))
  # bytes 0x0c - 0x0f
  header.extend(struct.pack('<I', GetEntryPoint(rorofile)))
  # bytes 0x10 - 0x13
  header.append((payload_len >> 6) & 0xff)
  header.append((payload_len >> 14) & 0xff)
  PadZeroTo(header, 0x14)
  # bytes 0x14 - 0x17
  header.extend(struct.pack('<I', args.payload_offset))

  # bytes 0x14 - 0x3F all 0
  PadZeroTo(header, 0x40)

  # header signature is appended by the caller

  return header


def BuildHeader2(args, payload_len, load_addr, payload_entry):
  # Identifier and header version
  header = bytearray(['P', 'H', 'C', 'M', '\0'])

  # byte[5]
  b = GetSpiClockParameter(args)
  b |= (1 << 2)
  header.append(b)

  # byte[6]
  b = 0
  header.append(b)

  # byte[7]
  header.append(GetSpiReadCmdParameter(args))

  # bytes 0x08 - 0x0b
  header.extend(struct.pack('<I', load_addr))
  # bytes 0x0c - 0x0f
  header.extend(struct.pack('<I', payload_entry))
  # bytes 0x10 - 0x13
  header.append((payload_len >> 6) & 0xff)
  header.append((payload_len >> 14) & 0xff)
  PadZeroTo(header, 0x14)
  # bytes 0x14 - 0x17
  header.extend(struct.pack('<I', args.payload_offset))

  # bytes 0x14 - 0x3F all 0
  PadZeroTo(header, 0x40)

  # header signature is appended by the caller

  return header

#
# MEC1701H with ECDSA Authentication
# Sign data using ECDSA. Key = Private EC key on curve P-256.
# For versions of MEC1701H with ECDSA disabled the 64-byte
# signature is the SHA-256 of the object:
# bytes[0:31] = SHA-256
# bytes[32:63] = all 0
#
def SignByteArray(data, pem_file):
  hasher = hashlib.sha256()
  hasher.update(data)
  h = hasher.digest()
  bah = bytearray(h)
  bah.extend("\0" * 32)
  return bah

#  hash_file = tempfile.mkstemp(prefix='pack_ec.')[1]
#  sign_file = tempfile.mkstemp(prefix='pack_ec.')[1]
#  try:
#      with open(hash_file, 'wb') as f:
#        hasher = hashlib.sha256()
#        hasher.update(data)
#        f.write(hasher.digest())
#      subprocess.check_call(['openssl', 'rsautl', '-sign', '-inkey', pem_file,
#                             '-keyform', 'PEM', '-in', hash_file,
#                             '-out', sign_file])
#      with open(sign_file, 'rb') as f:
#        signed = list(f.read())
#        signed.reverse()
#        return bytearray(''.join(signed))
#  finally:
#    os.remove(hash_file)
#    os.remove(sign_file)


# MEC1701H supports two 32-bit Tags located at offsets 0x0 and 0x4
# in the SPI flash.
# Tag format:
#   bits[23:0] correspond to bits[31:8] of the Header SPI address
#       Header is always on a 256-byte boundary.
#   bits[31:24] = CRC8-ITU of bits[23:0].
# Notice there is not chip-select field in the Tag both Tag's point
# to the same flash part.
#
def BuildTag(args):
  tag = bytearray([(args.header_loc >> 8) & 0xff,
                   (args.header_loc >> 16) & 0xff,
                   (args.header_loc >> 24) & 0xff])
  tag.append(Crc8(0, tag))
  return tag


# rwrw_file   = args.input = ec.bin.tmp1 521216 = 0x7F400 = 0x80000 - 0x0C00(3KB)
# loader_file = args.loader_file = build/glados_mec1701/RW/chip/mec1701/lfw/ec_lfw-lfw.flat 3072 bytes
# image_size  = args.image_size = 196608 = 0x30000 = 192KB
def PacklfwRoImage(rorw_file, loader_file, image_size):
  """TODO:Clean up to get rid of Temp file and just use memory
  to save data"""
  """Create a temp file with the
  first image_size bytes from the loader file and append bytes
  from the rorw file.
  return the filename"""
  fo=tempfile.NamedTemporaryFile(delete=False) # Need to keep file around
  with open(loader_file,'rb') as fin1: # read 3KB loader file
    pro = fin1.read()
  fo.write(pro)                         # write 3KB loader data to temp file
  with open(rorw_file, 'rb') as fin:    # read EC_RO already padded with 0xFF to length 0x2F000 by build process.
    ro = fin.read(image_size)
  fo.write(ro)                          # append EC_RO to 3KB loader
  fo.close()
  return fo.name


def parseargs():
  parser = argparse.ArgumentParser()
  parser.add_argument("-i", "--input",
                      help="EC binary to pack, usually ec.bin or ec.RO.flat.",
                      metavar="EC_BIN", default="ec.bin")
  parser.add_argument("-o", "--output",
                      help="Output flash binary file",
                      metavar="EC_SPI_FLASH", default="ec.packed.bin")
  parser.add_argument("--header_key",
                      help="PEM key file for signing header",
                      default="rsakey_sign_header.pem")
  parser.add_argument("--payload_key",
                      help="PEM key file for signing payload",
                      default="rsakey_sign_payload.pem")
  parser.add_argument("--loader_file",
                      help="EC loader binary",
                      default="ecloader.bin")
  parser.add_argument("-s", "--spi_size", type=int,
                      help="Size of the SPI flash in KB",
                      default=1024)
  parser.add_argument("-l", "--header_loc", type=int,
                      help="Location of header in SPI flash",
                      default=0x170000)
  parser.add_argument("-p", "--payload_offset", type=int,
                      help="The offset of payload from the header",
                      default=0x80)
  parser.add_argument("-r", "--rwpayload_loc", type=int,
                      help="The offset of payload from the header",
                      default=0x190000)
  parser.add_argument("-z", "--romstart", type=int,
                      help="The first location to output of the rom",
                      default=0)
  parser.add_argument("--spi_clock", type=int,
                      help="SPI clock speed. 8, 12, 24, or 48 MHz.",
                      default=24)
  parser.add_argument("--spi_read_cmd", type=int,
                      help="SPI read command. 0x3, 0xB, or 0x3B.",
                      default=0xb)
  parser.add_argument("--image_size", type=int,
                      help="Size of a single image.",
                      default=(192 * 1024))
  return parser.parse_args()

# Debug helper routine
def dumpsects(spi_list):
  for s in spi_list:
    #print "%x %d %s\n"%(s[0],len(s[1]),s[2])
    print("0x{0:x} 0x{1:x} {2:s}".format(s[0],len(s[1]),s[2]))

def printByteArrayAsHex(ba, title):
  print(title,"= ")
  count = 0
  for b in ba:
    count = count + 1
    print("0x{0:02x}, ".format(b),end="")
    if (count % 8) == 0:
	print("")

#
# called from chip/build.mk
# pack_ec.py -o build/glados_mec1701/ec.bin.tmp -i build/glados_mec1701/ec.bin.tmp1
#   --loader_file build/glados_mec1701/RW/chip/mec1701/ec_lfw-lfw.flag
#   --payload_key ./chip/mec1701/util/rsakey_sign_payload.pem
#   --header_key ./chip/mec1701/util/rsakey_sign_header.pem
#   --spi_size 512
#   --image_size 196608
#
def main():
  print("Begin MEC1701 pack_ec.py script")
  args = parseargs()

  # TODO MEC17xx maximum 192KB each for RO & RW
  # mec1701 chip Makefile sets args.spi_size = 512
  # glados_mec1701 board Makefile override sets args.spi_size = 512
  # Tags at offset 0
  # Header at offset 4096 (next 4KB sector)
  #
  #
  print("args.spi_size in KB =",args.spi_size)

  spi_size = args.spi_size * 1024
  # !!! IMPORTANT !!!
  # These values MUST match chip/mec1701/config_flash_layout.h
  # defines.
  #args.header_loc = spi_size - (192 * 1024)
  #args.rwpayload_loc = spi_size - (384 * 1024)
  # loader + EC_RO starts at beginning of second 4KB sector
  # EC_RW starts at offset 0x40000 (256KB)
  args.header_loc = 0x1000
  args.rwpayload_loc = 0x40000
  # MEC1701 Boot-ROM TAGs are at offset 0 and 4.
  args.romstart = 0

  spi_list = []

 # args.input = ec.bin.tmp1
 # args.loader_file = ec_lfw-lfw.flat
 # args.image_size = 196608 = 0x30000 = 192KB
  print("args.input = ",args.input)
  print("args.loader_file = ",args.loader_file)
  print("args.image_size = ",hex(args.image_size))
  rorofile=PacklfwRoImage(args.input, args.loader_file, args.image_size)
  payload = GetPayload(rorofile)
  payload_len = len(payload)
  # debug
  print("EC_LFW + EC_RO length = ",hex(payload_len))
  # MEC17xx TODO signing is ECDSA. We implemented the ECDSA disabled case where
  # the 64-byte signate contains a SHA-256 of the binary.
  payload_signature = SignByteArray(payload, args.payload_key)
  # MEC17xx TODO Header is 0x80 bytes and very different (ECDSA signature)
  header = BuildHeader(args, payload_len, LOAD_ADDR, rorofile)
  # debug
  printByteArrayAsHex(header, "Header LFW + EC_RO")

  # MEC17xx TODO payload is signed with same ECDSA key
  header_signature = SignByteArray(header, args.header_key)
  # debug
  printByteArrayAsHex(header_signature, "header_signature")

  tag = BuildTag(args)
  # TODO MEC17xx truncate RW to 192KB
  # offset may be different due to Header size and other changes
  # TODO MCHP we want to append a SHA-256 to the end of the actual payload
  # to test SPI read routines.
  #  payloadrw = GetPayloadFromOffset(args.input,args.image_size)[:256*1024]
  payload_rw = GetPayloadFromOffset(args.input,args.image_size)
  print("type(payload_rw) is ",type(payload_rw))
  print("len(payload_rw) is ",len(payload_rw))
  #payloadrw += sha256_digest_ba
  # Now extend size out to 192KB
  payload_rw = payload_rw[:192*1024]
  payload_rw_len = len(payload_rw)
  print("Extended size of EC_RW = ",hex(payload_rw_len))

  payload_rw_sig = SignByteArray(payload_rw, args.payload_key)

  payload_entry_tuple = struct.unpack_from('<I', payload_rw, 4)
  print("payload_entry_tuple = ",payload_entry_tuple)
  payload_entry = payload_entry_tuple[0]
  print("payload_entry = ",hex(payload_entry))

  header_rw = BuildHeader2(args, payload_rw_len, LOAD_ADDR_RW, payload_entry)

  # debug
  printByteArrayAsHex(header_rw, "Header EC_RW")

  header_rw_sig = SignByteArray(header_rw, args.header_key)

  printByteArrayAsHex(header_rw_sig, "header_rw_sig")


  os.remove(rorofile)           # clean up the temp file

  # MEC1701H Boot-ROM Tags are located at SPI offset 0
  spi_list.append((0, tag, "tag"))

  spi_list.append((args.header_loc, header, "header(lwf + ro)"))
  spi_list.append((args.header_loc + HEADER_SIZE, header_signature, "header(lwf + ro) signature"))
  spi_list.append((args.header_loc + args.payload_offset, payload, "payload(lfw + ro)"))
  spi_list.append((args.header_loc + args.payload_offset + payload_len,
                   payload_signature, "payload(lfw_ro) signature"))

  spi_list.append((args.rwpayload_loc, header_rw, "header(rw)"))
  spi_list.append((args.rwpayload_loc + HEADER_SIZE, header_rw_sig, "header(rw) signature"))
  spi_list.append((args.rwpayload_loc + args.payload_offset, payload_rw, "payload(rw)"))
  spi_list.append((args.rwpayload_loc + args.payload_offset + payload_rw_len,
                   payload_rw_sig, "payload(rw) signature"))

  spi_list = sorted(spi_list)
  # uncomment to debug
  dumpsects(spi_list)

  # Original glados board uses a 512KB flash
  # EC code + Boot-ROM TAG & Header are located in upper 256KB.
  # MEC1701H Boot-ROM locates TAG at SPI offset 0 instead of end of SPI.
  # How do we generate an image with the same tools?
  # Do we locate EC in lower half of SPI flash and ChromiumOS in upper half?
  with open(args.output, 'wb') as f:
    addr = args.romstart
    # debug
    print("args.romstart = ",hex(args.romstart))
    for s in spi_list:
      # debug
      assert addr <= s[0]
      if addr < s[0]:
        f.write('\xff' * (s[0] - addr))
        addr = s[0]
      f.write(s[1])
      addr += len(s[1])
    if addr < spi_size:
      f.write('\xff' * (spi_size - addr))

if __name__ == '__main__':
  main()
