#include <stdio.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <pthread.h>
#include "lwip/sys.h"
#include "lwip/api.h"
#include "lwip/ip_addr.h"
#include "libscion/address.h"

#define LWIP_SOCK_DIR "/run/shm/lwip/"
#define RPCD_SOCKET "/run/shm/lwip/lwip"
#define PATH_LEN 36  // of "accept" socket

struct conn_arg{
    int fd;
    char accept_sock[PATH_LEN];
    struct netconn *conn;
};

void get_default_addr(){
    // get default ip_addr_t from sciond
}

void handle_new_sock(int fd, struct netconn **conn){
    *conn = netconn_new(NETCONN_TCP);
    printf("NEWS received, returning OK\n");
    write(fd, "NEWSOK", 6);
}

void handle_bind(int fd, struct netconn *conn, char *buf, int len){
    // check len > 12 < 24 ?
    // encode none address
    ip_addr_t addr;
    int port; 
    char *p = buf;
    p += 4; // skip "BIND"
    port = *((u16_t *)p);
    fprintf(stderr, "Bound port %d 0 \n", port);
    p += 2; // skip port
    fprintf(stderr, "Bound port %d 0.5 %p\n", port, &(addr.addr));
    /* scion_addr_raw(&addr, p[0], p + 1); */
    // remove the following two lines
    u8_t def_addr[] = {127, 0, 0, 1}; 
    scion_addr(&addr, 1, 2, ADDR_IPV4_TYPE, def_addr);
    //
    fprintf(stderr, "Bound port %d 1 \n", port);
    print_scion_addr(&addr);
    fprintf(stderr, "Bound port %d 2 \n", port);
    netconn_bind(conn, &addr, port); // test addr = NULL
    write(fd, "BINDOK", 6);
}

void handle_listen(int fd, struct netconn *conn){
    netconn_listen(conn);
    printf("LIST received, returning OK\n");
    write(fd, "LISTOK", 6);
}

void handle_accept(int fd, struct netconn *conn, char *buf, int len){
    char accept_path[strlen(LWIP_SOCK_DIR) + PATH_LEN];
    struct netconn *newconn;
    if (len != 4 + PATH_LEN){
        perror("Incorrect ACCE length");
        write(fd, "ACCEERR", 6);
    }

    /* netconn_accept(conn, &newconn); */

}
void process_thread(int fd){
    int rc;
    char buf[1024];
    struct netconn *conn;
    fprintf(stderr, "started, waiting for requests\n");
    while ((rc=read(fd, buf, sizeof(buf))) > 0) {
        printf("read %u bytes from %d: %.*s\n", rc, fd, rc, buf);
        if (rc < 4){
            perror("command too short\n");
            continue;
        }
        // put WRIT and RECV upfront
        if (!strncmp(buf, "NEWS", 4)){
            //check rc
            handle_new_sock(fd, &conn);
        }
        else if (!strncmp(buf, "BIND", 4)){
            //check rc
            handle_bind(fd, conn, buf, rc);
        }
        else if (!strncmp(buf, "LIST", 4)){
            handle_listen(fd, conn);
        }
        else if (!strncmp(buf, "ACCE", 4)){
            handle_accept(fd, conn, buf, rc);
        }
    }
    if (rc == -1) {
        perror("read");
        // clean here
        exit(-1);
    }
    else if (rc == 0) {
        printf("EOF\n");
        // clean here
        close(fd);
    }
}


int main() {
  struct sockaddr_un addr;
  int fd,cl;
  pthread_t tid;
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
    pthread_create(&tid, NULL, &process_thread, cl);

  }


  return 0;
}

