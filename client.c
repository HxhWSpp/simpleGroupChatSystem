#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h> 
#include <unistd.h>
#include <pthread.h>
#include <poll.h>
#include <stdlib.h>

#define default_server_ip "127.0.0.1"
#define default_server_port "9813"


enum MSG_TYPE{   
    MSG,
    LOGIN,
    REG,
    JOIN_GROUP,
    LEAVE_GROUP,
    CREATE_GROUP,
    CHANGE_NAME,
    GROUP_QUERY,
    REMOVE_GROUP,
    CHAT_JOIN,
    CHAT_LEAVE,
    CLIENT_JOINED_GROUPS,
    CLIENT_OWNED_GROUPS,
    SERVER_MSG
};

//const char commands[][13] = {"JOIN" , "CREATE" , "CHANGE_NAME" , "JOIN" "LEAVE" ,"CREATE" };
//const cmd_count = 5;


//function declarations
void* recv_thread(void* arg);

int serialize_msg(unsigned char *buffer , enum MSG_TYPE msg_type, char msg[]);
int deserialize_msg(unsigned char r_buffer[] , unsigned char s_buffer[] , char sender[] ,  int *sender_len , enum MSG_TYPE *server_response_type);

void check_cmd(enum MSG_TYPE *type , char msg[]);

int main(int argc , char *argv[])
{
    
    pthread_t t1;
    int server_sockfd; 
    int sbytes , rbytes , buffer_size;

    /*setup structs for the server info..
    getaddrinfo() fills *servinfo as a linked list
    */
    struct addrinfo hints, *servinfo;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    //alocate enough for the server ip and '\0'    
    char* server_ip = malloc(INET_ADDRSTRLEN);
    char* server_port = malloc(INET_ADDRSTRLEN);    


    printf("Server IP: ");
    fgets(server_ip, INET_ADDRSTRLEN, stdin);
    printf("Server Port: ");
    fgets(server_port, INET_ADDRSTRLEN, stdin);
    if (strlen(server_ip) < 2)
    {
        server_ip = default_server_ip;     
    }
    if (strlen(server_port) < 2)
    {
        server_port = default_server_port;
    }
    
    
    printf("Trying to get address info for %s:%s...\n" , server_ip , server_port);
    
    
   
    if(getaddrinfo(default_server_ip, default_server_port, &hints, &servinfo) != 0){
        printf("[ERROR] couldn't get address info for %s:%s\n" , server_ip , server_port);
        return 1;
    }
 
      
            
    server_sockfd = socket(servinfo->ai_family, servinfo->ai_socktype,servinfo->ai_protocol);
    if (server_sockfd == -1 )
    {
        printf("[ERROR] couldn't get socket\n");
        return -1;
    }
    

    if(connect(server_sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1){
        printf("[ERROR] couldn't connect\n");
        return -1;
    }
      
    //free servinfo struct because we dont need it anymore
    freeaddrinfo(servinfo);
  
    //create second thread for recviving data
    pthread_create(&t1 , NULL , &recv_thread , &server_sockfd);

    //buffers 

    unsigned char s_buffer[256];
    //unsigned char r_buffer[256];

    //msg[] array for user input
    char msg[150];
    printf("Type user name:\n");
    fgets(msg, sizeof(msg), stdin);
    msg[strcspn(msg, "\n")] = '\0';
    buffer_size = serialize_msg(s_buffer , REG , msg);  
    sbytes = send(server_sockfd , s_buffer , buffer_size , 0);  

    enum MSG_TYPE type = 0;

    while(1)
    {        
        fgets(msg, sizeof(msg), stdin);      
        msg[strcspn(msg, "\n")] = '\0';

        if (msg[0] == '!')
        {
            //printf("in checking\n");
            check_cmd(&type , msg);
        }
        else{
            type = MSG;
        }
        //printf("%s\n" , msg);
        
        buffer_size = serialize_msg(s_buffer , type , msg);
        /*for (size_t i = 0; i < buffer_size; i++)
        {
            printf("0x%02X\n" , s_buffer[i]);
        }*/
        
        sbytes = send(server_sockfd , s_buffer , buffer_size , 0);
        memset(s_buffer , '\0' , sizeof(s_buffer));
    }

    //close(sockfd);

    return 0;
}

int serialize_msg(unsigned char *buffer , enum MSG_TYPE msg_type, char msg[]){
    //4bytes for type , 4bytes for len , msg rest

    int cursor = 0;
    int msg_len = strlen(msg);

    memcpy(buffer, &msg_type, sizeof(msg_type));
    cursor += sizeof(msg_type);
    memcpy(buffer + cursor, &msg_len, sizeof(msg_len));
    cursor += sizeof(msg_len);
    memcpy(buffer+cursor, msg, msg_len);
    cursor += msg_len;

    return cursor;

}

int deserialize_msg(unsigned char r_buffer[] , unsigned char s_buffer[] , char sender[] , int *sender_lenght , enum MSG_TYPE *server_response_type)
{
    //to do - empty buffers
    int cursor = 0;

    enum MSG_TYPE msg_type = 0;
    int msg_len = 0;

    memcpy(&msg_type, r_buffer, sizeof(msg_type));
    cursor += sizeof(msg_type);
    memcpy(&msg_len, r_buffer + cursor, sizeof(msg_len));
    cursor += sizeof(msg_len);
    memcpy(s_buffer, r_buffer + cursor, msg_len);
    cursor += msg_len;
    int sender_len = 0;
    memcpy(&sender_len, r_buffer + cursor, sizeof(sender_len));
    cursor += sizeof(sender_len);
    memcpy(sender, r_buffer + cursor, sender_len);
  
    (*sender_lenght) = sender_len;
    (*server_response_type) = msg_type;
    return msg_len;

}

void check_cmd(enum MSG_TYPE *type , char msg[])
{
    char cmd[13];
    int cmd_len = strcspn(msg + 1, " ");
    strncpy(cmd , &msg[1] , cmd_len);
    cmd[cmd_len] = '\0';

    if (strcmp(cmd , "CREATE") == 0)
    {
        
        *type = CREATE_GROUP;
        memset(msg , '\0' , cmd_len);
        strcpy(msg , &msg[cmd_len + 2]);
        //msg[cmd_len] = '\0';       
    }
    else if (strcmp(cmd , "JOIN") == 0)
    {
        *type = JOIN_GROUP;
        memset(msg , '\0' , cmd_len);
        strcpy(msg , &msg[cmd_len + 2]);
        msg[cmd_len] = '\0';
        
    }
    else if (strcmp(cmd , "GROUPS") == 0)
    {
        *type = GROUP_QUERY;
        memset(msg , '\0' , cmd_len);
        strcpy(msg , &msg[cmd_len + 2]);
        msg[cmd_len] = '\0';
        
    }
    else if (strcmp(cmd , "CHANGE") == 0)
    {
        *type = CHANGE_NAME;
        memset(msg , '\0' , cmd_len);
        strcpy(msg , &msg[cmd_len + 2]);
        msg[cmd_len] = '\0';
        
    }
    else if(strcmp(cmd , "REMOVE") == 0){
        *type = REMOVE_GROUP;
        memset(msg , '\0' , cmd_len);
        strcpy(msg , &msg[cmd_len + 2]);
        msg[cmd_len] = '\0';
    }
    else if(strcmp(cmd , "CHAT") == 0){
        *type = CHAT_JOIN;
        memset(msg , '\0' , cmd_len);
        strcpy(msg , &msg[cmd_len + 2]);
        msg[cmd_len] = '\0';
    }
    else if(strcmp(cmd , "LEAVE") == 0){
        *type = LEAVE_GROUP;
        memset(msg , '\0' , cmd_len);
        strcpy(msg , &msg[cmd_len + 2]);
        msg[cmd_len] = '\0';
    }
    else if(strcmp(cmd , "OWNED") == 0){
        *type = CLIENT_OWNED_GROUPS;
        memset(msg , '\0' , cmd_len);
        strcpy(msg , &msg[cmd_len + 2]);
        msg[cmd_len] = '\0';
    }
    else if(strcmp(cmd , "JOINED") == 0){
        *type = CLIENT_JOINED_GROUPS;
        memset(msg , '\0' , cmd_len);
        strcpy(msg , &msg[cmd_len + 2]);
        msg[cmd_len] = '\0';
    }
    




}


void* recv_thread(void* arg){
    int sockfd = *(int*)arg;
    
    unsigned char s_buffer[256];
    unsigned char r_buffer[256];
    char sender[11];
    int sender_len = 0;
    int msg_len = 0;
    enum MSG_TYPE msg_type = MSG;


    while(1)
    {
        int numbytes = recv(sockfd, r_buffer, 100-1, 0);
                
        msg_len = deserialize_msg(r_buffer , s_buffer , sender , &sender_len , &msg_type);
        if (msg_type != SERVER_MSG)
        {
            printf("%.*s : " ,sender_len, sender);
            printf("%.*s\n" , msg_len, s_buffer);
        }
        else
        {
            printf("%.*s\n" , msg_len, s_buffer); 
        }
        
                     
        
    }
}

