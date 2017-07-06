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

from six import int2byte


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

def GetPayloadFromOffset(payload_file, offset):
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
# Compute SHA-256 of data and return digest
# as a bytearray
#
def HashByteArray(data):
  hasher = hashlib.sha256()
  hasher.update(data)
  h = hasher.digest()
  bah = bytearray(h)
  return bah

#
# Return 64-byte signature of byte array data.
# If signing_key is a ECDSA SigningKey object
#   Sign data using ECDSA with RFC6979 deterministic algorithm.
# If signing_key is None then signature is
#   SHA256(data) || 32 bytes of 0.
# Return a bytearray of length 64 bytes containing ECDSA signature
# or SHA256 zero padded to 64 bytes.
#
def SignByteArray(data):
  print("Signature is SHA-256 of data")
  sigb = HashByteArray(data)
  sigb.extend("\0" * 32)
  return sigb

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
# Notice there is no chip-select field in the Tag both Tag's point
# to the same flash part.
#
def BuildTag(args):
  tag = bytearray([(args.header_loc >> 8) & 0xff,
                   (args.header_loc >> 16) & 0xff,
                   (args.header_loc >> 24) & 0xff])
  tag.append(Crc8(0, tag))
  return tag

def BuildTagFromHdrAddr(header_loc):
    tag = bytearray([(header_loc >> 8) & 0xff,
                     (header_loc >> 16) & 0xff,
                     (header_loc >> 24) & 0xff])
    tag.append(Crc8(0, tag))
    return tag


#
# Creates temporary file for read/write
# Reads binary file containing LFW image_size (loader_file)
# Writes LFW image to temporary file
# Reads RO image at beginning of rorw_file up to image_size - 32
#  (assumes RO/RW images have been padded with 0xFF and last 32 bytes are
#   0xFF, not code or rodata)
# If encryption enabled then encrypt RO data length = image_size - 32
# Computes SHA-256 of RO data and appends SHA-256 to end of RO data.
# Write RO data with SHA-256, length = image_len to temporary file
# Returns temporary file name
#
def PacklfwRoImage(rorw_file, loader_file, image_size):
  """Create a temp file with the
  first image_size bytes from the loader file and append bytes
  from the rorw file.
  return the filename"""
  fo=tempfile.NamedTemporaryFile(delete=False) # Need to keep file around
  with open(loader_file,'rb') as fin1: # read 4KB loader file
    pro = fin1.read()
  fo.write(pro)                         # write 4KB loader data to temp file
  with open(rorw_file, 'rb') as fin:
    ro = fin.read(image_size-32)

  # Store SHA-256 in last 32 bytes
  print("Computing SHA-256 of LFW + EC_RO less last 32 bytes")
  h = hashlib.sha256()
  h.update(ro)
  d = h.digest()
  ro = ro + d
  print("After hashing ro image, len(ro) = ", hex(len(ro)))

  fo.write(ro)
  fo.close()
  return fo.name


def parseargs():
  rpath = os.path.dirname(os.path.relpath(__file__))

  parser = argparse.ArgumentParser()
  parser.add_argument("-i", "--input",
                      help="EC binary to pack, usually ec.bin or ec.RO.flat.",
                      metavar="EC_BIN", default="ec.bin")
  parser.add_argument("-o", "--output",
                      help="Output flash binary file",
                      metavar="EC_SPI_FLASH", default="ec.packed.bin")
  parser.add_argument("--loader_file",
                      help="EC loader binary",
                      default="ecloader.bin")
  parser.add_argument("-s", "--spi_size", type=int,
                      help="Size of the SPI flash in KB",
                      default=512)
  parser.add_argument("-l", "--header_loc", type=int,
                      help="Location of header in SPI flash",
                      default=0x1000)
  parser.add_argument("-p", "--payload_offset", type=int,
                      help="The offset of payload from the header",
                      default=0x80)
  parser.add_argument("-r", "--rwheader_loc", type=int,
                      help="The offset of payload from the header",
                      default=0x40000)
  parser.add_argument("--spi_clock", type=int,
                      help="SPI clock speed. 8, 12, 24, or 48 MHz.",
                      default=24)
  parser.add_argument("--spi_read_cmd", type=int,
                      help="SPI read command. 0x3, 0xB, or 0x3B.",
                      default=0xb)
  parser.add_argument("--image_size", type=int,
                      help="Size of a single image.",
                      default=(188 * 1024))
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
  print("\n")

def print_args(args):
  print("parsed arguments:")
  print(".input  = ", args.input)
  print(".output = ", args.output)
  print(".loader_file = ", args.loader_file)
  print(".spi_size (KB) = ", hex(args.spi_size))
  print(".image_size = ", hex(args.image_size))
  print(".header_loc = ", hex(args.header_loc))
  print(".payload_offset = ", hex(args.payload_offset))
  print(".rwheader_loc = ", hex(args.rwheader_loc))
  print(".spi_clock = ", args.spi_clock)
  print(".spi_read_cmd = ", args.spi_read_cmd)


#
def main():
  print("Begin MEC1701 pack_ec.py script")
  args = parseargs()

  # MEC17xx maximum 192KB each for RO & RW
  # mec1701 chip Makefile sets args.spi_size = 512
  # Tags at offset 0
  #
  print_args(args)

  spi_size = args.spi_size * 1024
  print("SPI Flash image size in bytes =", hex(spi_size))

  # !!! IMPORTANT !!!
  # These values MUST match chip/mec1701/config_flash_layout.h
  # defines.
  #args.header_loc = spi_size - (192 * 1024)
  #args.rwpayload_loc = spi_size - (384 * 1024)
  # loader + EC_RO starts at beginning of second 4KB sector
  # EC_RW starts at offset 0x40000 (256KB)
  # MEC1701 Boot-ROM TAGs are at offset 0 and 4.

  spi_list = []

  print("args.input = ",args.input)
  print("args.loader_file = ",args.loader_file)
  print("args.image_size = ",hex(args.image_size))
  rorofile=PacklfwRoImage(args.input, args.loader_file, args.image_size)

  payload = GetPayload(rorofile)
  payload_len = len(payload)
  # debug
  print("EC_LFW + EC_RO length = ",hex(payload_len))

  # MEC17xx on Chromebooks is not using BootROM ECDSA.
  # We implemented the ECDSA disabled case where
  # the 64-byte signature contains a SHA-256 of the binary + 32 zeros bytes.
  payload_signature = SignByteArray(payload)
  # MEC17xx Header is 0x80 bytes with an 64 byte signature (32 byte SHA256 +
  # 32 zero bytes
  header = BuildHeader(args, payload_len, LOAD_ADDR, rorofile)
  # debug
  printByteArrayAsHex(header, "Header LFW + EC_RO")

  # MEC17xx payload ECDSA not used, 64 byte signature is SHA256 + 32 zero bytes
  header_signature = SignByteArray(header)
  # debug
  printByteArrayAsHex(header_signature, "header_signature")

  tag = BuildTag(args)
  # MEC17xx truncate RW length to 188KB to not overwrite LFW
  # offset may be different due to Header size and other changes
  # MCHP we want to append a SHA-256 to the end of the actual payload
  # to test SPI read routines.
  #  payloadrw = GetPayloadFromOffset(args.input,args.image_size)[:256*1024]
  print("Call to GetPayloadFromOffset")
  print("args.input = ", args.input)
  print("args.image_size = ", hex(args.image_size))
  payload_rw = GetPayloadFromOffset(args.input, args.image_size)
  print("type(payload_rw) is ", type(payload_rw))
  print("len(payload_rw) is ", hex(len(payload_rw)))
  # Now extend size out to 188KB, same as CONFIG_RO_SIZE
  rw_len = (188 * 1024) - 32 # leave space for SHA-256
  payload_rw = payload_rw[:rw_len]
  payload_rw_len = len(payload_rw)
  print("Extended size of EC_RW = ", hex(payload_rw_len))

  payload_entry_tuple = struct.unpack_from('<I', payload_rw, 4)
  print("payload_entry_tuple = ", payload_entry_tuple)
  payload_entry = payload_entry_tuple[0]
  print("payload_entry = ", hex(payload_entry))

  # Hash
  rw_digest = HashByteArray(payload_rw)
  printByteArrayAsHex(rw_digest, "EC_RW SHA-256 digest")
  payload_rw = payload_rw + bytes(rw_digest)
  print("EC_RW size after appending SHA-256", hex(len(payload_rw)))

  payload_rw_sig = SignByteArray(payload_rw)

  header_rw = BuildHeader2(args, payload_rw_len, LOAD_ADDR_RW, payload_entry)

  # debug
  printByteArrayAsHex(header_rw, "Header EC_RW")

  header_rw_sig = SignByteArray(header_rw)

  printByteArrayAsHex(header_rw_sig, "header_rw_sig")


  os.remove(rorofile)           # clean up the temp file

  tag2 = BuildTagFromHdrAddr(args.rwheader_loc)

  # MEC1701H Boot-ROM Tags are located at SPI offset 0
  spi_list.append((0, tag, "tag"))
  spi_list.append((4, tag2, "tag2"))

  spi_list.append((args.header_loc, header, "header(lwf + ro)"))
  spi_list.append((args.header_loc + HEADER_SIZE, header_signature,
    "header(lwf + ro) signature"))
  spi_list.append((args.header_loc + args.payload_offset, payload,
    "payload(lfw + ro)"))

  offset = args.header_loc + args.payload_offset + payload_len

  spi_list.append((offset, payload_signature,
                   "payload(lfw_ro) signature"))

  spi_list.append((args.rwheader_loc, header_rw, "header(rw)"))
  spi_list.append((args.rwheader_loc + HEADER_SIZE, header_rw_sig,
    "header(rw) signature"))
  spi_list.append((args.rwheader_loc + args.payload_offset, payload_rw,
    "payload(rw)"))

  offset = args.rwheader_loc + args.payload_offset + payload_rw_len

  spi_list.append((offset, payload_rw_sig,
                   "payload(rw) signature"))

  spi_list = sorted(spi_list)
  # uncomment to debug
  dumpsects(spi_list)

  #
  # MEC1701H Boot-ROM locates TAG at SPI offset 0 instead of end of SPI.
  #
  with open(args.output, 'wb') as f:
    print("Write spi list to file", args.output)
    addr = 0
    for s in spi_list:
      if addr < s[0]:
        print("Offset ",hex(addr)," Length", hex(s[0]-addr), "fill with 0xff")
        f.write('\xff' * (s[0] - addr))
        addr = s[0]
      print("Offset ",hex(addr), " Length", hex(len(s[1])), "write data")
      f.write(s[1])
      addr += len(s[1])

    if addr < spi_size:
      print("Offset ",hex(addr), " Length", hex(spi_size - addr), "fill with 0xff")
      f.write('\xff' * (spi_size - addr))

    f.flush()

if __name__ == '__main__':
  main()
