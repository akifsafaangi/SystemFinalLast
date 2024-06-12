#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include "utility.h"

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    if(argc != 5) {
        printf("Usage: %s <portnumber> <numberOfClients> <p> <q>\n", argv[0]);
        exit(0);
    }
    int port = atoi(argv[1]);
    int numberOfClients = atoi(argv[2]);
    int p = atoi(argv[3]);
    int q = atoi(argv[4]);

    int pid = getpid();
    int status;
    int client_fd;
    struct sockaddr_in serv_addr;

    // socket create and verification
    client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd == -1)
    {
        printf("socket creation failed...\n");
        exit(0);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);  
  
    if ((status = connect(client_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr))) < 0) {
        perror("Connection Failed \n");
        return -1;
    }
    client_information info = {pid, numberOfClients, p, q};
    // Send PID to the server
    if (write(client_fd, &info, sizeof(client_information)) < 0) {
        perror("Failed to send PID");
        close(client_fd);
        return -1;
    }
    printf("Sent PID: %d\n", pid);
    
    client_server_message msg;
    while(read(client_fd, &msg, sizeof(client_server_message)) > 0) {
        printf("%s", msg.msg);
    }

    // Close the connection
    close(client_fd);
    return 0;
}