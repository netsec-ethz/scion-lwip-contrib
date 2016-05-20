#include "lwip/sockets.h"
#include <pthread.h>
#include <time.h>
void * server();
void * client();

int main(){
    pthread_t sid, cid;
    pthread_create(&sid, NULL, &server, NULL);
    sleep(3);
    pthread_create(&cid, NULL, &client, NULL);
    sleep(3);
    pthread_create(&cid, NULL, &client, NULL);
    sleep(3);
    pthread_create(&cid, NULL, &client, NULL);
    sleep(3);
    pthread_create(&cid, NULL, &client, NULL);
}
