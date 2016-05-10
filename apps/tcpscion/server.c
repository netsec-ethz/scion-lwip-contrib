#include "lwip/sockets.h"

int server()
{
    int listenfd = 0, connfd = 0;
    struct sockaddr_in serv_addr;

    char sendBuff[1025];

    listenfd = lwip_socket(AF_INET, SOCK_STREAM, 0);
    memset(&serv_addr, '0', sizeof(serv_addr));
    memset(sendBuff, '0', sizeof(sendBuff));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serv_addr.sin_port = htons(5000);

    lwip_bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    lwip_listen(listenfd, 10);

    while(1)
    {
        connfd = lwip_accept(listenfd, (struct sockaddr*)NULL, NULL);
        fprintf(stderr, "Connection\n");
        lwip_write(connfd, "123", 3);
        close(connfd);
     }
}

/* int main(int argc, char *argv[]) */
/* { */
/*     server(); */
/* } */
