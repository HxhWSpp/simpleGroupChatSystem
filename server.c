#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h> 
#include <unistd.h>
//#include <pthread.h>
#include <poll.h>
#include <stdlib.h>
#include <stdint.h>

#define DEFAULT_PORT "9813"

#define TABLE_SIZE 1024

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
};


typedef struct Group
{
    int id;
    char group_name[MAX_GROUP_NAME];
    struct Client* owner;
    struct Client* members[MAX_GROUP_MEMBERS];
    int members_count;
    struct Group *next_group;
} Group;

typedef struct Client
{    
    char user_name[MAX_CLIENT_NAME];
    int fd;
    int chat_group_id;
    int joined_groups[MAX_JOINED_GROUPS];
    int joined_groups_count;
    int owned_groups[MAX_OWNED_GROUPS];
    int owned_groups_count;
    struct Client *next_client;
} Client;



int hash(int key)
{
  int c2=0x27d4eb2d; 
  key = (key ^ 61) ^ (key >> 16);
  key = key + (key << 3);
  key = key ^ (key >> 4);
  key = key * c2;
  key = key ^ (key >> 15);
  return key % 1024;
}


//function declarations
void add_fd(struct pollfd *pfds[], int newfd, int *fd_count, int *fd_size);
void del_from_pfds(struct pollfd pfds[], int i, int *fd_count);

int add_client(Client clients[] , unsigned char s_buffer[] , int sender_fd , int msg_lenght);
Client* find_client(Client clients[] , int sender_fd);
void change_client_name(Client *client ,unsigned char s_buffer[] ,  int msg_lenght);
int delete_client(Client clients[] , int sender_fd);

int get_all_groups(Group groups[] , unsigned char buffer[]);
int create_group(Group groups[] , int *g_count , Client *group_owner, char group_name[]);
int join_group(Group groups[] , Client *new_member , int group_id);
int leave_group(Group groups[] , Client *member , int group_id);
int remove_group(Group groups[] , int *g_count , int group_id , int owner_fd);
void group_info(Group groups[] , int group_id);
Group* find_group(Group clients[] , int group_id);


enum MSG_TYPE deserialize_msg(unsigned char r_buffer[] , unsigned char s_buffer[] , int  *msg_lenght);
int serialize_msg(unsigned char *buffer , enum MSG_TYPE msg_type, char msg[] , char sender[] , int *msg_lenght);


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

    //alocate enough for the server ip and '\0'    
    char* server_ip = malloc(INET_ADDRSTRLEN);
    char* server_port = malloc(INET_ADDRSTRLEN);    


    if (argc == 1 )
    {   
        strcpy(server_port ,DEFAULT_PORT);        
        //server_port = default_port;
    }
    else
    {
        strcpy(server_port ,argv[1]);
    }
   
         
    //get hostname of local machine
    char ip[INET_ADDRSTRLEN];
    gethostname(ip, INET_ADDRSTRLEN);


    if(getaddrinfo(NULL, server_port, &hints, &servinfo) != 0 ){
        printf("[ERROR] couldn't get address info for %s:%s\n" , server_ip , server_port);
        return 1;
    } 

    listener = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    if (listener == -1)
    {
        printf("[ERROR] couldn't create the listener socket\n");
        return 1;
    }
    

    //so the server can use the same port multiple times otherwise it has to wait a few minutes for the OS to free the port
    int yes =1;
    setsockopt(listener,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof yes);


    bind(listener, servinfo->ai_addr, servinfo->ai_addrlen);

    if(listen(listener , 20) == -1)
    {
        printf("[ERROR] couldn't prepare to accept connections on socket %d\n" , listener);
        return 1;
    } 

    printf("Listening on :%s\n"  , server_port);  
      
    int fd_count = 1;
    int fd_size = 100;
    struct pollfd *pfds = malloc(fd_size * sizeof(struct pollfd));
    pfds[0].fd = listener;
    pfds[0].events = POLLIN;


    Client clientsTable[TABLE_SIZE];
    for (size_t i = 0; i < TABLE_SIZE; i++)
    {
        clientsTable[i].fd = -1;      
        clientsTable[i].next_client = NULL;
    }
    
 
    int g_count = 0;
    Group groups[TABLE_SIZE];      
    for (size_t i = 0; i < TABLE_SIZE; i++)
    {
        groups[i].id = -1;
        groups[i].next_group = NULL;
    }  
    
    Client *sender_client;
    int buffer_size = 0;
    int msg_lenght = 0;
    int result = 0;      
    unsigned char r_buffer[256];
    unsigned char s_buffer[256];
   

    while(1){        
        
        int poll_count = poll(pfds, fd_count, -1);

        for (size_t i = 0; i < fd_count; i++)
        {
           if (pfds[i].revents & POLLIN == 1) 
           {               
                if (pfds[i].fd == listener)               
                {                                                                                    
                    socklen_t addr_size = sizeof connecting_addr;
                    client_socket = accept(listener, (struct sockaddr *)&connecting_addr, &addr_size);                 
                    if (client_socket != -1)
                    {
                        add_fd(&pfds, client_socket, &fd_count, &fd_size);                                               
                    }
                    else{
                        printf("[ERROR] something went wrong with the socket descriptor\n");
                    }                                                                                                                                                                    
                }
                else
                {                                                                 
                    rbytes = recv(pfds[i].fd, r_buffer, sizeof(r_buffer), 0);
                                                    
                    int sender_fd = pfds[i].fd;

                    if (rbytes <= 0) {

                       close(pfds[i].fd);  

                       del_from_pfds(pfds, i, &fd_count);
                       result = delete_client(clientsTable, sender_fd);
                       if (result == 0)
                       {

                       }
                                                                 

                    } 
                    else 
                    {                      
                        enum MSG_TYPE msg_type = deserialize_msg(r_buffer , s_buffer , &msg_lenght); 
                        printf("%d\n" , msg_type);
                        //printf("buffer is %s , msg-len is %d\n" , s_buffer , msg_lenght);
                        switch (msg_type)
                        {
                        case REG:  

                            result = add_client(clientsTable , s_buffer , sender_fd , msg_lenght);
                            printf("Client added to hash table at %d\n" , result);
                            
                            break;
                        case JOIN_GROUP:
                            char id_to_join[4];
                            strncpy(id_to_join , s_buffer , msg_lenght);                          
                            int g_join = atoi(id_to_join);

                            sender_client = find_client(clientsTable , sender_fd);                         
                            
                            result = join_group(groups ,  sender_client,  g_join);
                            if (result == 1)
                            {
                                printf("Couldn't join group or couldn't find group\n");
                            }
                            else
                            {
                                char group_joined[52];
                                memset(group_joined , '\0' , sizeof(group_joined));
                                sprintf(group_joined, "Successfully joined group with id %d", result);
                                int group_joined_len = strlen(group_joined);
                                buffer_size = serialize_msg(r_buffer , SERVER_MSG , group_joined , sender_client->user_name , &group_joined_len);
                            }
                                                           
                                                              
                            break;
                        case CLIENT_JOINED_GROUPS:
                                sender_client = find_client(clientsTable , sender_fd);   
                                
                                if (sender_client->joined_groups_count != 0)
                                {                              
                                    char joined_groups[MAX_JOINED_GROUPS * MAX_GROUP_NAME];
                                    int num = 0;      
                                                                                        
                                    strcpy(joined_groups , "JOINED: ");                           
                                    int cursor = strlen("JOINED: ");     

                                    for (int n = 0; n < sender_client->joined_groups_count; n++)
                                    {                                                              
                                        Group *g = find_group(groups ,sender_client->joined_groups[n]);
                                        //printf("found g %s\n" , g->group_name);
                                        strncpy(joined_groups + cursor , g->group_name , strlen(g->group_name));
                                        cursor += strlen(g->group_name);
                                                                                        
                                        num = sprintf(joined_groups+cursor, "-%d", g->id);
                                        cursor += num;
                                        joined_groups[cursor++] = ' ';
                                        
                                        
                                    }
                                    joined_groups[cursor] = '\0';

                                    
                                    buffer_size = serialize_msg(r_buffer , SERVER_MSG , joined_groups , sender_client->user_name , &cursor);
                                    sbytes = send(sender_client->fd ,r_buffer , buffer_size , 0);
                                    //printf("%s\n" , owned_groups);
                                    //printf( "%d\n", cursor);
                                    memset(joined_groups , '\0' , cursor + 1);
                                }
                            break;
                        case LEAVE_GROUP:
                                char id_to_leave[4];
                                strncpy(id_to_leave , s_buffer , msg_lenght);
                               
                                int g_leave = atoi(id_to_leave);
                                
                                sender_client = find_client(clientsTable , sender_fd);
                                result = leave_group(groups , sender_client , g_leave);                                                      
                            break;
                        case CHAT_JOIN:

                            char id_to_chat[4];
                            strncpy(id_to_chat , s_buffer , msg_lenght);
                            int chat_join = atoi(id_to_chat);
    
                            sender_client = find_client(clientsTable , sender_fd);
                            if (sender_client->chat_group_id == -1)
                            {
                                for (size_t m = 0; m < sender_client->joined_groups_count; m++)
                                {
                                    if (sender_client->joined_groups[m] == chat_join)
                                    {
                                        sender_client->chat_group_id = chat_join;
                                        break;
                                    }                                    
                                }                               
                            }
                            
                            

                            break;
                        case CHAT_LEAVE:                          
                            sender_client = find_client(clientsTable , sender_fd);
                            sender_client->chat_group_id = -1;

                            break;
                        case CREATE_GROUP:
                            sender_client = find_client(clientsTable , sender_fd);                          

                            if (sender_client->owned_groups_count != MAX_OWNED_GROUPS)
                            {
                                char group_name[MAX_GROUP_NAME];
                                memset(group_name , '\0' , msg_lenght + 1);
                                strncpy(group_name,s_buffer,msg_lenght);

                                int g_id = create_group(groups, &g_count , sender_client , group_name);
                                                                     
                                char group_created[52];
                                memset(group_created , '\0' , sizeof(group_created));
                                sprintf(group_created, "Successfully created group with id %d", g_id);
                                int group_created_len = strlen(group_created);
                                buffer_size = serialize_msg(r_buffer , SERVER_MSG , group_created , sender_client->user_name , &group_created_len);

                                sbytes = send(sender_client->fd ,r_buffer , buffer_size , 0);
                                
                            }
                            else
                            {
                                char group_warn[] = "You've reached the max number of owned groups for a client";
                                int warn_len = sizeof(group_warn);
                                buffer_size = serialize_msg(r_buffer , SERVER_MSG , group_warn , sender_client->user_name , &warn_len);
                                sbytes = send(sender_client->fd ,r_buffer , buffer_size , 0);
                            } 

                            break;
                        case CLIENT_OWNED_GROUPS:
                           
                            sender_client = find_client(clientsTable , sender_fd);   
                            if (sender_client->owned_groups_count != 0)
                            {                              
                                char owned_groups[MAX_OWNED_GROUPS * MAX_GROUP_NAME];
                                int num = -1;      
                                                                                     
                                strcpy(owned_groups , "OWNED: ");                           
                                int cursor = strlen("OWNED: ");     

                                for (int n = 0; n < sender_client->owned_groups_count; n++)
                                {                                                              
                                    Group *g = find_group(groups ,sender_client->owned_groups[n]);
                                    printf("found g %s\n" , g->group_name);
                                    strncpy(owned_groups + cursor , g->group_name , strlen(g->group_name));
                                    cursor += strlen(g->group_name);
                                                                                    
                                    num = sprintf(owned_groups+cursor, "-%d", g->id);
                                    cursor += num;
                                    owned_groups[cursor++] = ' ';
                                    
                                    
                                }
                                owned_groups[cursor] = '\0';

                                
                                buffer_size = serialize_msg(r_buffer , SERVER_MSG , owned_groups , sender_client->user_name , &cursor);
                                sbytes = send(sender_client->fd ,r_buffer , buffer_size , 0);
                                //printf("%s\n" , owned_groups);
                                //printf( "%d\n", cursor);
                                memset(owned_groups , '\0' , cursor + 1);
                            }
                            else
                            {
                                char chat_warn[] = "You dont own any groups";
                                int warn_len = sizeof(chat_warn);
                                buffer_size = serialize_msg(r_buffer , SERVER_MSG , chat_warn , sender_client->user_name , &warn_len);
                                sbytes = send(sender_client->fd ,r_buffer , buffer_size , 0);
                            }  
                                                                                                                                                                                     
                            break;
                        case CHANGE_NAME:
                            

                            break;
                        case GROUP_QUERY:                         
                            int len = get_all_groups(groups , s_buffer);
                            //printf("%s" , s_buffer);
                            buffer_size = serialize_msg(r_buffer , SERVER_MSG , s_buffer , sender_client->user_name , &len);
                            sbytes = send(pfds[i].fd ,r_buffer , buffer_size , 0);
                            
                            
                            break;
                        case MSG: 
                            
                            sender_client = find_client(clientsTable , sender_fd);
                            if (sender_client->chat_group_id != -1)
                            {
                                Group *group_send = find_group( groups, sender_client->chat_group_id);
                                buffer_size = serialize_msg(r_buffer , msg_type , s_buffer , sender_client->user_name , &msg_lenght); 
                                for (size_t j = 0; j < group_send->members_count; j++)
                                {
                                    if (group_send->members[j]->fd != sender_fd && group_send->members[j]->chat_group_id == group_send->id)
                                    {
                                        sbytes = send(group_send->members[j]->fd ,r_buffer , buffer_size , 0);
                                    }                                                              
                                }          
                            } 
                            else
                            {
                                char chat_warn[] = "You are not in a chat";
                                int warn_len = sizeof(chat_warn);
                                buffer_size = serialize_msg(r_buffer , SERVER_MSG , chat_warn , sender_client->user_name , &warn_len);
                                sbytes = send(sender_client->fd ,r_buffer , buffer_size , 0);
                            }                                                                                                                                                    
                            break;
                        case REMOVE_GROUP:
                           
                            char id_to_remove[100];
                            strncpy(id_to_remove , s_buffer , msg_lenght);
                            int g_remove = atoi(id_to_remove);                           
                                          
                            remove_group(groups , &g_count , g_remove , sender_fd);
                            
                            break;
                        
                        default:                            
                            break;
                        }
                    }                                                                                                                                                                                                                                                                                              
                }
           }          
            
        }

    }
       
}

int serialize_msg(unsigned char *buffer , enum MSG_TYPE msg_type, char msg[] , char sender[] , int *msg_lenght)
{
    int cursor = 0;
   
    int sender_len = strlen(sender);

    memcpy(buffer, &msg_type, sizeof(msg_type));
    cursor += sizeof(msg_type);
    memcpy(buffer + cursor, msg_lenght, sizeof(*msg_lenght));
    cursor += sizeof(*msg_lenght);
    memcpy(buffer+cursor, msg, *msg_lenght);
    cursor += *msg_lenght;
    
    memcpy(buffer+cursor, &sender_len, sizeof(sender_len));
    cursor += sizeof(sender_len);
    memcpy(buffer+cursor, sender, sender_len);
    cursor += sender_len;

    return cursor;
}

enum MSG_TYPE deserialize_msg(unsigned char r_buffer[] , unsigned char s_buffer[] , int *msg_lenght)
{
    int cursor = 0;

    enum MSG_TYPE msg_type = 0;
   
    
    memcpy(&msg_type, r_buffer, sizeof(msg_type));
    cursor += sizeof(msg_type);
    memcpy(msg_lenght, r_buffer + cursor, sizeof(*msg_lenght));
    cursor += sizeof(*msg_lenght);
    memcpy(s_buffer, r_buffer + cursor, *msg_lenght);
    //printf("%d\n" ,msg_len);
  
    return msg_type;

}

void add_fd(struct pollfd *pfds[], int newfd, int *fd_count, int *fd_size)
{
    // If we don't have room, add more space in the pfds array
    if (*fd_count == *fd_size) {
        *fd_size *= 2;

        *pfds = realloc(*pfds, sizeof(**pfds) * (*fd_size));
    }    
    
    (*pfds)[*fd_count].fd = newfd;
    (*pfds)[*fd_count].events = POLLIN; 
   
    (*fd_count)++;
}

void del_from_pfds(struct pollfd pfds[], int i, int *fd_count)
{
    // Copy the one from the end over this one
    pfds[i] = pfds[*fd_count-1];

    (*fd_count)--;
}

int add_client(Client clients[] , unsigned char s_buffer[] , int sender_fd , int msg_lenght)
{
    int index = hash(sender_fd);
   
    Client *client = malloc(sizeof(Client));
    client->fd = sender_fd;
    strncpy(client->user_name , s_buffer , msg_lenght);
    client->next_client = NULL;
    client->joined_groups_count = 0;
    client->owned_groups_count = 0;
    client->chat_group_id = -1;
        

    if (clients[index].fd == -1)
    {
        clients[index] = *client;
        return index;
    }
    else
    {
        //printf("Found collision for %d , adding to %d's linked list\n" , sender_fd , index);
        Client *temp = &clients[index];
        while (temp->next_client != NULL) 
        {
            temp = temp->next_client;
        }
        temp->next_client = client;
        return index;               
    }
     
}

int delete_client(Client clients[] , int sender_fd)
{
    int index = hash(sender_fd);

    Client *curr = &clients[index];
    Client *prev = NULL;
  
    if (curr->next_client == NULL)
    {
        curr->fd = -1;
        memset(curr->user_name , '\0' , sizeof(curr->user_name));
        printf("There is no next_client at %d , just reset the fields\n" , index);
        return 0;
    }
    else
    {
        while (curr->next_client != NULL)
        {
            if (curr->fd == sender_fd)
            {
                if (prev == NULL)
                {
                    Client *next = curr->next_client;
                    *curr = *next;
                    printf("There was a linked list at %d , deleted head\n" , index);                 
                    return 0;
                }
                
                prev->next_client = curr->next_client;
                free(curr);
                printf("The Client was somewhare in the middle of the linked list at %d\n" , index);
                return 0;             
            }
            prev = curr;
            curr = curr->next_client;           
        }
        if (curr->fd == sender_fd)
        {
            prev->next_client = curr->next_client;
            free(curr);
            printf("Client was the tail of the linked list at %d\n" , index); 
            return 0; 
        }
        printf("Could find client\n");
        return 1; 
    }          
}

Client* find_client(Client clients[] , int sender_fd)
{
    int index = hash(sender_fd);

    Client *client = &clients[index];
    if (client->fd == sender_fd)
    {
        return client;
    }
    else
    {
        Client *temp = &clients[index];
        while (temp->next_client != NULL) 
        {
            if (temp->fd == sender_fd)
            {
                return temp;
            }       
            temp = temp->next_client;
        }       
    }
    
}



void change_client_name(Client *client ,unsigned char s_buffer[] ,  int msg_lenght)
{
    //clear old name
    int name_len = strlen(client->user_name);
    memset(client->user_name, '\0', name_len);
    
    //set new name
    strncpy(client->user_name , s_buffer , msg_lenght);
}

int create_group(Group groups[] , int *g_count , Client *group_owner , char group_name[])
{   
    int index = hash((*g_count));
   
    Group *group = malloc(sizeof(Group));
    group->id = *g_count;
    group->owner = group_owner;  
    strcpy(group->group_name , group_name);
    

    for (size_t i = 0; i < MAX_GROUP_MEMBERS; i++)
    {
        group->members[i] = NULL;
    }
    group->members[0] = group_owner;
    group->members_count++;

    
   
    if (groups[index].id == -1)
    {
        groups[index] = *group;
        
    }
    else
    {
        //printf("Found collision for %d , adding to %d's linked list\n" , (*g_count) , index);
        Group *temp = &groups[index];
        while (temp->next_group != NULL) 
        {
            temp = temp->next_group;
        }
        temp->next_group = group;        
    }
    //printf("its over\n");
    group_owner->owned_groups[group_owner->owned_groups_count] = group->id;
    group_owner->owned_groups_count++;

    group_owner->joined_groups[group_owner->joined_groups_count] = group->id;
    group_owner->joined_groups_count++;
    (*g_count)++;
    return group->id;
}

Group* find_group(Group groups[] , int group_id)
{
    int index = hash(group_id);
    //printf("hashed %d\n" , index);

    Group *group = &groups[index];
    if (group->id == -1)
    {
        return NULL;
    }
    
    if (group->id == group_id)
    {
        return group;
    }
    else
    {
        Group *temp = &groups[index];
        while (temp->next_group != NULL) 
        {
            if (temp->id == group_id)
            {
                return temp;
            }       
            temp = temp->next_group;
        }       
    }
}

int remove_group(Group groups[] , int *g_count , int group_id , int owner_fd)
{  
    int index = hash(group_id);

    Group *curr = &groups[index];
    Group *prev = NULL;

  
    if (curr->next_group == NULL && curr->owner->fd == owner_fd && curr->id == group_id)
    {
        curr->id = -1;
        memset(curr->group_name , '\0' , sizeof(curr->group_name));
        printf("There is no next_group at %d , just reset the fields\n" , index);
        return 0;
    }
    else
    {
        while (curr->next_group != NULL)
        {
            if (curr->id == group_id && curr->owner->fd == owner_fd)
            {
                if (prev == NULL)
                {
                    Group *next = curr->next_group;
                    *curr = *next;
                    printf("There was a linked list at %d , deleted head\n" , index); 
                    (*g_count)--;                
                    return 0;
                }
                
                prev->next_group = curr->next_group;
                free(curr);
                printf("The Group was somewhare in the middle of the linked list at %d\n" , index);
                (*g_count)--;
                return 0;             
            }
            prev = curr;
            curr = curr->next_group;           
        }
        if (curr->id == group_id && curr->owner->fd == owner_fd)
        {
            prev->next_group = curr->next_group;
            free(curr);
            printf("Group was the tail of the linked list at %d\n" , index);
            (*g_count)--; 
            return 0; 
        }
        printf("Could not delete group\n"); 
    }          
    
}

int join_group(Group groups[] ,  Client *new_member,  int group_id)
{
    
    Group *group = find_group(groups , group_id);
    if (group == NULL)
    {
        return 1;
    }
    

    new_member->joined_groups[new_member->joined_groups_count] = group_id;
    new_member->joined_groups_count++;

    group->members[group->members_count] = new_member;
    group->members_count++; 
    return 0;  
   
}

int leave_group(Group groups[] , Client *member , int group_id)
{

    Group *group = find_group(groups , group_id);
    if (group == NULL)
    {
        return 1;
    }
    
    int fd = member->fd;
    
    //Client *temp = group->members[group->members_count];

    for (size_t i = 0; i < group->members_count; i++)
    {
        if (group->members[i]->fd == fd)
        {
            group->members[i] = group->members[group->members_count - 1];
            group->members[group->members_count - 1] = NULL;
            group->members_count--;

            for (size_t j = 0; j < member->joined_groups_count; j++)
            {
                if (member->joined_groups[j] == group->id)
                {
                    member->joined_groups[j] = member->joined_groups[member->joined_groups_count - 1];
                    member->joined_groups[member->joined_groups_count - 1] = -1;
                    member->joined_groups_count--;

                    return 0;
                }
                
            }       

        }       
    }         
    //pfds[i] = pfds[*fd_count-1];

    //(*fd_count)--;
}

void group_info(Group groups[] , int group_id)
{
    Group *group = find_group(groups , group_id);

    printf("GROUP NAME: %s\n" , group->group_name);
    printf("Members:\n");

    for (size_t i = 0; i < group->members_count; i++)
    {
        printf("   %s\n" , group->members[i]->user_name);
    }
}

int get_all_groups(Group groups[] , unsigned char buffer[])
{

    int cursor = 0;
    int num = 0;
    strcpy(buffer + cursor , "ALL GROUPS:\n");
    cursor += strlen("ALL GROUPS:\n");
    int name_len = 0;
    for (size_t i = 0; i < TABLE_SIZE; i++)
    {
        if (groups[i].id != -1)
        {
            name_len = strlen(groups[i].group_name);
            strncpy(buffer+cursor , groups[i].group_name , name_len);
            cursor += name_len;
            num = sprintf(buffer+cursor, "-%d", groups[i].id);
            cursor += num;
            buffer[cursor++] = '\n';           

            //cursor++;
            if (groups[i].next_group != NULL)
            {
                Group *temp = groups[i].next_group;              
                while (temp != NULL)
                {
                    name_len = strlen(temp->group_name);
                    strncpy(buffer+cursor , temp->group_name , name_len);
                    cursor += name_len;
                    num = sprintf(buffer+cursor, "-%d", temp->id);
                    cursor += num;
                    buffer[cursor++] = '\n';  
                    temp = temp->next_group;
                }             
            }
            
        }       
    }
    buffer[cursor] = '\0';
    
    return cursor;
    
}