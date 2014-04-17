#!/usr/bin/python

# dut-control --port=9990 ec_uart_baudrate:3000000
# stty -F /dev/pts/1 raw
#  cat /dev/pts/1 | ./util/pd_dump.py

import array
import struct
import sys
from zlib import crc32
#import servo.dut_control as dutc

MAX_INTERVAL = 10
PERIOD  = 4
PERIOD_THRESHOLD = ((PERIOD + 2*PERIOD) / 2)

NO_EOP = -1
HARD_RESET = -2

# Control Message type
CTRL_TYPES = {
	0 : "reserved",
	1 : "GOOD_CRC",
	2 : "GOTO_MIN",
	3 : "ACCEPT",
	4 : "REJECT",
	5 : "PING",
	6 : "PS_RDY",
	7 : "GET_SOURCE_CAP",
	8 : "GET_SINK_CAP",
	9 : "PROTOCOL_ERR",
	10 : "SWAP",
	11 : "reserved",
	12 : "WAIT",
	13 : "SOFT_RESET",
	14 : "reserved",
	15 : "reserved"
}

# Data message type 
DATA_TYPES = {
	1 : "SOURCE_CAP",
	2 : "REQUEST",
	3 : "BIST",
	4 : "SINK_CAP",
	15 : "VDM"
}

DEC4B5B = [
0x10, # Error     /* 00000 */,
0x10, # Error     /* 00001 */,
0x10, # Error     /* 00010 */,
0x10, # Error     /* 00011 */,
0x10, # Error     /* 00100 */,
0x10, # Error     /* 00101 */,
0x10, # Error     /* 00110 */,
0x13, # RST-1     /* 00111 K-code: Hard Reset #1 */,
0x10, # Error     /* 01000 */,
0x01, # 1 = 0001  /* 01001 */,
0x04, # 4 = 0100  /* 01010 */,
0x05, # 5 = 0101  /* 01011 */,
0x10, # Error     /* 01100 */,
0x15, # EOP       /* 01101 K-code: EOP End Of Packet */,
0x06, # 6 = 0110  /* 01110 */,
0x07, # 7 = 0111  /* 01111 */,
0x10, # Error     /* 10000 */,
0x12, # Sync-2    /* 10001 K-code: Startsynch #2 */,
0x08, # 8 = 1000  /* 10010 */,
0x09, # 9 = 1001  /* 10011 */,
0x02, # 2 = 0010  /* 10100 */,
0x03, # 3 = 0011  /* 10101 */,
0x0A, # A = 1010  /* 10110 */,
0x0B, # B = 1011  /* 10111 */,
0x11, # Sync-1    /* 11000 K-code: Startsynch #1 */,
0x14, # RST-2     /* 11001 K-code: Hard Reset #2 */,
0x0C, # C = 1100  /* 11010 */,
0x0D, # D = 1101  /* 11011 */,
0x0E, # E = 1110  /* 11100 */,
0x0F, # F = 1111  /* 11101 */,
0x00, # 0 = 0000  /* 11110 */,
0x10, # Error     /* 11111 */,
]
# HEAD 4101 [08019032,0001912c,0003c12c,000640c8]
class PDPacket:


	RX_FREQ = 2400000
	def usec(self,tstamp):
		return tstamp / (self.RX_FREQ / 1000000.0)

	def usb_crc32(self, s):
		return (crc32(s, 0) & 0xFFFFFFFF) ^ 0xFFFFFFFF

	RDO_FLAGS = {
		(1 << 24) : "no_suspend",
		(1 << 25) : "comm_cap",
		(1 << 26) : "cap_mismatch",
		(1 << 27) : "give_back"
	}
	def dump_request(self):
		if len(self._payload) < 1:
			return "<BAD>"
		rdo = self._payload[0]
		pos = (rdo >> 28) & 7
		op_ma = ((rdo >> 10) & 0x3ff) * 10
		max_ma = (rdo & 0x3ff) * 10
		flags = ""
		for f in self.RDO_FLAGS.iterkeys():
			if rdo & f:
				flags+=" "+self.RDO_FLAGS[f]
		return "[%d]%d/%d mA%s" % (pos,op_ma,max_ma,flags)

	PDO_TYPE=["", "BATT:", "VAR:", "<bad>"]
	PDO_FLAGS = {
		(1 << 29) : "dual_role",
		(1 << 28) : "suspend",
		(1 << 27) : "ext",
		(1 << 26) : "comm_cap"
	}
	def dump_source_cap(self):
		if len(self._payload) < 1:
			return "<BAD>"
		str = ""
		for pdo in self._payload:
			t = (pdo >> 30) & 3
			if t == 0:
				mv = ((pdo >> 10) & 0x3ff) * 50
				ma = ((pdo >> 0) & 0x3ff) * 10
				p = "%.1fV %.1fA" % (mv/1000.0,ma/1000.0)
			elif t == 1:
				minv = ((pdo >> 10) & 0x3ff) * 50
				maxv = ((pdo >> 20) & 0x3ff) * 50
				mw = ((pdo >> 0) & 0x3ff) * 250
				p = "%.1f/%.1fV %.1fW" % (minv/1000.0,maxv/1000.0,mw/1000.0)
			elif t == 2:
				minv = ((pdo >> 10) & 0x3ff) * 50
				maxv = ((pdo >> 20) & 0x3ff) * 50
				ma = ((pdo >> 0) & 0x3ff) * 10
				p = "%.1f/%.1fV %.1fA" % (minv/1000.0,maxv/1000.0,ma/1000.0)
			else:
				p = ""
			flags = ""
			for f in self.PDO_FLAGS.iterkeys():
				if pdo & f:
					flags+=" "+self.PDO_FLAGS[f]
			str += "{%s%s%s}" % (self.PDO_TYPE[t],p,flags)
		return str

	def dump(self):
		p = ",".join(["%08x" % (x) for x in self._payload])
		role = "SRC" if self.head_role() else "SNK"
		t = self.head_type()
		if self.head_count() == 0:
			summary=CTRL_TYPES[t]
		else:
			summary = DATA_TYPES[t] if DATA_TYPES.has_key(t) else "DAT???"
			if t == 2:
				summary +=self.dump_request()
			elif t == 1:
				summary +=self.dump_source_cap()
		if self._err:
			summary+="!!!%s!!!" % (self._err)
		summary+="^%d/%s^" % (len(self._edges),",".join("%d" % i for i in self._edges))
		print "%10.1f=%6.1f %s|%d :%s: (EOP@%d HEAD %04x [%s])" % (self.usec(self._t0),self.usec(self._length),role,self.head_id(),summary,self._sop,self._head,p)

	def scan_preamble(self):
		last = 0
		for idx in xrange(len(self._edges)):
			dt = self._edges[idx]
			incr = 1 << 31 if dt <= PERIOD_THRESHOLD else 0
			last = (last >> 1) | incr
			if last == 0x36db6db6:
				return idx - 1 #SYNC-1
			if last == 0xF33F3F3F:
				return HARD_RESET
		return NO_EOP

	def dequeue_bits(self, idx, length):
		while idx < len(self._edges) and self._lastlen < length:
			dt = self._edges[idx]
			idx+=1
			incr = 0
			if dt <= PERIOD_THRESHOLD:
				if idx >= len(self._edges):
					self._err += "BIT(%d) " % idx
					return idx, -1
				dt = self._edges[idx]
				if dt > PERIOD_THRESHOLD:
					self._err += "BIT(%d) " % idx
					return idx, -1
				idx+=1
				incr = 0x80000000
			self._last = (self._last >> 1) | incr
			self._lastlen += 1
		if idx < len(self._edges):
			val = (self._last << (self._lastlen - length)) >> (32 - length)
			self._lastlen -= length
			return idx, val
		else:
			return -1, -1

	def decode_short(self, idx):
		idx,w = self.dequeue_bits(idx, 20)
		val_dec = DEC4B5B[w & 0x1f] | (DEC4B5B[(w >> 5) & 0x1f] << 4) | (DEC4B5B[(w >> 10) & 0x1f] << 8) | (DEC4B5B[(w >> 15) & 0x1f] << 12)
		return idx, val_dec

	def decode_word(self, idx):
		idx, lo = self.decode_short(idx)	
		idx, hi = self.decode_short(idx)
		return idx, lo | (hi << 16)	

	def head_id(self):
		return (self._head >> 9) & 7

	def head_role(self):
		return (self._head >> 8) & 1

	def head_type(self):
		return self._head & 0xF

	def head_count(self):
		return (self._head >> 12) & 7

	def __init__(self, edges, t0, t1):
		self._t0 = t0
		self._length = t1 - t0 + 1
		self._edges = edges
		self._toggle = 0; # preamble ends with 1 
		self._last = 0;
		self._lastlen = 0;
		self._head = 0
		self._payload = []
		self._err = ""
		self._sop = self.scan_preamble()
		if self._sop < 1:
			self._err +="BAD PREAMBLE "
			return
		(idx,sop_val) = self.dequeue_bits(self._sop, 20)
		if sop_val != 0x8e318: #PD_EOP:
			self._err +="BAD EOP "
			return
		idx, self._head = self.decode_short(idx)
		if self.head_count() > 7:
			self._err +="INVAL COUNT "
		for i in xrange(self.head_count()):
			idx, p = self.decode_word(idx)
			if idx < 0:
				self._err +="BAD PAYLOAD "
				return
			self._payload.append(p)
		idx, self._crc = self.decode_word(idx)
		#s = struct.pack("H" + "I"*len(self._payload),self._head&0xFFFF,*self._payload)
		#print "CRC %08x <> %08x" % (self._crc, self.usb_crc32(s))
		(idx,eop_val) = self.dequeue_bits(idx, 20)

def next_intervals(idx, b):
	v = b[idx]
	if v == 0xff:
		if idx + 4 >= len(b) - 3:
			print "Corrupted"
			return (len(b) - 3, 0, 0)
		else:
			d0 = b[1] | (b[2] << 8)
			d1 = b[3] | (b[4] << 8)
			idx += 4
	else:
		d1 = v >> 4
		d0 = v & 0xF
	if d0 == 0: d0 = 0x10000
	if d1 == 0: d1 = 0x10000
	return (idx+1, d0, d1)

def parse_stream(fs):
	packets = []
	tstamp = 0
	t0 = 0
	edges = []
	while True:
		b = array.array("B",fs.readline())
		if len(b) < 5:
			continue
		seq = b[0]
		space_mode = 0
		idx = 1
		while idx < len(b) - 3:
			(idx, dt0, dt1) = next_intervals(idx, b)
			for dt in dt0,dt1:
				if dt > MAX_INTERVAL:
					if len(edges) > 0:
						packets.append(PDPacket(edges, t0, tstamp))
						packets[-1].dump()
						edges = []
					tstamp += dt
					t0 = tstamp
				else:
					edges.append(dt)
					tstamp += dt
		if (b[idx] != seq) or (b[idx+1] != 0xd) or (b[idx+2] != 0xa):
			print "COMM ERR [%d/%d]" % (seq,b[idx])
				

if __name__=="__main__":
	parse_stream(sys.stdin)
