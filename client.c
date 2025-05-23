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
#include <signal.h>

#define default_server_ip "127.0.0.1"
#define default_server_port "9813"

#define MAX_CLIENT_NAME 45


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
    SERVER_MSG,
    GROUP_INFO,
    EXIT,
    NOT_FOUND
};


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


    char* server_ip = malloc(INET_ADDRSTRLEN);
    char* server_port = malloc(INET_ADDRSTRLEN);    


    
    
    printf("Trying to get address info for %s:%s...\n" , default_server_ip , default_server_port);
    
    
   
    if(getaddrinfo(default_server_ip, default_server_port, &hints, &servinfo) != 0){
        printf("[ERROR] couldn't get address info for %s:%s\n" , default_server_ip , default_server_ip);
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
  
    //create second thread for receiving data
    pthread_create(&t1 , NULL , &recv_thread , &server_sockfd);

    //buffers 
    unsigned char s_buffer[256];

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
        //read user input     
        fgets(msg, sizeof(msg), stdin);      
        msg[strcspn(msg, "\n")] = '\0';      

        //if user input starts with ! check for command , if no command has been found sets msg_type to NOT_FOUND
        if (msg[0] == '!')
        {
            check_cmd(&type , msg);
            if (type == EXIT)
            {
                break;
            }

        }
        else{
            type = MSG;
        }
        
        buffer_size = serialize_msg(s_buffer , type , msg);

        if (type != NOT_FOUND)
        {
           sbytes = send(server_sockfd , s_buffer , buffer_size , 0);
        }
        

        //resets the s_buffer with \0
        memset(s_buffer , '\0' , sizeof(s_buffer));
    }

    //shutdown read and write for the socket so recv thread can exit
    shutdown(server_sockfd , SHUT_RDWR);
    //close the socket
    close(server_sockfd);

    //wait for recv thread to finish
    pthread_join(t1 ,NULL);
    printf("Shuting down client\n");

    //close(server_sockfd);

    return 0;
}


//serializes the msg - 4bytes for type , 4bytes for msg len , the rest is the msg
int serialize_msg(unsigned char *buffer , enum MSG_TYPE msg_type, char msg[])
{

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

//deserializes the msg recvieved by the server
int deserialize_msg(unsigned char r_buffer[] , unsigned char s_buffer[] , char sender[] , int *sender_lenght , enum MSG_TYPE *server_response_type)
{
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
    s_buffer[msg_len] = '\0';
    return msg_len;

}


//checks for command and reformats the msg
void check_cmd(enum MSG_TYPE *type , char msg[])
{
    char cmd[13];
    int cmd_len = strcspn(msg + 1, " ");
    int payload_len = strlen(msg) - cmd_len;
    strncpy(cmd , &msg[1] , cmd_len);
    cmd[cmd_len] = '\0';

    if (strcmp(cmd , "CREATE") == 0)
    {      
        *type = CREATE_GROUP;       
        strcpy(msg , &msg[cmd_len + 2]);
        msg[payload_len] = '\0';    
    }
    else if (strcmp(cmd , "JOIN") == 0)
    {
        *type = JOIN_GROUP;
        
        strcpy(msg , &msg[cmd_len + 2]);
        msg[payload_len] = '\0';    

        
    }
    else if (strcmp(cmd , "GROUPS") == 0)
    {
        *type = GROUP_QUERY;

        strcpy(msg , &msg[cmd_len + 2]);
        msg[payload_len] = '\0';    
        
    }
    else if(strcmp(cmd , "REMOVE") == 0){
        *type = REMOVE_GROUP;

        strcpy(msg , &msg[cmd_len + 2]);
        msg[payload_len] = '\0';    
    }
    else if(strcmp(cmd , "CHAT") == 0){
        *type = CHAT_JOIN;

        strcpy(msg , &msg[cmd_len + 2]);
        msg[payload_len] = '\0';    
    }
    else if(strcmp(cmd , "CHATL") == 0){
        *type = CHAT_LEAVE;

        strcpy(msg , &msg[cmd_len + 2]);
        msg[payload_len] = '\0';    
    }
    else if(strcmp(cmd , "LEAVE") == 0){
        *type = LEAVE_GROUP;

        strcpy(msg , &msg[cmd_len + 2]);
        msg[payload_len] = '\0';    
    }
    else if(strcmp(cmd , "OWNED") == 0){
        *type = CLIENT_OWNED_GROUPS;
       
        strcpy(msg , &msg[cmd_len + 2]);
        msg[payload_len] = '\0';    
    }
    else if(strcmp(cmd , "JOINED") == 0){
        *type = CLIENT_JOINED_GROUPS;
        
        strcpy(msg , &msg[cmd_len + 2]);
        msg[payload_len] = '\0';    
    }
    else if(strcmp(cmd , "INFO") == 0){
        *type = GROUP_INFO;
       
        strcpy(msg , &msg[cmd_len + 2]);
        msg[payload_len] = '\0';    
    }
    else if(strcmp(cmd , "EXIT") == 0){
        *type = EXIT;
        
        strcpy(msg , &msg[cmd_len + 2]);
        msg[payload_len] = '\0';    
    }
    else{
        *type = NOT_FOUND;
    }
    
    




}


void* recv_thread(void* arg){
    int sockfd = *(int*)arg;
    
    unsigned char s_buffer[256];
    unsigned char r_buffer[256];
    char sender[MAX_CLIENT_NAME];
    int sender_len = 0;
    int msg_len = 0;
    enum MSG_TYPE msg_type = MSG;


    while(1)
    {
        int numbytes = recv(sockfd, r_buffer, 100-1, 0);
        if (numbytes <= 0)
        {
            break;
        }
        
          
        msg_len = deserialize_msg(r_buffer , s_buffer , sender , &sender_len , &msg_type);
        if (msg_type != SERVER_MSG)
        {
            printf("%.*s : " ,sender_len, sender);
            printf("%.*s\n" , msg_len, s_buffer);
        }
        else
        {
            printf("%.*s" , msg_len, s_buffer); 
        }
        
                     
        
    }

    return NULL;
}

