#include "lwip/sys.h"
#include "lwip/api.h"

int server()
{
    struct netconn *conn, *newconn;
    ip_addr_t addr;
    addr.addr = 16777343; // 127.0.0.1
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
