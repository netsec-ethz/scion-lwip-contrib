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
#define SOCK_PATH_LEN 36  // of "accept" socket
#define CMD_SIZE 4
#define BUFLEN 1024

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
    if (read(fd, buf, sizeof(buf)) != CMD_SIZE){
        perror("new_sock() error on read\n");
        return;
    }
    if (strncmp(buf, "NEWS", CMD_SIZE)){
        perror("new_sock() wrong command\n");
        return;
    }
    args->fd = fd;
    args->conn = netconn_new(NETCONN_TCP);
    write(fd, "NEWSOK", 6);
    pthread_create(&tid, NULL, &sock_thread, args);
}

void handle_bind(struct conn_args *args, char *buf, int len){
    // check len > 12 < 24 ?
    // encode none address
    ip_addr_t addr;
    int port; 
    char *p = buf;
    p += CMD_SIZE; // skip "BIND"
    port = *((u16_t *)p);
    p += 2; // skip port
    scion_addr_raw(&addr, p[0], p + 1);
    fprintf(stderr, "Bound port %d, and addr:\n", port);
    print_scion_addr(&addr);
    netconn_bind(args->conn, &addr, port); // test addr = NULL
    write(args->fd, "BINDOK", 6);
}

void handle_connect(struct conn_args *args, char *buf, int len){
    ip_addr_t addr;
    int port; 
    char *p = buf;
    p += CMD_SIZE; // skip "BIND"
    port = *((u16_t *)p);
    p += 2; // skip port
    scion_addr_raw(&addr, p[0], p + 1);
    print_scion_addr(&addr);
    if (netconn_connect(args->conn, &addr, port) == ERR_OK)
        write(args->fd, "CONNOK", 6);
    else
        write(args->fd, "CONNER", 6);
}

void handle_listen(struct conn_args *args){
    netconn_listen(args->conn);
    printf("LIST received, returning OK\n");
    write(args->fd, "LISTOK", 6);
}

void handle_accept(struct conn_args *args, char *buf, int len){
    char accept_path[strlen(LWIP_SOCK_DIR) + SOCK_PATH_LEN];
    struct netconn *newconn;
    struct sockaddr_un addr;
    int new_fd;
    fprintf(stderr, "handle_accept()\n");
    if (len != CMD_SIZE + SOCK_PATH_LEN){
        perror("Incorrect ACCE length\n");
        write(args->fd, "ACCEER", 6);
    }

    fprintf(stderr, "handle_accept(): waiting...\n");
    netconn_accept(args->conn, &newconn);

    /* strncpy(accept_path, LWIP_SOCK_DIR, strlen(LWIP_SOCK_DIR)); */
    /* strncat(accept_path, buf + CMD_SIZE , SOCK_PATH_LEN); */
    sprintf(accept_path, "%s%.*s", LWIP_SOCK_DIR, SOCK_PATH_LEN, buf + CMD_SIZE);
    /* accept_path[strlen(LWIP_SOCK_DIR) + SOCK_PATH_LEN] = '\x00'; */
    fprintf(stderr, "Will connect to %s\n", accept_path);
    if ( (new_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
      perror("socket error");
      exit(-1);
    }
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, accept_path, sizeof(addr.sun_path)-1);
    if (connect(new_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        fprintf(stderr, "trying connect to %s\n", accept_path);
        perror("connect error0"); 
        exit(-1);
    }

    // start thread with new_fd and newconn
    struct conn_args *new_args = malloc(sizeof *new_args);
    pthread_t tid;
    new_args->fd = new_fd;
    new_args->conn = newconn;
    pthread_create(&tid, NULL, &sock_thread, new_args);
    // let know that new thread is ready
    write(new_fd, "ACCEOK", 6); // TODO: return addr here
}

void handle_send(struct conn_args *args, char *buf, int len){
    //PSz: discuss how to implement it long term, lib probably has to pass len
    fprintf(stderr, "netconn_write(%d): %s", len-CMD_SIZE, buf+CMD_SIZE);
    netconn_write(args->conn, buf+CMD_SIZE, len-CMD_SIZE, NETCONN_COPY); // try with NOCOPY
    write(args->fd, "SENDOK", 6);
}

void handle_recv(struct conn_args *args){
    struct netbuf *buf;
    void *data;
    u16_t len;
    if (netconn_recv(args->conn, &buf) == ERR_OK){
        netbuf_data(buf, &data, &len);
        // put two write()s instead RECVOK should be followed by len
        char *msg = malloc(len + 6);
        memcpy(msg, "RECVOK", 6);
        memcpy(msg + 6, data, len);
        write(args->fd, msg, len + 6);
        free(msg);
    }
    else
        write(args->fd, "RECVER", 6);
}

void handle_close(struct conn_args *args){
    // TODO: check this:
    close(args->fd);
    netconn_close(args->conn);
    netconn_delete(args->conn);
    free(args);
    //smth missing?
}

void *sock_thread(void *data){
    struct conn_args *args = data;
    int rc;
    char buf[BUFLEN];
    fprintf(stderr, "started, waiting for requests\n");
    while ((rc=read(args->fd, buf, sizeof(buf))) > 0) {
        printf("read %u bytes from %d: %.*s\n", rc, args->fd, rc, buf);
        if (rc < CMD_SIZE){
            perror("command too short\n");
            continue;
        }
        if (!strncmp(buf, "SEND", CMD_SIZE))
            handle_send(args, buf, rc);
        else if (!strncmp(buf, "RECV", CMD_SIZE))
            handle_recv(args);
        else if (!strncmp(buf, "BIND", CMD_SIZE))
            handle_bind(args, buf, rc);
        else if (!strncmp(buf, "CONN", CMD_SIZE))
            handle_connect(args, buf, rc);
        else if (!strncmp(buf, "LIST", CMD_SIZE))
            handle_listen(args);
        else if (!strncmp(buf, "ACCE", CMD_SIZE))
            handle_accept(args, buf, rc);
        else if (!strncmp(buf, "CLOS", CMD_SIZE)){
            handle_close(args);
            break;
        }
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
        close(args->fd);
        handle_close(args);
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

