#include "lwip/sockets.h" 
#include <errno.h>
#include <stdio.h>


int main(int argc, char *argv[])
{
    int sockfd = 0, n = 0;
    char recvBuff[1024];
    struct sockaddr_in serv_addr; 
    memset(recvBuff, '0',sizeof(recvBuff));
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Error : Could not create socket \n");
        return 1;
    } 

    memset(&serv_addr, '0', sizeof(serv_addr)); 

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serv_addr.sin_port = htons(5000); 


    int res = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if(res < 0)
    {
       printf("\n Error : Connect Failed %d: %s \n", res, strerror(errno));
       return 1;
    } 

    while ( (n = read(sockfd, recvBuff, sizeof(recvBuff)-1)) > 0)
    {
        recvBuff[n] = 0;
        if(fputs(recvBuff, stdout) == EOF)
            printf("\n Error : Fputs error\n");
    } 
    printf("\n");

    if(n < 0)
        printf("\n Read error \n");

    return 0;
}
