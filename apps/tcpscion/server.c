#include <stdint.h>
#include "lwip/sys.h"
#include "lwip/api.h"
#include "lwip/ip_addr.h"
#include "libscion/address.h"

int server()
{
    struct netconn *conn, *newconn;
    ip_addr_t addr;
    u8_t def_addr[] = {127, 0, 0, 1}; 
    scion_addr(&addr, 1, 2, ADDR_IPV4_TYPE, def_addr);
    conn = netconn_new(NETCONN_TCP);
    netconn_bind(conn, &addr, 5000); // test addr = NULL
    netconn_listen(conn);
    while (1) {
        if (netconn_accept(conn, &newconn) == ERR_OK) {
            netconn_write(newconn, "123", 3, NETCONN_COPY); // handle err
            netconn_close(newconn);
            netconn_delete(newconn);
        }
    }
}
