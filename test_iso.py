import usb.core
import time
import struct
import Queue
import threading

VID = 0x18d1
PID = 0x5030


que = Queue.Queue()


class Packet(object):
    def __init__(self):
        self.frame = []
        self.last_frame_id = -1

    def Parse(self, buf):
        frame_index = buf[0]
        new_frame = buf[1] & (1 << 0)

        if frame_index != (self.last_frame_id + 1) % 256:
            if not new_frame:
                self.last_frame_id = frame_index
                self.frame = []
                return
        if new_frame:
            self.frame = []
        self.frame += buf[2:]
        self.last_frame_id = frame_index


def Worker():
    packet = Packet()
    while True:
        packet.Parse(que.get())
        if len(packet.frame) == 24 * 14 * 2:
            print("\x1b[H.\x1b[2J")
            for x in xrange(24):
                l = ''
                for y in xrange(14):
                    idx = x * 14 + y
                    v = (packet.frame[2 * idx])
                    v |= (packet.frame[2 * idx + 1]) << 8
                    l += '%02x ' % (v >> 3)
                print(l)


def main():
    device = usb.core.find(idVendor=VID, idProduct=PID)
    configuration = device.get_active_configuration()
    interface = configuration[(4, 1)]
    interface.set_altsetting()
    endpoint = interface[0]

    thread = threading.Thread(target=Worker)
    thread.daemon = True
    thread.start()

    while True:
        xs = list(endpoint.read(128))
        if xs:
            que.put(xs)

    time.sleep(0.001)

main()
