#
# Copyright (c) 2001, 2002 Swedish Institute of Computer Science.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without modification,
# are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
# 3. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
# WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
# SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
# OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
# IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
# OF SUCH DAMAGE.
#
# This file is part of the lwIP TCP/IP stack.
#
# Author: Adam Dunkels <adam@sics.se>
#

# Copyright 2016 ETH Zurich
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# This file is based on ports/unix/proj/lib/Makefile


CONTRIBDIR=../../../..
LWIPARCH=$(CONTRIBDIR)/ports/unix

#Set this to where you have the lwip core module checked out from CVS
#default assumes it's a dir named lwip at the same level as the contrib module
LWIPDIR=$(CONTRIBDIR)/../lwip/src

SCIONLIB=$(CONTRIBDIR)/../../c/lib

CCDEP=gcc
CC=gcc
CFLAGS=-g -Wall -fPIC

CFLAGS:=$(CFLAGS) \
	-I$(LWIPDIR)/include -I$(LWIPARCH)/include -I$(LWIPDIR)/include/scion -I$(LWIPDIR)/include/ipv4 -I$(LWIPDIR) -I$(SCIONLIB) -I.

COREFILES=$(LWIPDIR)/core/memp.c $(LWIPDIR)/core/pbuf.c $(LWIPDIR)/core/sys.c \
		$(LWIPDIR)/core/tcp.c $(LWIPDIR)/core/tcp_in.c $(LWIPDIR)/core/tcp_out.c $(LWIPDIR)/core/init.c \
        $(LWIPDIR)/core/def.c $(LWIPDIR)/core/timers.c

CORESCIONFILES=$(LWIPDIR)/core/scion/scion.c $(LWIPDIR)/core/scion/scion_addr.c

# APIFILES: The files which implement the sequential API.
APIFILES=$(LWIPDIR)/api/api_lib.c $(LWIPDIR)/api/api_msg.c $(LWIPDIR)/api/tcpip.c $(LWIPDIR)/api/netbuf.c $(LWIPDIR)/api/err.c

# ARCHFILES: Architecture specific files.
ARCHFILES=$(LWIPARCH)/sys_arch.c

# LWIPFILES: All the above.
LWIPFILES=$(COREFILES) $(CORESCIONFILES) $(APIFILES) $(ARCHFILES)
LWIPFILESW=$(wildcard $(LWIPFILES))
LWIPOBJS=$(notdir $(LWIPFILESW:.c=.o))

DYNAMIC=liblwip.so
STATIC=liblwip.a
TARGETS=$(DYNAMIC) $(STATIC)
PREFIX?=/usr/local

%.o:
	$(CC) $(CFLAGS) -c $(<:.o=.c)

all: $(TARGETS)
.PHONY: all clean install uninstall

clean:
	rm -f *.o *.so *.a *.s .depend* *.core core

depend dep: .depend

include .depend

$(DYNAMIC): $(LWIPOBJS) unixlib.o
	$(CC) -g -nostartfiles -shared -o $@ $^

$(STATIC): $(LWIPOBJS) unixlib.o
	 ar rcs $@ $^

.depend: unixlib.c $(LWIPFILES)
	$(CCDEP) $(CFLAGS) -MM $^ > .depend || rm -f .depend

install: .installstamp

.installstamp: $(TARGETS)
	sudo cp $(TARGETS) $(PREFIX)/lib/
	sudo ldconfig
	touch .installstamp

uninstall:
	$(foreach var,$(TARGETS),sudo rm -f $(PREFIX)/lib/$(var);)
	sudo ldconfig
	rm -f .installstamp
