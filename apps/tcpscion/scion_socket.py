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
    

    def __init__(self, family, type_, proto=0):
        assert family == AF_SCION
        assert type_ == SOCK_STREAM
        assert proto == 0 
        self.family = family
        self.type_ = type_
        self.proto = proto
        self.lwip_sock = None
        self.lwip_accept = None

    def bind(self, addr_port):
        addr, port = addr_port
        haddr_type = addr.host.TYPE
        req = (b"BIND" + struct.pack("H", port) + 
               struct.pack("B", haddr_type) + addr.pack())
        self._to_lwip(req)
        # "BINDOK", "BINDER"
        rep = self._from_lwip()

    def create_socket(self):
        assert self.lwip_sock is None
        # Create a socket to LWIP
        self.lwip_sock = stdsock.socket(stdsock.AF_UNIX, stdsock.SOCK_STREAM)
        self.lwip_sock.connect(RPCD_SOCKET)
        # Register it 
        req = b"NEWS"
        self._to_lwip(req)
        # send to lw: "NEWS"
        # receive: "NEWSOK" or "NEWSER"
        rep = self._from_lwip()

    def _to_lwip(self, req):
        print("Sending to LWIP:", req)
        self.lwip_sock.send(req)

    def _from_lwip(self):
        rep = self.lwip_sock.recv(self.BUFLEN)  # read in a loop
        print("Reading from LWIP:", rep)

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
        # new_sock, _ = self.lwip_accept.accept()
        rep = self._from_lwip()
# 
#         sock = SCIONSocket(self.family, self.type_, self.proto)
#         addr = sock.new_accept()
        # read from accept socket()

    def _init_accept_sock(self):
        if self.lwip_accept:
            return
        fname = "%s%s" % (LWIP_SOCK_DIR, uuid.uuid4())
        while os.path.exists(fname):
            fname = "%s%s" % (LWIP_SOCK_DIR, uuid.uuid4())
        print("_init_accept:", fname)
        self.lwip_accept = stdsock.socket(stdsock.AF_UNIX, stdsock.SOCK_STREAM)
        self.lwip_accept.bind(fname)

    def new_accept(self): #it creates new 
        assert self.lwip_sock is None
        return None # should be SCIONAddr here


def socket(family, type_, proto=0):
    sock = SCIONSocket(family, type_, proto)
    sock.create_socket()
    return sock

s = socket(AF_SCION, SOCK_STREAM)
addr = SCIONAddr.from_values(ISD_AS("1-2"), haddr_parse(1, "127.0.0.1"))
s.bind((addr, 5000))
s.listen()
s.accept()
