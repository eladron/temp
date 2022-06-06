#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>
#include <thread>
#include <mutex>
#include <algorithm>
#include <vector>

#define SOCK1 1
#define SOCK2 1
#define SOCK3 1
#define PORT 80

using namespace std;

int clock1 ,clock2 ,clock3 = 0;
mutex clocks_lock;
mutex sock1_lock,sock2_lock,sock3_lock;
int sock1,sock2,sock3;

void lb_worker(int client_socket)
{
    vector<char> buf(1024);
    recv(client_socket,buf.data(),buf.size(),0);
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
    clocks_lock.lock();
    int max_value = max(max(clock1+proc_timeV,clock2+proc_timeV),clock3+proc_timeM);
    if (max_value == clock1+proc_timeV)
    {
        sock_to_send = SOCK1;
        clock1 += proc_timeV;
    }
    else if(max_value == clock2+proc_timeM)
    {
        sock_to_send = SOCK2;
        clock2 += proc_timeV;
    }
    else
    {
        sock_to_send = SOCK3;
        clock3 += proc_timeM;
    }
    clocks_lock.unlock();

    if (sock_to_send == SOCK1)
    {
        sock1_lock.lock();
        send(sock1,buf.data(),buf.size(),0);
        recv(sock1,buf.data(),buf.size(),0);
        send(client_socket,buf.data(),buf.size(),0);
        sock1_lock.unlock();
    }
    else if(sock_to_send == SOCK2)
    {
        sock2_lock.lock();
        send(sock2,buf.data(),buf.size(),0);
        recv(sock2,buf.data(),buf.size(),0);
        send(client_socket,buf.data(),buf.size(),0);
        sock2_lock.unlock();
    }
    else
    {
        sock3_lock.lock();
        send(sock3,buf.data(),buf.size(),0);
        recv(sock3,buf.data(),buf.size(),0);
        send(client_socket,buf.data(),buf.size(),0);
        sock3_lock.unlock();
    }
    close(client_socket);
}

int main(int argc, char const* argv[])
{   //opening sockets with servers
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
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    bind(lb_fd, (struct sockaddr*)&address,sizeof(address));
    listen(lb_fd,128);

    while(true)
    {
        int client_socket  = accept(lb_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
        thread t(lb_worker,client_socket);
    }

    shutdown(lb_fd, SHUT_RDWR);
    return 0;
}