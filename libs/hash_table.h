#include <string.h>
#include <stdlib.h>



#ifndef TABLE_SIZE
    #define TABLE_SIZE 1024
#endif

#ifdef HASH_TABLES

typedef struct Item
{
    int key;
    unsigned char *item_data;
    struct Item *item_next;
}Item;

typedef struct HashTable
{
    int collissions;
    Item *items;
    size_t items_count;
} HashTable;

size_t hash(int key)
{
  int c2=0x27d4eb2d; 
  key = (key ^ 61) ^ (key >> 16);
  key = key + (key << 3);
  key = key ^ (key >> 4);
  key = key * c2;
  key = key ^ (key >> 15);
  return key % TABLE_SIZE;
}

HashTable* init()
{
    HashTable *table = malloc(sizeof(HashTable));

    table->items = calloc(TABLE_SIZE , sizeof(Item));
    table->items_count = 0;
    table->collissions = 0;
    
    return table;
}



void* put(HashTable* table , size_t element_key , void* element)
{
    int index = hash(element_key);    

    if (table->items[index].item_data == NULL)
    {       
        table->items[index].item_data = (unsigned char*)element;
        table->items[index].key = element_key;
        table->items_count++;
        return table->items[index].item_data;
    } 

    table->collissions++;

    Item *new_item = malloc(sizeof(Item));
    new_item->item_data = (unsigned char*)element;
    new_item->key = element_key;
    new_item->item_next = NULL;                                
    

    if (table->items[index].item_next == NULL) {
        table->items[index].item_next = new_item;
        table->items_count++;
        return table->items[index].item_next;
    }

    
    Item *curr = table->items[index].item_next;   
    for(;;)
    {
        if (curr->item_next == NULL)
        {                               
            curr->item_next = new_item;   
            table->items_count++;       
            return curr->item_next;
        }
        curr = curr->item_next;
    }
    
}

void* get(HashTable* table , size_t element_key)
{
    int index = hash(element_key);
    if (table->items[index].item_data == NULL)
    {
        return NULL;
    }
    
    if (table->items[index].key == element_key)
    {
        return table->items[index].item_data;
    }

    Item *curr = table->items[index].item_next;   
    for (;;)
    {
        if (curr->key == element_key)
        {
            return curr;
        }

        if (curr->item_next == NULL)
        {
            return NULL;
        }
        
        curr = curr->item_next;      
    }
}

int del(HashTable* table , size_t element_key)
{
    int index = hash(element_key);
    if (table->items[index].item_data == NULL)
    {
        return 1;
    }

    if (table->items[index].key == element_key)
    {
        table->items[index].key = -1;
        free(table->items[index].item_data);
        table->items[index].item_data = NULL;
        table->items_count -= 1;
        
        if (table->items[index].item_next != NULL)
        {
            Item *temp = table->items[index].item_next;
            table->items[index].key = temp->key;
            table->items[index].item_data = temp->item_data;
            table->items[index].item_next = temp->item_next;

            free(temp->item_data);
            free(temp);
        } 
        return 0;
    }

    // h [c] c c
    Item *curr = table->items[index].item_next;
    if (curr->key == element_key)
    {
        table->items[index].item_next = curr->item_next;
        table->items_count -= 1;
        free(curr);
        return 0;
    }
    curr = curr->item_next;

    
    Item *prev = table->items[index].item_next;
    for (;;)
    {    
        
        if (curr->key == element_key)
        {
            prev->item_next = curr->item_next;
            table->items_count -= 1;
            free(curr);
            return 0;
        }

        prev = prev->item_next;
        curr = curr->item_next;      
    }
    
}


unsigned char** get_all(HashTable *table , int elem_size)
{   
    unsigned char** arr = malloc(table->items_count * elem_size);
      
    
    int arr_i = 0;
    for (size_t i = 0; i < TABLE_SIZE; i++)
    {
        if (table->items[i].item_data != NULL)
        { 
           
            unsigned char* data = table->items[i].item_data;
            arr[arr_i] = data;
            //memcpy(arr + arr_i, data , elem_size);
            arr_i++;     
            
            Item *curr = table->items[i].item_next;   
            for(;curr != NULL;)
            {  
                arr[arr_i] = curr->item_data;                         
                //memcpy(arr + arr_i , curr->item_data , elem_size);
                arr_i++;               
                curr = table->items[i].item_next;          
            }
        }
    }

    /*for (int i = 0; i < table->items_count; i++) {
        for (int j = 0; j < elem_size; j++) {
            printf("%02X ", arr[i][j]);
        }
        printf("\n");
    }*/
    
    
    return arr;
}

#endif