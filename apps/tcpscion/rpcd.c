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
#define RESP_SIZE (CMD_SIZE + 2)
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
    char buf[8];
    struct conn_args *args;
    struct netconn *conn;
    pthread_t tid;
    printf("NEWS received\n");
    if (read(fd, buf, sizeof(buf)) != CMD_SIZE){
        write(fd, "NEWSER", RESP_SIZE);
        perror("handle_new_sock() error on read\n");
        return;
    }
    if (strncmp(buf, "NEWS", CMD_SIZE)){
        write(fd, "NEWSER", RESP_SIZE);
        perror("handle_new_sock() wrong command\n");
        return;
    }
    conn = netconn_new(NETCONN_TCP);
    if (conn == NULL){
        write(fd, "NEWSER", RESP_SIZE);
        perror("handle_new_sock() failed at netconn_new()\n");
        return;
    }
    args = malloc(sizeof *args);
    args->fd = fd;
    args->conn = conn;
    // Create a detached thread.
    pthread_attr_t attr;
    if (pthread_attr_init(&attr)){
        perror("Attribute init failed");
        free(args);
        write(fd, "NEWSER", RESP_SIZE);
        return;
    }
    if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)){
        perror("Setting detached state failed");
        free(args);
        write(fd, "NEWSER", RESP_SIZE);
        return;
    }
    if (pthread_create(&tid, &attr, &sock_thread, args)){
        perror("handle_accept() error at pthread_create()\n");
        free(args);
        write(fd, "NEWSER", RESP_SIZE);
        return;
    }
    write(fd, "NEWSOK", RESP_SIZE);
}

void handle_bind(struct conn_args *args, char *buf, int len){
    ip_addr_t addr;
    u16_t port;
    u8_t svc;
    char *p = buf;
    printf("BIND received\n");
    if ((len < CMD_SIZE + 4 + ADDR_NONE_LEN) || (len > CMD_SIZE + 4 + ADDR_IPV6_LEN)){
        write(args->fd, "BINDER", RESP_SIZE);
        perror("handle_bind() error on read\n");
        return;
    } // TODO(PSz): add more tests

    p += CMD_SIZE; // skip "BIND"
    port = *((u16_t *)p);
    p += 2; // skip port
    svc = p[0]; // SVC Address
    p++; // skip svc
    args->conn->pcb.ip->svc = svc; // set svc for TCP/IP context
    scion_addr_raw(&addr, p[0], p + 1);
    // TODO(PSz): test bind with addr = NULL
    if (netconn_bind(args->conn, &addr, port) != ERR_OK){
        write(args->fd, "BINDER", RESP_SIZE);
        perror("handle_bind() error at netconn_bind()\n");
        return;
    }
    fprintf(stderr, "Bound port %d, svc: %d, and addr:", port, svc);
    print_scion_addr(&addr);
    write(args->fd, "BINDOK", RESP_SIZE);
}

void handle_connect(struct conn_args *args, char *buf, int len){
    // Some sanity checks with len etc...
    ip_addr_t addr;
    u16_t port, path_len;
    char *p = buf;
    printf("CONN received\n");
    p += CMD_SIZE; // skip "BIND"
    port = *((u16_t *)p);
    p += 2; // skip port
    path_len = *((u16_t *)p);
    p += 2; // skip path_len

    // add path to TCP/IP state
    spath_t *path = malloc(sizeof *path);
    path->path = malloc(path_len);
    memcpy(path->path, p, path_len);
    path->len = path_len;
    args->conn->pcb.ip->path = path;
    fprintf(stderr, "Path added, len %d\n", path_len);

    p += path_len; // skip path
    scion_addr_raw(&addr, p[0], p + 1);
    print_scion_addr(&addr);
    if (p[0] == ADDR_SVC_TYPE)
        args->conn->pcb.ip->svc = ntohs(*(u16_t*)(p + 5)); // set svc for TCP/IP context
    if (netconn_connect(args->conn, &addr, port) != ERR_OK){
        write(args->fd, "CONNER", RESP_SIZE);
        perror("handle_connect() error at netconn_connect()\n");
        // FIXME(PSz): check does failing netconn_connect() free this.
        /* free(path->path); */
        /* free(path); */
        return;
    }
    write(args->fd, "CONNOK", RESP_SIZE);
}

void handle_listen(struct conn_args *args){
    printf("LIST received\n");
    if (netconn_listen(args->conn) != ERR_OK){
        write(args->fd, "LISTER", RESP_SIZE);
        perror("handle_bind() error at netconn_listen()\n");
        return;
    }
    write(args->fd, "LISTOK", RESP_SIZE);
}

void handle_accept(struct conn_args *args, char *buf, int len){
    char accept_path[strlen(LWIP_SOCK_DIR) + SOCK_PATH_LEN];
    struct netconn *newconn;
    struct sockaddr_un addr;
    int new_fd;
    printf("ACCE received\n");
    if (len != CMD_SIZE + SOCK_PATH_LEN){
        perror("handle_accept(): incorrect ACCE length\n");
        write(args->fd, "ACCEER", RESP_SIZE);
        return;
    }

    if (netconn_accept(args->conn, &newconn) != ERR_OK){
        perror("handle_accept() error at netconn_accept()\n");
        write(args->fd, "ACCEER", RESP_SIZE);
        return;
    }
    fprintf(stderr, "handle_accept(): waiting...\n");

    sprintf(accept_path, "%s%.*s", LWIP_SOCK_DIR, SOCK_PATH_LEN, buf + CMD_SIZE);
    fprintf(stderr, "Will connect to %s\n", accept_path);
    if ((new_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("handle_accept() error at socket()\n");
        write(args->fd, "ACCEER", RESP_SIZE);
        return;
    }
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, accept_path, sizeof(addr.sun_path)-1);
    if (connect(new_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("handle_accept() error at connect()\n");
        fprintf(stderr, "failed connection to %s\n", accept_path);
        write(args->fd, "ACCEER", RESP_SIZE);
        return;
    }

    // start thread with new_fd and newconn
    struct conn_args *new_args = malloc(sizeof *new_args);
    pthread_t tid;
    new_args->fd = new_fd;
    new_args->conn = newconn;

    // Create a detached thread.
    pthread_attr_t attr;
    if (pthread_attr_init(&attr)){
        perror("Attribute init failed");
        free(new_args);
        write(args->fd, "ACCEER", RESP_SIZE);
        return;
    }
    if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)){
        perror("Setting detached state failed");
        free(new_args);
        write(args->fd, "ACCEER", RESP_SIZE);
        return;
    }
    if (pthread_create(&tid, &attr, &sock_thread, new_args)){
        perror("handle_accept() error at pthread_create()\n");
        free(new_args);
        write(args->fd, "ACCEER", RESP_SIZE);
        return;
    }

    // Letting know that new thread is ready.
    u16_t tot_len, path_len = newconn->pcb.ip->path->len;
    u8_t haddr_len = get_haddr_len(newconn->pcb.ip->remote_ip.type);
    tot_len = RESP_SIZE + 2 + path_len + 1 + 4 + haddr_len;

    u8_t tmp[tot_len], *p;
    p = tmp;
    memcpy(p, "ACCEOK", RESP_SIZE);
    p += RESP_SIZE;
    *((u16_t *)(p)) = path_len;
    p += 2;
    memcpy(p, newconn->pcb.ip->path->path, path_len);
    p += path_len;
    p[0] = newconn->pcb.ip->remote_ip.type;
    p++;
    memcpy(p, newconn->pcb.ip->remote_ip.addr, 4 + haddr_len);
    write(args->fd, tmp, tot_len);
    write(new_fd, "ACCEOK", RESP_SIZE); // TODO: return addr + path? here
}

void handle_send(struct conn_args *args, char *buf, int len){
    char *p = buf;
    u32_t size;
    size_t written;

    p += CMD_SIZE; // skip "SEND"
    len -= CMD_SIZE;
    size = *((u32_t *)p);
    p += 4; // skip total size
    len -= 4; // how many bytes local read() has read.
    printf("SEND received (%d bytes to send, locally received: %d)\n", size, len);

    // This is implemented more like send_all(). If this is not desired, we
    // could allocate temporary buf or sync buf size with python's send().
    while (1){
        if (len > size){
            perror("handle_send() error: received more than to send\n");
            write(args->fd, "SENDER", RESP_SIZE);
            return;
        }
        if (netconn_write_partly(args->conn, p, len, NETCONN_COPY, &written) != ERR_OK){
            perror("handle_send() error at netconn_write()\n");
            printf("NETCONN PARTLY BROKEN: %d, %d, %d\n", len, written, size);
            write(args->fd, "SENDER", RESP_SIZE);
            return;
        }
        printf("NETCONN PARTLY OK: %d, %d, %d\n", len, written, size);
        size -= written;
        len -= written;
        if (!size) // done
            break;
        if (len > 0){ // write again from current buf
            p += written;
            continue;
        }
        // read new part from app
        len=read(args->fd, buf, BUFLEN);
        if (len < 1){
            perror("handle_send() error at local sock read()\n");
            write(args->fd, "SENDER", RESP_SIZE);
            return;
        }
        p = buf;
    }
    write(args->fd, "SENDOK", RESP_SIZE);
}

void handle_recv(struct conn_args *args){
    struct netbuf *buf;
    void *data;
    u16_t len;

    if (netconn_recv(args->conn, &buf) != ERR_OK){
        perror("handle_recv() error at netconn_recv()\n");
        write(args->fd, "RECVER", RESP_SIZE);
        return;
    }

    if (netbuf_data(buf, &data, &len) != ERR_OK){
        perror("handle_recv() error at netbuf_data()\n");
        write(args->fd, "RECVER", RESP_SIZE);
        return;
    }

    char msg[len + RESP_SIZE + 2];
    memcpy(msg, "RECVOK", RESP_SIZE);
    *((u16_t *)(msg + RESP_SIZE)) = len;  // encode len
    memcpy(msg + RESP_SIZE + 2, data, len);
    write(args->fd, msg, len + RESP_SIZE + 2);  // err handling
    netbuf_delete(buf);
}

void handle_close(struct conn_args *args){
    close(args->fd);
    netconn_close(args->conn);
    netconn_delete(args->conn);
    args->conn = NULL;
    free(args);
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
        // FIXME(PSz): crashes after RST ACK
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

