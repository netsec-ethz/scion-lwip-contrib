#include <stdio.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <pthread.h>
#include "lwip/sys.h"
#include "lwip/api.h"
#include "libscion/address.h"
#include "lwip/ip_addr.h"

#define LWIP_SOCK_DIR "/run/shm/lwip/"
#define RPCD_SOCKET "/run/shm/lwip/lwip"
#define PATH_LEN 36  // of "accept" socket

struct conn_args{
    int fd;
    struct netconn *conn;
};

void get_default_addr(){
    // get default ip_addr_t from sciond
}

void *sock_thread(void *data);
void handle_new_sock(int fd){
    char buf[5];
    struct conn_args *args = malloc(sizeof *args);
    pthread_t tid;
    printf("NEWS received\n");
    if (read(fd, buf, sizeof(buf)) != 4){
        perror("new_sock() error on read\n");
        return;
    }
    if (strncmp(buf, "NEWS", 4)){
        perror("new_sock() wrong command\n");
        return;
    }
    args->fd = fd;
    args->conn = netconn_new(NETCONN_TCP);
    write(fd, "NEWSOK", 6);
    pthread_create(&tid, NULL, &sock_thread, args);
}

void handle_bind(int fd, struct netconn *conn, char *buf, int len){
    // check len > 12 < 24 ?
    // encode none address
    ip_addr_t addr;
    int port; 
    char *p = buf;
    p += 4; // skip "BIND"
    port = *((u16_t *)p);
    p += 2; // skip port
    scion_addr_raw(&addr, p[0], p + 1);
    fprintf(stderr, "Bound port %d, and addr:\n", port);
    print_scion_addr(&addr);
    netconn_bind(conn, &addr, port); // test addr = NULL
    write(fd, "BINDOK", 6);
}

void handle_connect(int fd, struct netconn *conn, char *buf, int len){
    ip_addr_t addr;
    int port; 
    char *p = buf;
    p += 4; // skip "BIND"
    port = *((u16_t *)p);
    p += 2; // skip port
    scion_addr_raw(&addr, p[0], p + 1);
    print_scion_addr(&addr);
    if (netconn_connect(conn, &addr, 5000) == ERR_OK)
        write(fd, "CONNOK", 6);
    else
        write(fd, "CONNER", 6);
}

void handle_listen(int fd, struct netconn *conn){
    netconn_listen(conn);
    printf("LIST received, returning OK\n");
    write(fd, "LISTOK", 6);
}

void handle_accept(int fd, struct netconn *conn, char *buf, int len){
    char accept_path[strlen(LWIP_SOCK_DIR) + PATH_LEN];
    struct netconn *newconn;
    struct sockaddr_un addr;
    int new_fd;
    fprintf(stderr, "handle_accept()\n");
    if (len != 4 + PATH_LEN){
        perror("Incorrect ACCE length\n");
        write(fd, "ACCEER", 6);
    }

    fprintf(stderr, "handle_accept(): waiting...\n");
    netconn_accept(conn, &newconn);

    strncpy(accept_path, LWIP_SOCK_DIR, strlen(LWIP_SOCK_DIR));
    strncat(accept_path, buf + 4 , PATH_LEN);
    fprintf(stderr, "Will connect to %s\n", accept_path);
        //remove:
    if ( (new_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
      perror("socket error");
      exit(-1);
    }
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, accept_path, sizeof(addr.sun_path)-1);
    if (connect(new_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("connect error"); 
        exit(-1);
    }

    // start thread with new_fd and newconn
    struct conn_args *args = malloc(sizeof *args);
    pthread_t tid;
    args->fd = new_fd;
    args->conn = newconn;
    pthread_create(&tid, NULL, &sock_thread, args);
    // let know that new thread is ready
    write(new_fd, "ACCEOK", 6); // TODO: return addr here
}

void handle_send(int fd, struct netconn *conn, char *buf, int len){
    //PSz: discuss how to implement it long term, lib probably has to pass len
    netconn_write(conn, buf+4, len-4, NETCONN_COPY); // try with NOCOPY
    write(fd, "SENDOK", 6);
}

void handle_recv(int fd, struct netconn *conn){
    struct netbuf *buf;
    void *data;
    u16_t len;
    if (netconn_recv(conn, &buf) == ERR_OK){
        netbuf_data(buf, &data, &len);
        // put two write()s instead RECVOK should be followed by len
        char msg[len + 6];
        memcpy(msg, "RECVOK", 6);
        memcpy(msg, buf, len);
        write(fd, msg, len + 6);
    }
    else
        write(fd, "RECVER", 6);
}

void handle_close(struct conn_args *args){
    pthread_exit(0);
    // TODO: check code below
    /* write(args->fd, "CLOSOK", 6); */
    /* close(args->fd); */
    netconn_close(args->conn);
    netconn_delete(args->conn);
    free(args);
    //smth missing?
    pthread_exit(0);
}

void *sock_thread(void *data){
    struct conn_args *args = data;
    int rc, fd = args->fd;
    struct netconn *conn = args->conn;
    char buf[1024];
    fprintf(stderr, "started, waiting for requests\n");
    while ((rc=read(fd, buf, sizeof(buf))) > 0) {
        printf("read %u bytes from %d: %.*s\n", rc, fd, rc, buf);
        if (rc < 4){
            perror("command too short\n");
            continue;
        }
        if (!strncmp(buf, "SEND", 4))
            handle_send(fd, conn, buf, rc);
        else if (!strncmp(buf, "RECV", 4))
            handle_recv(fd, conn);
        else if (!strncmp(buf, "BIND", 4))
            handle_bind(fd, conn, buf, rc);
        else if (!strncmp(buf, "CONN", 4))
            handle_connect(fd, conn, buf, rc);
        else if (!strncmp(buf, "LIST", 4))
            handle_listen(fd, conn);
        else if (!strncmp(buf, "ACCE", 4))
            handle_accept(fd, conn, buf, rc);
        else if (!strncmp(buf, "CLOS", 4))
            handle_close(args);
    }
    if (rc == -1) {
        // clean here
        handle_close(args);
        perror("read");
        exit(-1);
    }
    else if (rc == 0) {
        printf("EOF\n");
        // clean here
        handle_close(args);
        free(args);
        close(fd);
    }
    return;
}

int main() {
    struct sockaddr_un addr;
    int fd,cl;
    if ( (fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket error");
        exit(-1);
    }

    mkdir(LWIP_SOCK_DIR, 0755);

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, RPCD_SOCKET, sizeof(addr.sun_path)-1);

    unlink(RPCD_SOCKET);

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind error");
        exit(-1);
    }
    if (listen(fd, 5) == -1) {
        perror("listen error");
        exit(-1);
    }

    while (1) {
        if ( (cl = accept(fd, NULL, NULL)) == -1) {
            perror("accept error");
            continue;
        }
        // socket() called by app. Create a netconn and a coresponding thread.
        handle_new_sock(cl);
    }
    return 0;
}

