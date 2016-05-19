#include <stdint.h>
#include "lwip/sys.h"
#include "lwip/api.h"
#include "lwip/ip_addr.h"
#include "libscion/address.h"

int client()
{
    struct netconn *conn;
    ip_addr_t addr;
    u8_t def_addr[] = {127, 0, 0, 1}; 
    scion_addr_val(&addr, 1, 2, ADDR_IPV4_TYPE, def_addr);
    conn = netconn_new(NETCONN_TCP);
    if (netconn_connect(conn, &addr, 5000) == ERR_OK)
    {
          struct netbuf *buf;
          void *data;
          u16_t len;
          if (netconn_recv(conn, &buf) == ERR_OK)
          {
               netbuf_data(buf, &data, &len);
               printf("len:%d:%s\n", len, (char *)data);
          }
    }
    netconn_close(conn);
    netconn_delete(conn);
}
