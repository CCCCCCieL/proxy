#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "csapp.h"
#include "cache.h"

#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 1024

// initial the space for beginning of the array list
void create_cache_space(cache_list_t *cp) 
{
    cp->cache_list = (single_cache_t *)Malloc(sizeof(single_cache_t));
    cp->cache_list->next = cp->cache_list;
    cp->cache_list->previous=cp->cache_list;
    cp->size = 0;
}

// find if it is already cashed in the list
single_cache_t *cache_search(cache_list_t *cp, char *hostname, char *port, char *path)
{
    single_cache_t *first = cp->cache_list->next;
    for (single_cache_t *element = first; element!=cp->cache_list; element=element->next)
    {
        if (!strcmp(element->hostname,hostname)&&!strcmp(element->port,port)&&!strcmp(element->path,path)){
            return element;
        }
        
    }
    return NULL;
    
}

// make the files to a cache in single_cache_t form
single_cache_t *cache_new(char *hostname, char *port, char *path, char *cache, size_t size){
    // allocate a new space
    single_cache_t *new = Malloc(sizeof(single_cache_t)+size);

    // the basic info
    memcpy(new->hostname,hostname,strlen(hostname));
    memcpy(new->path, path,strlen(path)); 
    memcpy(new->port, port,strlen(port));
    memcpy(new->cache, cache,size);

    new->size = size;
    new->used = 1;

    return new;
}

// add the new cache into the array list
void cache_add(cache_list_t *cp, single_cache_t *new_cache, char *eviction)
{
    // check if the total cache exceed
    if (cp->size + new_cache->size > MAX_CACHE_SIZE) {
        // if LRU strtepy is wanted
        if (!strcmp(eviction,"LRU")){
            LRU_remove(&cp,new_cache);
        // if LFU strtepy is wanted
        } else if (!strcmp(eviction,"LFU")){
            single_cache_t *first = cp->cache_list->next;
            // find the least frequently used cache
            int min_used = 0;
            for (single_cache_t *element = first; element!=cp->cache_list; element=element->next)
            {
                if (element->used<min_used){
                    min_used = element -> used;
                }
                
            }
            LFU_remove(min_used, &cp, new_cache);
        }
    } 
    // add it
    new_cache->next = cp->cache_list->next;
    new_cache->next->previous = new_cache;

    cp->cache_list->next = new_cache;
    new_cache->previous = cp->cache_list;

    cp->size += new_cache->size;
    
}

// if the cache is used, it would be taken into the top of array list
void pick_cache(cache_list_t *cp, single_cache_t *cache) {
    cache -> previous->next = cache ->next;
    cache -> next -> previous = cache->previous;

    cache->previous = cp->cache_list;
    cache->next = cp->cache_list->next;

    cp->cache_list->next->previous = cache;
    cp->cache_list->next = cache;
}

// remove by lru policy
void LRU_remove(cache_list_t *cp, single_cache_t *new_cache) {
    while (cp->size + new_cache->size > MAX_CACHE_SIZE) {
        // cp->cache_list->previous is the last one
        single_cache_t *removedOne = cp->cache_list->previous;
        removedOne->previous->next = cp->cache_list;
        cp->cache_list->previous = removedOne->previous;
        cp->size -= removedOne->size;
        Free(removedOne);
    }
    
}

// remove by lfu policy
void LFU_remove(int min, cache_list_t *cp, single_cache_t *new_cache) {
    printf("LRU strategy!");
    while (cp->size + new_cache->size > MAX_CACHE_SIZE)
    {
        single_cache_t *first = cp->cache_list->next;
        for (single_cache_t *element = first; element!=cp->cache_list; element=element->next){
            if (element->used == min){
                element->previous->next = element->next;
                element->next ->previous = element->previous;
                cp->size -= element->size;

                Free(element);
            }
                
        }
    }
    
}