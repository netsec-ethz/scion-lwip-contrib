import socket as stdsock # To avoid name collision 
import struct
import time
from lib.packet.host_addr import haddr_parse
from lib.packet.scion_addr import ISD_AS, SCIONAddr
import uuid
import os

# unix socket is created when tcp socket is created, i.e., socket() or accept()

LWIP_SOCK_DIR = "/run/shm/lwip/"
RPCD_SOCKET = "/run/shm/lwip/lwip"
AF_SCION = 3 # TODO(PSz): double check
SOCK_STREAM = 1 # ditto 

class SCIONSocket(object):
    # MAX_TRY = 3 # max retries for init and create socket
    BUFLEN = 1024
    

    def __init__(self, family, type_, proto=0, name=''):
        assert family == AF_SCION
        assert type_ == SOCK_STREAM
        assert proto == 0 
        self.family = family
        self.type_ = type_
        self.proto = proto
        self.lwip_sock = None
        self.lwip_accept = None
        self.name = name # debug only

    def bind(self, addr_port):
        addr, port = addr_port
        haddr_type = addr.host.TYPE
        req = (b"BIND" + struct.pack("H", port) + 
               struct.pack("B", haddr_type) + addr.pack())
        self._to_lwip(req)
        rep = self._from_lwip()

    def connect(self, addr_port):
        addr, port = addr_port
        haddr_type = addr.host.TYPE
        req = (b"CONN" + struct.pack("H", port) + 
               struct.pack("B", haddr_type) + addr.pack())
        self._to_lwip(req)
        rep = self._from_lwip()

    def create_socket(self):
        assert self.lwip_sock is None
        # Create a socket to LWIP
        self.lwip_sock = stdsock.socket(stdsock.AF_UNIX, stdsock.SOCK_STREAM)
        self.lwip_sock.connect(RPCD_SOCKET)
        # Register it 
        req = b"NEWS"
        self._to_lwip(req)
        rep = self._from_lwip()

    def _to_lwip(self, req):
        print(self.name, "Sending to LWIP:", req)
        self.lwip_sock.send(req)

    def _from_lwip(self, buflen=None):
        if buflen is None:
            buflen=self.BUFLEN
        rep = self.lwip_sock.recv(buflen)  # read in a loop
        print(self.name, "Reading from LWIP:", rep)

    def listen(self): # w/o backlog for now, let's use LWIP's default
        req = b"LIST"
        self._to_lwip(req)
        rep = self._from_lwip()

    def accept(self):
        self._init_accept_sock()
        self.lwip_accept.listen(5)  # should be consistent with LWIP's backlog
        # pass it in new?
        req = b"ACCE" + self.lwip_accept.getsockname()[-36:].encode('ascii')
        self._to_lwip(req)
        new_sock, _ = self.lwip_accept.accept()
        print(self.name, "From accept socket: ", new_sock.recv(self.BUFLEN))
        sock = SCIONSocket(self.family, self.type_, self.proto, name="NEW_ACC")
        sock.lwip_sock = new_sock 
        return sock, None # addr 

    def _init_accept_sock(self):
        if self.lwip_accept:
            return
        fname = "%s%s" % (LWIP_SOCK_DIR, uuid.uuid4())
        while os.path.exists(fname):
            fname = "%s%s" % (LWIP_SOCK_DIR, uuid.uuid4())
        print(self.name, "_init_accept:", fname)
        self.lwip_accept = stdsock.socket(stdsock.AF_UNIX, stdsock.SOCK_STREAM)
        self.lwip_accept.bind(fname)

    def send(self, msg):
        req = b"SEND" + msg
        self._to_lwip(req)
        rep = self._from_lwip()

    def recv(self, bufsize):
        req = b"RECV"  # + bufsize, ignored for now
        self._to_lwip(req)
        rep = self._from_lwip()
        if rep is None or len(rep) < 6:
            return b''
        return rep[6:]

    def close(self):
        req = b"CLOS"
        self._to_lwip(req)
        self.lwip_sock.close()
        if self.lwip_accept: 
            self.lwip_accept.close()


def socket(family, type_, proto=0, name=''):
    sock = SCIONSocket(family, type_, proto, name)
    sock.create_socket()
    return sock


# Test
import threading
import time
def server():
    print("server running")
    s = socket(AF_SCION, SOCK_STREAM, name='SERVER')
    addr = SCIONAddr.from_values(ISD_AS("1-2"), haddr_parse(1, "127.0.0.1"))
    s.bind((addr, 5000))
    s.listen()
    while True:
        new_sock, addr = s.accept()
        new_sock.send(b" TEST_FROM_APP")
        new_sock.close()

def client():
    print("client running")
    s = socket(AF_SCION, SOCK_STREAM, name='CLIENT%d' % int(time.time()) )
    addr = SCIONAddr.from_values(ISD_AS("1-2"), haddr_parse(1, "127.0.0.1"))
    s.connect((addr, 5000))
    s.recv(1024)
    s.close()

threading.Thread(target=server).start()
while True:
    input()
    print("\n\n")
    threading.Thread(target=client).start()

