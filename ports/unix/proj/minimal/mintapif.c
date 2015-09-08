/*
 * Copyright (c) 2001-2003 Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Author: Adam Dunkels <adam@sics.se>
 *
 */

#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/socket.h>

#include "lwip/opt.h"

#include "lwip/debug.h"
#include "lwip/def.h"
#include "lwip/ip.h"
#include "lwip/mem.h"
#include "lwip/stats.h"
#include "lwip/snmp.h"
#include "lwip/pbuf.h"
#include "lwip/sys.h"
#include "lwip/timers.h"
#include "netif/etharp.h"
#include "lwip/ethip6.h"

#if defined(LWIP_DEBUG) && defined(LWIP_TCPDUMP)
#include "netif/tcpdump.h"
#endif /* LWIP_DEBUG && LWIP_TCPDUMP */

#include "mintapif.h"

#define IFCONFIG_BIN "/sbin/ifconfig "

#if defined(LWIP_UNIX_LINUX)
#include <sys/ioctl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
/*
 * Creating a tap interface requires special privileges. If the interfaces
 * is created in advance with `tunctl -u <user>` it can be opened as a regular
 * user. The network must already be configured. If DEVTAP_IF is defined it
 * will be opened instead of creating a new tap device.
 *
 * You can also use PRECONFIGURED_TAPIF environment variable to do so.
 */
/* #define DEVTAP_IF "tap0" */
#define DEVTAP "/dev/net/tun"
#define NETMASK_ARGS "netmask %d.%d.%d.%d"
#define IFCONFIG_ARGS "tap0 inet %d.%d.%d.%d " NETMASK_ARGS
#elif defined(LWIP_UNIX_OPENBSD)
#define DEVTAP "/dev/tun0"
#define NETMASK_ARGS "netmask %d.%d.%d.%d"
#define IFCONFIG_ARGS "tun0 inet %d.%d.%d.%d " NETMASK_ARGS " link0"
#else /* others */
#define DEVTAP "/dev/tap0"
#define NETMASK_ARGS "netmask %d.%d.%d.%d"
#define IFCONFIG_ARGS "tap0 inet %d.%d.%d.%d " NETMASK_ARGS
#endif

/* Define those to better describe your network interface. */
#define IFNAME0 't'
#define IFNAME1 'p'

#ifndef TAPIF_DEBUG
#define TAPIF_DEBUG LWIP_DBG_OFF
#endif

struct mintapif {
  struct eth_addr *ethaddr;
  /* Add whatever per-interface state that is needed here. */
  u32_t lasttime;
  int fd;
};

/* Forward declarations. */
static void mintapif_input(struct netif *netif);

/*-----------------------------------------------------------------------------------*/
static void
low_level_init(struct netif *netif)
{
  struct mintapif *mintapif;
  int ret;
#ifndef DEVTAP_IF
  char buf[1024];
  char *preconfigured_tapif = getenv("PRECONFIGURED_TAPIF");
#endif /* DEVTAP_IF */

  mintapif = (struct mintapif *)netif->state;

  /* Obtain MAC address from network interface. */

  /* (We just fake an address...) */
  mintapif->ethaddr->addr[0] = 0x02;
  mintapif->ethaddr->addr[1] = 0x12;
  mintapif->ethaddr->addr[2] = 0x34;
  mintapif->ethaddr->addr[3] = 0x56;
  mintapif->ethaddr->addr[4] = 0x78;
  mintapif->ethaddr->addr[5] = 0xab;

  /* device capabilities */
  netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP;

  mintapif->fd = open(DEVTAP, O_RDWR);
  LWIP_DEBUGF(TAPIF_DEBUG, ("mintapif_init: fd %d\n", mintapif->fd));
  if (mintapif->fd == -1) {
#ifdef LWIP_UNIX_LINUX
    perror("mintapif_init: try running \"modprobe tun\" or rebuilding your kernel with CONFIG_TUN; cannot open "DEVTAP);
#else /* LWIP_UNIX_LINUX */
    perror("mintapif_init: cannot open "DEVTAP);
#endif /* LWIP_UNIX_LINUX */
    exit(1);
  }

#ifdef LWIP_UNIX_LINUX
  {
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
#ifdef DEVTAP_IF
    strncpy(ifr.ifr_name, DEVTAP_IF, IFNAMSIZ);
#else /* DEVTAP_IF */
    if (preconfigured_tapif) {
      strncpy(ifr.ifr_name, preconfigured_tapif, IFNAMSIZ);
    }
#endif /* DEVTAP_IF */
    ifr.ifr_flags = IFF_TAP|IFF_NO_PI;
    if (ioctl(mintapif->fd, TUNSETIFF, (void *) &ifr) < 0) {
      perror("mintapif_init: "DEVTAP" ioctl TUNSETIFF");
      exit(1);
    }
  }
#endif /* Linux */
  netif_set_link_up(netif);

#ifndef DEVTAP_IF
  if (preconfigured_tapif == NULL) {
    snprintf(buf, 1024, IFCONFIG_BIN IFCONFIG_ARGS,
             ip4_addr1(&(netif->gw)),
             ip4_addr2(&(netif->gw)),
             ip4_addr3(&(netif->gw)),
             ip4_addr4(&(netif->gw))
#ifdef NETMASK_ARGS
             ,
             ip4_addr1(&(netif->netmask)),
             ip4_addr2(&(netif->netmask)),
             ip4_addr3(&(netif->netmask)),
             ip4_addr4(&(netif->netmask))
#endif /* NETMASK_ARGS */
             );

    LWIP_DEBUGF(TAPIF_DEBUG, ("mintapif_init: system(\"%s\");\n", buf));
    ret = system(buf);
    if (ret < 0) {
      perror("ifconfig failed");
      exit(1);
    }
    if (ret != 0) {
      printf("ifconfig returned %d\n", ret);
    }
  }
#endif /* DEVTAP_IF */

  mintapif->lasttime = 0;
}
/*-----------------------------------------------------------------------------------*/
/*
 * low_level_output():
 *
 * Should do the actual transmission of the packet. The packet is
 * contained in the pbuf that is passed to the function. This pbuf
 * might be chained.
 *
 */
/*-----------------------------------------------------------------------------------*/

static err_t
low_level_output(struct netif *netif, struct pbuf *p)
{
  struct mintapif *mintapif;
  struct pbuf *q;
  char buf[1514];
  char *bufptr;
  ssize_t written;

  mintapif = (struct mintapif *)netif->state;
#if 0
  if (((double)rand()/(double)RAND_MAX) < 0.2) {
    printf("drop output\n");
    return ERR_OK;
  }
#endif

  /* initiate transfer(); */
  bufptr = &buf[0];

  for(q = p; q != NULL; q = q->next) {
    /* Send the data from the pbuf to the interface, one pbuf at a
       time. The size of the data in each pbuf is kept in the ->len
       variable. */
    /* send data from(q->payload, q->len); */
    memcpy(bufptr, q->payload, q->len);
    bufptr += q->len;
  }

  /* signal that packet should be sent(); */
  written = write(mintapif->fd, buf, p->tot_len);
  if (written == -1) {
    snmp_inc_ifoutdiscards(netif);
    perror("mintapif: write");
  }
  else {
    snmp_add_ifoutoctets(netif, written);
  }
  return ERR_OK;
}
/*-----------------------------------------------------------------------------------*/
/*
 * low_level_input():
 *
 * Should allocate a pbuf and transfer the bytes of the incoming
 * packet from the interface into the pbuf.
 *
 */
/*-----------------------------------------------------------------------------------*/
static struct pbuf *
low_level_input(struct netif *netif)
{
  struct pbuf *p, *q;
  u16_t len;
  char buf[1514];
  char *bufptr;
  struct mintapif *mintapif;

  mintapif = (struct mintapif *)netif->state;

  /* Obtain the size of the packet and put it into the "len"
     variable. */
  len = read(mintapif->fd, buf, sizeof(buf));
  if (len == (u16_t)-1) {
    perror("read returned -1");
    exit(1);
  }

  snmp_add_ifinoctets(netif,len);

#if 0
  if (((double)rand()/(double)RAND_MAX) < 0.2) {
    printf("drop\n");
    return NULL;
  }
#endif

  /* We allocate a pbuf chain of pbufs from the pool. */
  p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);

  if (p != NULL) {
    /* We iterate over the pbuf chain until we have read the entire
       packet into the pbuf. */
    bufptr = &buf[0];
    for(q = p; q != NULL; q = q->next) {
      /* Read enough bytes to fill this pbuf in the chain. The
         available data in the pbuf is given by the q->len
         variable. */
      /* read data into(q->payload, q->len); */
      memcpy(q->payload, bufptr, q->len);
      bufptr += q->len;
    }
    /* acknowledge that packet has been read(); */
  } else {
    /* drop packet(); */
    snmp_inc_ifindiscards(netif);
    printf("Could not allocate pbufs\n");
  }

  return p;
}
/*-----------------------------------------------------------------------------------*/
/*
 * mintapif_input():
 *
 * This function should be called when a packet is ready to be read
 * from the interface. It uses the function low_level_input() that
 * should handle the actual reception of bytes from the network
 * interface.
 *
 */
/*-----------------------------------------------------------------------------------*/
static void
mintapif_input(struct netif *netif)
{
  struct pbuf *p;

  p = low_level_input(netif);

  if (p == NULL) {
#if LINK_STATS
    LINK_STATS_INC(link.recv);
#endif /* LINK_STATS */
    LWIP_DEBUGF(TAPIF_DEBUG, ("mintapif_input: low_level_input returned NULL\n"));
    return;
  }

  if (netif->input(p, netif) != ERR_OK) {
    LWIP_DEBUGF(NETIF_DEBUG, ("mintapif_input: netif input error\n"));
    pbuf_free(p);
  }
}
/*-----------------------------------------------------------------------------------*/
/*
 * mintapif_init():
 *
 * Should be called at the beginning of the program to set up the
 * network interface. It calls the function low_level_init() to do the
 * actual setup of the hardware.
 *
 */
/*-----------------------------------------------------------------------------------*/
err_t
mintapif_init(struct netif *netif)
{
  struct mintapif *mintapif;

  mintapif = (struct mintapif *)mem_malloc(sizeof(struct mintapif));
  if (mintapif == NULL) {
    LWIP_DEBUGF(NETIF_DEBUG, ("mintapif_init: out of memory for mintapif\n"));
    return ERR_MEM;
  }
  netif->state = mintapif;
#if LWIP_SNMP
  /* ifType is other(1), there doesn't seem
     to be a proper type for the tunnel if */
  netif->link_type = 1;
  /* @todo get this from struct tunif? */
  netif->link_speed = 0;
  netif->ts = 0;
  netif->ifinoctets = 0;
  netif->ifinucastpkts = 0;
  netif->ifinnucastpkts = 0;
  netif->ifindiscards = 0;
  netif->ifoutoctets = 0;
  netif->ifoutucastpkts = 0;
  netif->ifoutnucastpkts = 0;
  netif->ifoutdiscards = 0;
#endif

  netif->name[0] = IFNAME0;
  netif->name[1] = IFNAME1;
  netif->output = etharp_output;
#if LWIP_IPV6
  netif->output_ip6 = ethip6_output;
#endif /* LWIP_IPV6 */
  netif->linkoutput = low_level_output;
  netif->mtu = 1500;
  /* hardware address length */
  netif->hwaddr_len = 6;

  mintapif->ethaddr = (struct eth_addr *)&(netif->hwaddr[0]);

  low_level_init(netif);

  return ERR_OK;
}
/*-----------------------------------------------------------------------------------*/

int
mintapif_select(struct netif *netif)
{
  fd_set fdset;
  int ret;
  struct timeval tv;
  struct mintapif *mintapif;
  u32_t msecs = sys_timeouts_sleeptime();

  mintapif = (struct mintapif *)netif->state;

  tv.tv_sec = msecs / 1000;
  tv.tv_usec = (msecs % 1000) * 1000;

  FD_ZERO(&fdset);
  FD_SET(mintapif->fd, &fdset);

  ret = select(mintapif->fd + 1, &fdset, NULL, NULL, &tv);
  if (ret > 0) {
    mintapif_input(netif);
  }
  return ret;
}
