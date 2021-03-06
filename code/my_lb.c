#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define SOCK1 1
#define SOCK2 2
#define SOCK3 3
#define PORT 80

struct arg_struct
{
   int client_socket;
   char* client_address;
};


int clock1 ,clock2 ,clock3 = 0;
pthread_mutex_t clocks_lock;
pthread_mutex_t sock1_lock,sock2_lock,sock3_lock;
int sock1,sock2,sock3;

int min(int a1,int a2)
{
    int m = a1 < a2 ? a1 : a2;
    return m;
}


void *lb_worker(void *args)
{
    int client_socket = ((struct arg_struct*)args)->client_socket;
    char* client_address = ((struct arg_struct*)args)->client_address;
    char buf[2];
    recv(client_socket,buf,sizeof(buf),0);
    //printf("recieved from address: %s : %s\n" , client_address,buf);
    char type = buf[0];
    char csize = buf[1];
    int size = atoi(&csize);
    int proc_timeV ,proc_timeM = 0;
    if(type == 'M') {
        proc_timeV = 2 * size;
        proc_timeM = size;
    }
    else if(type == 'V')
    {
        proc_timeV = size;
        proc_timeM = 3 * size;
    }
    else
    {
        proc_timeV = size;
        proc_timeM = 2 * size;
    }
    
    int sock_to_send = 0;
    pthread_mutex_lock(&clocks_lock);
    int min_value = min(min(clock1+proc_timeV,clock2+proc_timeV),clock3+proc_timeM);
    if (min_value == clock1+proc_timeV)
    {
        sock_to_send = SOCK1;
        clock1 += proc_timeV;
    }
    else if(min_value == clock2+proc_timeV)
    {
        sock_to_send = SOCK2;
        clock2 += proc_timeV;
    }
    else
    {
        sock_to_send = SOCK3;
        clock3 += proc_timeM;
    }
    pthread_mutex_unlock(&clocks_lock);

    //printf("%s with data =%s choose server%d\n",client_address,buf,sock_to_send);
    if (sock_to_send == SOCK1)
    {
        pthread_mutex_lock(&sock1_lock);
        //printf("sending request from address %s, with data = %s to server1\n",client_address,buf);
        send(sock1,buf,sizeof(buf),0);
        recv(sock1,buf,sizeof(buf),0);
        pthread_mutex_unlock(&sock1_lock);
    }
    else if(sock_to_send == SOCK2)
    {
        pthread_mutex_lock(&sock2_lock);
        //printf("sending request from address %s, with data = %s to server2\n",client_address,buf);
        send(sock2,buf,sizeof(buf),0);
        recv(sock2,buf,sizeof(buf),0);
        pthread_mutex_unlock(&sock2_lock);
    }
    else
    {
        pthread_mutex_lock(&sock3_lock);
        //printf("sending request from address %s, with data = %s to server3\n",client_address,buf);
        send(sock3,buf,sizeof(buf),0);
        recv(sock3,buf,sizeof(buf),0);
        pthread_mutex_unlock(&sock3_lock);
    }
    send(client_socket,buf,sizeof(buf),0);
    //printf("sending to address: %s, data = %s from server%d\n" ,client_address, buf,sock_to_send);
    close(client_socket);
    free(((struct arg_struct*)args)->client_address);
    free(args);
}

int main(int argc, char const* argv[])
{   //opening sockets with servers


    pthread_mutex_init(&clocks_lock  , NULL);
    pthread_mutex_init(&sock1_lock, NULL);
    pthread_mutex_init(&sock2_lock, NULL);
    pthread_mutex_init(&sock3_lock, NULL);
    struct sockaddr_in server_address1;
    struct sockaddr_in server_address2;
    struct sockaddr_in server_address3;
    sock1 = socket(AF_INET, SOCK_STREAM, 0);
    sock2 = socket(AF_INET, SOCK_STREAM, 0);
    sock3 = socket(AF_INET, SOCK_STREAM, 0);
    
    server_address1.sin_family = AF_INET;
    server_address1.sin_port = htons(PORT);
    server_address2.sin_family = AF_INET;
    server_address2.sin_port = htons(PORT);
    server_address3.sin_family = AF_INET;
    server_address3.sin_port = htons(PORT);
 
    inet_pton(AF_INET, "192.168.0.101", &server_address1.sin_addr);
    inet_pton(AF_INET, "192.168.0.102", &server_address2.sin_addr);
    inet_pton(AF_INET, "192.168.0.103", &server_address3.sin_addr);
 
    connect(sock1, (struct sockaddr*)&server_address1, sizeof(server_address1));
    connect(sock2, (struct sockaddr*)&server_address2, sizeof(server_address2));
    connect(sock3, (struct sockaddr*)&server_address3, sizeof(server_address3));

    //open socket for loadBalancer and clients
    int lb_fd, new_socket, valread;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    lb_fd = socket(AF_INET, SOCK_STREAM,0);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr("10.0.0.1");
    address.sin_port = htons(PORT);

    bind(lb_fd, (struct sockaddr*)&address,sizeof(address));
    listen(lb_fd,128);
    while(1)
    {
        int client_socket  = accept(lb_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
        //printf("client entered with address %s\n",inet_ntoa(address.sin_addr));
        struct arg_struct *args = (struct arg_struct *)malloc(sizeof(struct arg_struct));
        args->client_socket = client_socket;
        args->client_address = (char*)malloc(sizeof(inet_ntoa(address.sin_addr)) + 1);
        memcpy(args->client_address, inet_ntoa(address.sin_addr), strlen(inet_ntoa(address.sin_addr))+1);
        pthread_t thread_id;
        pthread_create(&thread_id, NULL, lb_worker, (void*)args);
    }

    shutdown(lb_fd, SHUT_RDWR);
    return 0;
}