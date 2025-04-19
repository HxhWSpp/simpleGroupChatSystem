#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h> 
#include <unistd.h>
#include <poll.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <stdbool.h>


#define DEFAULT_PORT "9813"

#define HASH_TABLES
#include "libs/hash_table.h"

#define MAX_CLIENT_NAME 45
#define MAX_GROUP_NAME 125
#define MAX_GROUP_MEMBERS 50
#define MAX_JOINED_GROUPS 100
#define MAX_OWNED_GROUPS 5



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
    GROUP_INFO
};


typedef struct Group
{
    int id;
    char group_name[MAX_GROUP_NAME];
    struct Client* members[MAX_GROUP_MEMBERS];
    int members_count;
} Group;

typedef struct Client
{    
    char user_name[MAX_CLIENT_NAME];
    int fd;
    int chat_group_id;
    struct Group* joined_groups[MAX_JOINED_GROUPS];
    int joined_groups_count;
    struct Group* owned_groups[MAX_OWNED_GROUPS];
    int owned_groups_count;
} Client;


//function declarations
int extract_num(unsigned char s_buffer[] , int msg_len);

void add_fd(struct pollfd *pfds[], int newfd, int *fd_count, int *fd_size);
void del_from_pfds(struct pollfd pfds[], int i, int *fd_count);

Client* add_client(HashTable* clients_table , unsigned char s_buffer[] , int sender_fd , int msg_lenght);
Client* find_client(HashTable* clients_table , int sender_fd);
int delete_client(HashTable* clients_table , int sender_fd);

void construct_group_list(Group *group_list[] , unsigned char s_buffer[] , int buffer_size, int list_size);
void get_client_joined_groups(Client *client , unsigned char s_buffer[] , int buffer_size);
void get_client_owned_groups(Client *client , unsigned char s_buffer[] , int buffer_size);

int get_all_groups(HashTable *groups , unsigned char buffer[] , int buffer_size);
Group* create_group(HashTable *groups , Client *group_owner, char buffer[] , int msg_len);
Group* join_group(HashTable *groups , Client *new_member , int group_id , char** result_msg);
int leave_group(HashTable *groups, Client *member , int group_id , char** result_msg);
int remove_group(HashTable *groups , int group_id  , int sender_fd);
int group_info(HashTable *groups , int group_id , unsigned char buffer[] , int buffer_size);
Group* find_group(HashTable *groups , int group_id);


enum MSG_TYPE deserialize_msg(unsigned char r_buffer[] , unsigned char s_buffer[] , int  *msg_lenght);
int serialize_msg(unsigned char *buffer , enum MSG_TYPE msg_type, char msg[] , char sender[] , int msg_lenght);
bool construct_msg();

volatile sig_atomic_t running = 1; 

void handle_signal(int sig) 
{
    running = 0;
}

int main(int argc , char *argv[])
{
   
    int client_socket , listener;
    int sbytes , rbytes;

    struct sockaddr_storage connecting_addr;

    struct addrinfo hints;
    struct addrinfo *servinfo;  


    memset(&hints, 0, sizeof(hints)); 
    hints.ai_family = AF_UNSPEC;     
    hints.ai_socktype = SOCK_STREAM; 
    hints.ai_flags = AI_PASSIVE;


   

    if(getaddrinfo(NULL, DEFAULT_PORT, &hints, &servinfo) != 0 ){
        printf("[ERROR] couldn't get address info for 127.0.0.1:%s\n" , DEFAULT_PORT);
        return 1;
    } 

    //set listener to the socket descriptor that socket() returns
    listener = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    if (listener == -1)
    {
        printf("[ERROR] couldn't create the listener socket\n");
        return 1;
    }
    

    //so the server can use the same port multiple times otherwise it has to wait a few minutes for the OS to free the port
    int yes =1;
    setsockopt(listener,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof yes);

    //bind the socket descriptor on a port so we can call listen() later 
    bind(listener, servinfo->ai_addr, servinfo->ai_addrlen);

    //makes the socket descriptor accept connections
    if(listen(listener , 20) == -1)
    {
        printf("[ERROR] couldn't prepare to accept connections on socket %d\n" , listener);
        return 1;
    } 

    printf("Listening on :%s\n"  , DEFAULT_PORT);  
      

    //create a dynamic array to put the accepted socket descriptors in and poll them for data
    int fd_count = 1;
    int fd_size = 100;
    struct pollfd *pfds = malloc(fd_size * sizeof(struct pollfd));
    pfds[0].fd = listener;
    pfds[0].events = POLLIN;


    //create a hash table for clients , we will use fd as a key
    HashTable *clients_table = init();
    
    
    //create a hash table for gorups , we will use g_count as a key
    
    HashTable *groups_table = init(); 

    

    Client *sender_client;
    Group* group;

    int buffer_size = 0;
    int msg_lenght = 0;
    char* result;  


    
    unsigned char r_buffer[1024];
    unsigned char s_buffer[1024];

    //when ctrl+c is detected it calls handle_signal;
    signal(SIGINT, handle_signal);
   

    while(running == 1)
    {    
        
        //poll the sockets for data
        int poll_count = poll(pfds, fd_count, -1);

        for (size_t i = 0; i < fd_count; i++)
        {
           if (pfds[i].revents & POLLIN == 1) 
           {               
                if (pfds[i].fd == listener)               
                {      
                    //if the listener socket has data , it means that a client wants to connect                                                                              
                    socklen_t addr_size = sizeof connecting_addr;
                    client_socket = accept(listener, (struct sockaddr *)&connecting_addr, &addr_size);                 
                    if (client_socket != -1)
                    {
                        add_fd(&pfds, client_socket, &fd_count, &fd_size);                                               
                    }
                    else
                    {
                        printf("[ERROR] something went wrong with the socket descriptor , couldnt accept client\n");
                    }                                                                                                                                                                    
                }
                else
                {     
                    //a client has sent data to us                                                            
                    rbytes = recv(pfds[i].fd, r_buffer, sizeof(r_buffer), 0);
                                                    
                    int sender_fd = pfds[i].fd;
                    printf("%d\n" , rbytes);

                    if (rbytes <= 0) {
                        //if received bytes is <= 0 , it means that a client has disconected
                        sender_client = find_client(clients_table, sender_fd);

                        close(sender_client->fd);  
                        //find the client in the hash table and clean after him . (leave groups he is in , remove groups he is a owner to)

                        for (size_t v = sender_client->joined_groups_count; v > 0; v--)
                        {
                            leave_group(groups_table , sender_client ,sender_client->joined_groups[v - 1] , &result);
                        }

                        for (size_t o = sender_client->owned_groups_count; o > 0; o--)
                        {
                           remove_group(groups_table , sender_client->owned_groups[o - 1] , sender_client->fd);
                        }                     
                        

                        del_from_pfds(pfds, i, &fd_count);
                        
                        //delete client from the hash table
                        delete_client(clients_table, sender_fd);                      
                                                                 

                    } 
                    else 
                    {     

                        enum MSG_TYPE msg_type = deserialize_msg(r_buffer , s_buffer , &msg_lenght); 
                        
                        
                        switch (msg_type)
                        {
                        case REG:  
                            //add a client to the hash table with [user_name]
                            sender_client = add_client(clients_table , s_buffer , sender_fd , msg_lenght);
                            printf("Client [%s] added sucessfuly\n" , sender_client->user_name );
                            
                            break;
                        case JOIN_GROUP:
                            //extract the sent id for the buffer and parse it to int   
                            int g_join = atoi(s_buffer);                                                      
                            
                            sender_client = find_client(clients_table , sender_fd);   

                            group = join_group(groups_table ,  sender_client,  g_join , &result);

                            buffer_size = serialize_msg(r_buffer , SERVER_MSG , result , sender_client->user_name , strlen(result));
                            sbytes = send(sender_client->fd ,r_buffer , buffer_size , 0);
                                                                                                                                                                                                                                                                                                                                 
                            break;
                        case CLIENT_JOINED_GROUPS:
                                sender_client = find_client(clients_table , sender_fd);   
                                
                                get_client_joined_groups(sender_client , s_buffer , sizeof(s_buffer));

                               

                                    
                                buffer_size = serialize_msg(r_buffer , SERVER_MSG , s_buffer , sender_client->user_name , strlen(s_buffer));
                                sbytes = send(sender_client->fd ,r_buffer , buffer_size , 0);
                                                                    
                               
                            break;
                        case LEAVE_GROUP:
                                //extract group id for the buffer and parse it to int
                                int g_leave = atoi(s_buffer);

                                sender_client = find_client(clients_table , sender_fd);

                                leave_group(groups_table , sender_client , g_leave , &result);
                               
                                buffer_size = serialize_msg(r_buffer , SERVER_MSG , result , sender_client->user_name , strlen(result));
                                sbytes = send(sender_client->fd ,r_buffer , buffer_size , 0);
                                 

                            break;
                        case CHAT_JOIN:
                                int chat_id = atoi(s_buffer);                               
                            
                                sender_client = find_client(clients_table , sender_fd);
                                group = find_group(groups_table , chat_id);

                                for (size_t i = 0; i < group->members_count; i++)
                                {
                                    if (group->members[i]->fd == sender_client->fd)
                                    {
                                        sender_client->chat_group_id = group->id;
                                    }                                  
                                }                             
                                
                            break;
                        case CHAT_LEAVE: 
                            //if the client wants to leave a chat just set chat_group_id to -1
                            sender_client = find_client(clients_table , sender_fd);
                            sender_client->chat_group_id = -1;

                            break;
                        case CREATE_GROUP:
                            //create a group with name and save it to the group hash table
                            sender_client = find_client(clients_table , sender_fd); 

                            group =  create_group(groups_table , sender_client , s_buffer , msg_lenght);                                                                                                                                                     
                            break;
                        case CLIENT_OWNED_GROUPS:
                            //find client and construct a msg with a list of groups that he owns
                            sender_client = find_client(clients_table , sender_fd);

                            get_client_owned_groups(sender_client , s_buffer , sizeof(s_buffer));

                            buffer_size = serialize_msg(r_buffer , SERVER_MSG , s_buffer , sender_client->user_name , strlen(s_buffer));
                            sbytes = send(sender_client->fd ,r_buffer , buffer_size , 0);                                                                                                                                      
                            break;
                        case GROUP_INFO:
                            //extract the group id and construct a msg with the group's info (name , owner , members) and send it to the client
                            sender_client = find_client(clients_table, sender_fd);                          
                           
                            int group_id = atoi(s_buffer);
                           
                            group_info(groups_table , group_id , s_buffer , sizeof(s_buffer));
                            
                            buffer_size = serialize_msg(r_buffer , SERVER_MSG , s_buffer , sender_client->user_name , strlen(s_buffer));
                            sbytes = send(sender_client->fd ,r_buffer , buffer_size , 0);             
                            break;
                        case GROUP_QUERY: 
                            //construct a msg with a list of all groups (GROUP_NAME - ID) 
                            sender_client = find_client(clients_table, sender_fd);                           
                            get_all_groups(groups_table  , s_buffer , sizeof(s_buffer)); 
                            
                            buffer_size = serialize_msg(r_buffer , SERVER_MSG , s_buffer , sender_client->user_name , strlen(s_buffer));
                            sbytes = send(sender_client->fd ,r_buffer , buffer_size , 0);             
                                                            
                            
                            break;
                        case MSG: 
                            //a normal chat msg
                            //check if the client is in a group chat and send the msg to the group
                            //buffer_size = derialize_msg(r_buffer , msg_type , s_buffer , sender_client->user_name , &msg_lenght); 
                            sender_client = find_client(clients_table , sender_fd);                         
                            if (sender_client->chat_group_id != -1)
                            {
                                Group *group_send = find_group( groups_table, sender_client->chat_group_id);                              
                                buffer_size = serialize_msg(r_buffer , msg_type , s_buffer , sender_client->user_name , msg_lenght);

                                for (size_t j = 0; j < group_send->members_count; j++)
                                {
                                    if (group_send->members[j]->fd != sender_fd && group_send->members[j]->chat_group_id == group_send->id)
                                    {                                  
                                        sbytes = send(group_send->members[j]->fd ,r_buffer , buffer_size , 0);
                                    }                                                              
                                }          
                            } 
                                                                                                                                                
                            break;
                        case REMOVE_GROUP:
                           //extract the group id from the buffer and parse it to int
                            
                            int g_remove = atoi(s_buffer);                           
                                          
                            remove_group(groups_table , g_remove , sender_fd);                           
                            break;
                        
                        default:                            
                            break;
                        }
                    }                                                                                                                                                                                                                                                                                              
                }
           }          
            
        }

    }
       

    printf("Closing client sockets socket\n");
    for (size_t i = 0; i < fd_count; i++)
    {
        close(pfds[i].fd);
    }
    
    close(listener);
}

//serialize a msg and send it to the client
int serialize_msg(unsigned char *buffer , enum MSG_TYPE msg_type, char msg[] , char sender[] , int msg_lenght)
{
    int cursor = 0;
   
    int sender_len = strlen(sender);

    memcpy(buffer, &msg_type, sizeof(msg_type));
    cursor += sizeof(msg_type);
    memcpy(buffer + cursor, &msg_lenght, sizeof(msg_lenght));
    cursor += sizeof(msg_lenght);
    memcpy(buffer+cursor, msg, msg_lenght);
    cursor += msg_lenght;
    
    memcpy(buffer+cursor, &sender_len, sizeof(sender_len));
    cursor += sizeof(sender_len);
    memcpy(buffer+cursor, sender, sender_len);
    cursor += sender_len;

    return cursor;
}

//deserialize a msg sent from the client
enum MSG_TYPE deserialize_msg(unsigned char r_buffer[] , unsigned char s_buffer[] , int *msg_lenght)
{
    int cursor = 0;

    enum MSG_TYPE msg_type = 0;
   
    
    memcpy(&msg_type, r_buffer, sizeof(msg_type));
    cursor += sizeof(msg_type);
    memcpy(msg_lenght, r_buffer + cursor, sizeof(*msg_lenght));
    cursor += sizeof(*msg_lenght);
    memcpy(s_buffer, r_buffer + cursor, *msg_lenght);
    //printf(" len for deser - %d" , *msg_lenght);

    s_buffer[(*msg_lenght)] = '\0';

    
    //printf("%d\n" ,msg_len);
  
    return msg_type;

}

//add a fd to the dynamic array
void add_fd(struct pollfd *pfds[], int newfd, int *fd_count, int *fd_size)
{
    // if we don't have room, add more space in the pfds array
    if (*fd_count == *fd_size) {
        *fd_size *= 2;

        *pfds = realloc(*pfds, sizeof(**pfds) * (*fd_size));
    }    
    
    (*pfds)[*fd_count].fd = newfd;
    (*pfds)[*fd_count].events = POLLIN; 
   
    (*fd_count)++;
}

//delete a fd
void del_from_pfds(struct pollfd pfds[], int i, int *fd_count)
{
    pfds[i] = pfds[*fd_count-1];

    (*fd_count)--;
}


//add client to the hash table
Client* add_client(HashTable *clients_table , unsigned char s_buffer[] , int sender_fd , int msg_lenght)
{
    
    Client *client = malloc(sizeof(Client));
    client->fd = sender_fd;
    strncpy(client->user_name , s_buffer , msg_lenght);
    client->joined_groups_count = 0;
    client->owned_groups_count = 0;
    client->chat_group_id = -1;
        

    return (Client*)put(clients_table , client->fd , client);
     
}


//delete a client from the hash table
int delete_client(HashTable* clients_table , int sender_fd)
{
   return del(clients_table , sender_fd);
}

//find client using sender_fd as a key
Client* find_client(HashTable* clients_table , int sender_fd)
{
    return (Client*)get(clients_table , sender_fd);
}

//create a group using g_count as a key and set g_count a the newly created group's id
Group* create_group(HashTable *groups , Client *group_owner , char buffer[] , int msg_len)
{   
    Group *new_group = malloc(sizeof(Group));
    new_group->id = groups->items_count;
    strncpy(new_group->group_name ,buffer, msg_len);
    //printf("msg len - %d\n" , msg_len);
    new_group->members[0] = group_owner; 
    new_group->members_count = 1;

    Group* added_group = (Group*)put(groups, groups->items_count, new_group);
    

    group_owner->owned_groups[group_owner->owned_groups_count] = added_group;
    group_owner->owned_groups_count++;

    return added_group;
}


//find group in the hash table
Group* find_group(HashTable *groups, int group_id)
{
    return (Group*)get(groups , group_id);
}

//when removing a group clean it up form the client's arrays
void clean_group(Group *curr , Group groups[])
{
    
    
}

//remove group from the hash table
int remove_group(HashTable *groups, int group_id , int owner_fd)
{  
    Group *group = (Group*)get(groups , group_id);

    if (group->members[0]->fd != owner_fd)
    {
        return 1;
    }

    Client *owner = group->members[0];
    Client *c;
    Group *g;

    for (size_t i = 0; i < group->members_count; i++)
    {
        c = group->members[i];
        for (size_t j = 0; j < c->joined_groups_count; j++)
        {                 
            if (c->joined_groups[j]->id == group->id)
            {
                c->joined_groups[j] = c->joined_groups[c->joined_groups_count - 1];
                c->joined_groups[c->joined_groups_count - 1] = NULL;
                c->joined_groups_count--;
                break;
            }       
        }       
    }

    for (size_t i = 0; i < owner->owned_groups_count; i++)
    {
        if (owner->owned_groups[i]->id == group->id)
        {
            owner->owned_groups[i] = owner->owned_groups[owner->owned_groups_count - 1];
            owner->owned_groups[owner->owned_groups_count - 1] = NULL;
            owner->owned_groups_count--;
            break;
        }
        
    }
     
    return del(groups , group->id);      
}


//join a group
Group* join_group(HashTable *groups ,  Client *new_member,  int group_id , char** result_msg)
{
    Group *group = (Group*)get(groups , group_id);

    if (new_member->joined_groups_count == MAX_JOINED_GROUPS)
    {
        *result_msg = "[ERROR] Reached max number of joined groups!\n\0";
        return NULL;
    }
    

    for (size_t i = 0; i < group->members_count; i++)
    {
        if (new_member->fd == group->members[i]->fd)
        {
            *result_msg = "[ERROR] You are already in this group\n\0";
            return NULL;
        }  
    }
    
    group->members[group->members_count] = new_member;
    group->members_count += 1;

    new_member->joined_groups[new_member->joined_groups_count] = group;
    new_member->joined_groups_count += 1;

    *result_msg = "[SUCCESSFUL] Successfully joined group\n\0";

    return group;

    
   
}


//leave a group
int leave_group(HashTable *groups , Client *member , int group_id , char** result_msg)
{
    //check on group's side [TODO!]
    Group *group = (Group*)get(groups , group_id);
    bool left_on_client_side = false;

    for (size_t i = 1; i < group->members_count; i++)
    {
        if (member->fd = group->members[i]->fd)
        {
            group->members[i] = group->members[group->members_count - 1];
            group->members[group->members_count - 1] = NULL;
            group->members_count--;
            left_on_client_side = true;
        }
    }


    if (left_on_client_side == false)
    {
        *result_msg = "[ERROR] Cant leave groups you are a owner to , or not in\n\0";
        return 1;
    }

    for (size_t i = 0; i < member->joined_groups_count; i++)
    {
        if (member->joined_groups[i]->id == group->id)
        {
            member->joined_groups[i] = member->joined_groups[member->joined_groups_count - 1];
            member->joined_groups[member->joined_groups_count - 1] = NULL;
            member->joined_groups_count--;
        
            break;
        }
        
    }
    

    *result_msg = "[SECCESSFUL] Seccessfully left group\n\0";
    return 0;       
}


//get a group's info
int group_info(HashTable *groups , int group_id , unsigned char buffer[] , int buffer_size)
{
    Group *group = (Group*)get(groups , group_id);

    int cursor = 0;
    int name_len = strlen(group->group_name);
    strncpy(buffer + cursor , group->group_name , name_len);
    cursor += name_len;
    buffer[cursor++] = '|';  
    snprintf(buffer + cursor, buffer_size - cursor, "%d", group->id);
    cursor++;  
    buffer[cursor++] = '\n';    
    
    
    for (size_t i = 0; i < group->members_count; i++)
    {
        buffer[cursor++] = ' ';                   
        name_len = strlen(group->members[i]->user_name);
        strncpy(buffer + cursor ,group->members[i]->user_name , name_len);
        cursor += name_len;
        buffer[cursor++] = '\n';      
    }
    buffer[cursor] = '\0';

}

//get all groups
int get_all_groups(HashTable *groups , unsigned char buffer[] , int buffer_size)
{
    Group **group_list = (Group**)get_all(groups , sizeof(Group));
    //printf("Hello\n");
    
    construct_group_list(group_list , buffer , buffer_size , groups->items_count);
    
    free(group_list);
      

    
}

int extract_num(unsigned char s_buffer[] , int msg_len)
{
    int result = 0;
    int exp = 1;
    char num = ' ';
    while (msg_len != 0)
    {
        num = (char)(s_buffer[msg_len - 1]);
        result += atoi(&num) * exp;
        exp *= 10;
        msg_len--;
    }

    return result;
    
}

void get_client_joined_groups(Client* client , unsigned char s_buffer[] , int buffer_size)
{
    construct_group_list(client->joined_groups , s_buffer , buffer_size , client->joined_groups_count);   
}

void get_client_owned_groups(Client *client , unsigned char s_buffer[] , int buffer_size)
{
    construct_group_list(client->owned_groups , s_buffer , buffer_size , client->owned_groups_count);   
}


void construct_group_list(Group *group_list[] , unsigned char s_buffer[] , int buffer_size, int list_size)
{
    int cursor = 0;
    int name_len = 0;

    for (size_t i = 0; i < list_size; i++)
    {     
        name_len = strlen(group_list[i]->group_name);
        strncpy(s_buffer + cursor ,group_list[i]->group_name , name_len);
        cursor += name_len;
        s_buffer[cursor++] = '|';  
        snprintf(s_buffer + cursor, buffer_size - cursor, "%d", group_list[i]->id);
        cursor++;  
        s_buffer[cursor++] = '\n';    
        //printf("%s\n" , group_list[i]->group_name);  
    }
    s_buffer[cursor] = '\0';
    
}

