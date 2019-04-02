#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <memory.h>
#include <errno.h>
#include "common.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "map.h"
#include <dirent.h> 

#define THREADS_COUNT 20 
#define SYNC 1
#define REQUEST 0
#define IP_LENGTH 16
#define NAME_LENGTH 32
#define BUFFER_SIZE 1024

map_str_t map;
int n_map_entries = 0;

char node_ip[IP_LENGTH];
char node_name[NAME_LENGTH];
short node_port;

char occupied_thread[THREADS_COUNT];
struct sockaddr_in server_addr;

unsigned short is_initiated=0;

int find_free_thread(){
	int i;
	for(i = 0; i < THREADS_COUNT; i++)
		if(occupied_thread[i] == 0) return i;
	
	return -1;
}

char* parse_node(char* node){

    char* node_info = malloc(32*sizeof(char));
    strcpy(node_info, node);
    char node_files[BUFFER_SIZE]; 
    int i;
    
    char* last_colon = strchr(strchr(strchr(node_info, ':') + 1, ':') + 1, ':');    
    strcpy(node_files, last_colon + 1);
    node_info[last_colon - node_info + 1] = '\0';
	
	if(!map_get(&map, node_info)) n_map_entries++;
    map_set(&map, node_info, node_files);
    return node_info;
}


void handle_request(node_info_t* node_info){
    int comm_socket_fd = node_info->comm_socket_fd;
    struct sockaddr_in* client_addr = node_info->node_addr;
    int addr_len = node_info->addr_len;
    int ind = node_info->t_number;
    char data_buffer[BUFFER_SIZE];
    int sent_recv_bytes;
    int type = 0; 

    sent_recv_bytes = recvfrom(comm_socket_fd, (int*) &type, sizeof(int), 0,
                                  (struct sockaddr_in *) client_addr, &addr_len);

    if (sent_recv_bytes == 0) {
            close(comm_socket_fd);
            return; 
        }

    if(type){
        printf("Client %s:%u wants to synchronize\n", inet_ntoa(client_addr -> sin_addr), 
                ntohs(client_addr -> sin_port));

        memset(data_buffer, 0, sizeof(data_buffer));
        sent_recv_bytes = recvfrom(comm_socket_fd, (char *) data_buffer, sizeof(data_buffer), 0,
                                  (struct sockaddr_in *) client_addr, &addr_len);

        if (sent_recv_bytes == 0) {
            close(comm_socket_fd);
            return; 
        }
        
        char* node_i = parse_node(data_buffer);
        printf("Client %s has such files: %s\n", node_i, *map_get(&map, node_i));
        free(node_i);
        int number_of_nodes = 0;

        sent_recv_bytes = recvfrom(comm_socket_fd, (int*) &number_of_nodes, sizeof(int), 0,
                                  (struct sockaddr_in *) client_addr, &addr_len);

        if (sent_recv_bytes == 0) {
            close(comm_socket_fd);
            return; 
        }

        for(; number_of_nodes > 0; number_of_nodes--){
            memset(data_buffer, 0, sizeof(data_buffer));
            sent_recv_bytes = recvfrom(comm_socket_fd, (char *) data_buffer, sizeof(data_buffer), 0,
                                  (struct sockaddr_in *) client_addr, &addr_len);

            if (sent_recv_bytes == 0) {
                close(comm_socket_fd);
                return; 
            }

            if (!map_get(&map, data_buffer)){
                map_set(&map, data_buffer, "");
                n_map_entries++;
            }
        }        
    }
    else{ 

        memset(data_buffer, 0, sizeof(data_buffer));
        sent_recv_bytes = recvfrom(comm_socket_fd, (char *) data_buffer, sizeof(data_buffer), 0,
                                  (struct sockaddr_in *) client_addr, &addr_len);
        
        if (sent_recv_bytes == 0) {
            close(comm_socket_fd);
            return; 
        }

        printf("Client %s:%u wants to receive file with name %s\n", inet_ntoa(client_addr -> sin_addr), 
                ntohs(client_addr -> sin_port), data_buffer);
        
        int count = -1;
        char file_path[1024] = "files/";
        strcat(file_path, data_buffer);
        FILE *fp = fopen (file_path,"r");
        if (fp == NULL) {
            printf ("There is no such file\n");
            sent_recv_bytes = sendto(comm_socket_fd, (int *) &count, sizeof(int), 0,
                             (struct sockaddr_in *) client_addr, sizeof(struct sockaddr_in));
        }
        else{
            char c;
	        while((c = fgetc(fp)) != EOF){
		        if(c == ' ' || c == '\n'){
			        ++count;
		        }
	        }
            
            printf("File contains %d words\n", count);
            sent_recv_bytes = sendto(comm_socket_fd, (int*) &count, sizeof(int), 0,
                             (struct sockaddr_in *) client_addr, sizeof(struct sockaddr_in));
            fseek(fp, 0L, SEEK_SET);  
            char word[128];
            while(count > 0){
                fscanf(fp, "%s", word);
                sent_recv_bytes = sendto(comm_socket_fd, (char*) word, sizeof(word), 0,
                             (struct sockaddr_in *) client_addr, sizeof(struct sockaddr_in));

                count--;
            }
            fclose(fp);
            printf("Client %s%u has received file %s", inet_ntoa(client_addr -> sin_addr), 
                ntohs(client_addr -> sin_port), data_buffer);
        }       
    }
    free(client_addr);
    close(comm_socket_fd);
    occupied_thread[ind] = 0;
	return; 
}

void server(){
    printf("Enter name of the node: ");
    scanf("%s", node_name);
    printf("Enter IP of the node: ");
    scanf("%s", node_ip);
    printf("Enter port of the node: ");
    scanf("%hu", &node_port);

    int master_sock_tcp_fd = 0,
        comm_socket_fd = 0,
        addr_len = 0; 

    if ((master_sock_tcp_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
        fprintf(stderr, "socket creation failed\n");
        exit(-1);
    }

    fd_set readfds;
    addr_len = sizeof(struct sockaddr_in);

    server_addr.sin_family = AF_INET;/*This socket will process only ipv4 network packets*/
    server_addr.sin_port = htons(node_port);/*Server will process any data arriving on port no 2000*/
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(master_sock_tcp_fd, (struct sockaddr_in *) &server_addr, sizeof(struct sockaddr_in)) == -1) {
        fprintf(stderr, "socket bind failed\n");
        exit(-1);
    }

    if (listen(master_sock_tcp_fd, 5) < 0) {
        fprintf(stderr, "listen failed\n");
        exit(-1);
    }

    int threads_n = THREADS_COUNT;
	pthread_t threads[THREADS_COUNT];
	memset(occupied_thread, 0, sizeof(*occupied_thread));
    node_info_t node_info = {0}; 

    is_initiated = 1;

    printf("Server started\n");
    while (1) {

        FD_ZERO(&readfds);                     /* Initialize the file descriptor set*/
        FD_SET(master_sock_tcp_fd, &readfds);  /*Add the socket to this set on which our server is running*/

        select(master_sock_tcp_fd + 1, &readfds, NULL, NULL, NULL);

        if (FD_ISSET(master_sock_tcp_fd, &readfds)) {
            struct sockaddr_in* client_addr = malloc(sizeof(struct sockaddr_in));
            comm_socket_fd = accept(master_sock_tcp_fd, (struct sockaddr_in *) client_addr, &addr_len);
            if (comm_socket_fd < 0) {
                printf("accept error : errno = %d\n", errno);
                exit(0);
            }

            printf("Connection accepted from client : %s:%u\n",
                   inet_ntoa(client_addr -> sin_addr), ntohs(client_addr -> sin_port));

            int thread = find_free_thread();
		    if(thread >= 0){
                node_info.addr_len = addr_len;
                node_info.comm_socket_fd = comm_socket_fd;
                node_info.node_addr = client_addr;
                node_info.t_number = thread;
			    if(pthread_create(&threads[thread], NULL, handle_request, &node_info) != 0){
			    	fprintf(stderr, "failed to create a thread\n");
			    	exit(1);
			    }
		    	occupied_thread[thread] = 1;
		    }
        }
    }
}

char* vis(){

    char* data = malloc(BUFFER_SIZE);
    strcpy(data, node_name);
    strcat(data, ":");
    strcat(data, node_ip);
    strcat(data, ":");
    char port[6] = "";
    sprintf(port, "%u", node_port);
    strcat(data, port);
    strcat(data, ":");
    
    DIR *d;
    struct dirent *dir;
    d = opendir("files");
    if (d) {
        
        while ((dir = readdir(d)) != NULL) {
            if(dir->d_type==DT_REG){
                strcat(data, dir->d_name);
                strcat(data, ",");
            }
        }
        
        if(data[strlen(data) - 1] == ',')
            data[strlen(data) - 1] = '\0'; 

        closedir(d);
    }
    return data;
}

void client_sync(char* node_info){
    char node_copy[1024];
    strcpy(node_copy, node_info); 
    char* name = strtok(node_copy,":");
    char* ip = strtok(NULL,":");
    char* port = strtok(NULL, ":");

    int sockfd = 0, 
        sent_recv_bytes = 0;

    struct sockaddr_in dest;
    inet_aton(ip, &dest.sin_addr);
    dest.sin_port = htons(atoi(port));
    dest.sin_family = AF_INET;
    
    sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if(connect(sockfd, (struct sockaddr_in*) &dest, sizeof(struct sockaddr_in)) < 0) printf("Connection to %s:%s failed\n",ip,port);
    else{ 
        int type = SYNC;

        sent_recv_bytes = sendto(sockfd, &type, sizeof(int), 0, (struct sockaddr_in *)&dest, sizeof(struct sockaddr_in));

        char* send_vis = vis();
        //sending current node info name:ip:port:files
        sent_recv_bytes = sendto(sockfd, send_vis, BUFFER_SIZE, 0, (struct sockaddr_in *)&dest, sizeof(struct sockaddr_in));

        map_iter_t iter = map_iter(&map);

        char* next_node;
        while(next_node = map_next(&map, &iter)){
            if(strcmp(node_info, next_node) != 0)
                sent_recv_bytes = sendto(sockfd, next_node, BUFFER_SIZE, 0, (struct sockaddr_in *)&dest, sizeof(struct sockaddr_in));       
        }
        free(send_vis);
        printf("Successfull sync with node %s:%s:%s\n", name, ip, port);
    }
    close(sockfd);
}

void client_request(char* file_name){
    char* next_node; 
    char** files;
    char* file;
    char* name;
    char* ip;
    char* port;

    int sockfd = 0, 
        sent_recv_bytes = 0,
        type = REQUEST,
        count = 0,
        addr_len = sizeof(struct sockaddr_in);

    struct sockaddr_in dest;
    
    map_iter_t iter = map_iter(&map);
    while(next_node = map_next(&map, &iter)){
        files = map_get(&map, next_node);
        file = strtok(*files,",");
        while(strcmp(file_name, file) != 0){
            file = strtok(NULL, ",");
        }
    
        if(strcmp(file_name, file) == 0){
            name = strtok(next_node,":");
            ip = strtok(NULL,":");
            port = strtok(NULL, ":");

            inet_aton(ip, &dest.sin_addr);
            dest.sin_port = htons(atoi(port));
            dest.sin_family = AF_INET;
            
            sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

            connect(sockfd, (struct sockaddr_in*) &dest, sizeof(struct sockaddr_in));

            sent_recv_bytes = sendto(sockfd, &type, sizeof(int), 0, (struct sockaddr_in *)&dest, sizeof(struct sockaddr_in));

            sent_recv_bytes = sendto(sockfd, file, BUFFER_SIZE, 0, (struct sockaddr_in *)&dest, sizeof(struct sockaddr_in));

            sent_recv_bytes =  recvfrom(sockfd, &count, sizeof(int), 0,
	           (struct sockaddr_in *)&dest, &addr_len);

            if(count == -1){
                close(sockfd);
                continue;
            }

            FILE *fp;
            char file_path[1024] = "files/";
            strcat(file_path, file_name);
            if ((fp = fopen(file_path,"w+")) == NULL){
                printf("Cannot create or open file\n");
                return;
            }
            else{
                char word[128];
                while(count > 0){
                    sent_recv_bytes =  recvfrom(sockfd, (char*) word, sizeof(word), 0,
                        (struct sockaddr_in *)&dest, &addr_len);
                    fwrite(word, sizeof(char), strlen(word), fp);        
                    fwrite(" ", sizeof(char), 1, fp);
                    count--;
                }   
                printf("\n");
                fclose(fp);
            }

            printf("File %s has received\n", file_name);
            return;
        }
    }
    printf("Known nodes do not have file %s\n", file_name);
}

void client_loop(){
    while(1){
        map_iter_t iter = map_iter(&map);
        char* key;
        while(key = map_next(&map, &iter)){
            printf("Trying to SYNC node %s\n", key);
            client_sync(key);
        }
        sleep(10);      
    } 
}

void client(){
    char node_info[1024];
    while(!is_initiated){
        sleep(1);
    }

    // printf("Enter node that you would like to connect in format node:name:ip\n");
    // scanf("%s", node_info);
    pthread_t client_sync_p, client_loop_p, client_request_p; 

    // if(pthread_create(&client_sync_p, NULL, client_sync, node_info) != 0){
    //         fprintf(stderr, "failed to create a client thread\n");
    //         exit(-1);
    //     }
    
    if(pthread_create(&client_loop_p, NULL, client_loop, NULL) != 0){
            fprintf(stderr, "failed to create a client thread\n");
            exit(-1);
        }

    char input[1024];
    while(1){
        printf("Enter CANCEL if you want to change type\n");
        printf("Enter SYNC if you want to connect to new node or REQUEST to get a file\n");
        scanf("%s", input);

        if(strcmp(input, "SYNC") == 0){
            printf("Please, wait a little..\n");
            pthread_join(client_sync_p, NULL);
            printf("Enter node that you would like to connect in format name:ip:port\n");
            scanf("%s", node_info);
            if(strcmp(node_info, "CANCEL") == 0) continue;
            if(pthread_create(&client_sync_p, NULL, client_sync, node_info) != 0){
                fprintf(stderr, "failed to create a client thread\n");
                continue;
            }
        }
        if(strcmp(input, "REQUEST") == 0){
            printf("Database of all known nodes \n");
            map_iter_t iter = map_iter(&map);
            
            char* key;
            char** files;
            while((key = map_next(&map, &iter))){
                files = map_get(&map, key);
                printf("%s: %s", key, *files);
            }
            printf("\n");
            printf("Please, wait a little..\n");
            pthread_join(client_request_p, NULL);
            printf("Enter filename: ");
            scanf("%s", input);
            if(strcmp(input, "CANCEL") == 0) continue;
            if(pthread_create(&client_request_p, NULL, client_request, input) != 0){
                fprintf(stderr, "failed to create a client thread\n");
                continue;
            }
        }                
    }
}


int main(){

    map_init(&map);
    pthread_t client_p;

    if(pthread_create(&client_p, NULL, client, NULL) != 0){
            fprintf(stderr, "failed to create a client thread\n");
            exit(-1);
        }

    server();
   
    return 0;
}