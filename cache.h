#ifndef COMP2310_CACHE_H
#define COMP2310_CACHE_H

#include <stdlib.h>
#include "csapp.h"

typedef struct single_cache {
    char hostname[MAXLINE];
    char port[MAXLINE];
    char path[MAXLINE];

    struct single_cache *previous;
    struct single_cache *next;

    size_t size;
    int used;

    char cache[0]; //content
    
} single_cache_t;

typedef struct cache_list {
    single_cache_t *cache_list;
    size_t size;

} cache_list_t;


void create_cache_space(cache_list_t *cp);
single_cache_t *cache_search(cache_list_t *cp, char *hostname, char *port, char *path);
single_cache_t *cache_new(char *hostname, char *port, char *path, char *cache, size_t cache_size);
void cache_add(cache_list_t *cp, single_cache_t *new_cache, char *eviction);
void pick_cache(cache_list_t *cp, single_cache_t *cache);
void LFU_remove(int min, cache_list_t *cp, single_cache_t *new_cache);
void LRU_remove(cache_list_t *cp, single_cache_t *new_cache);

#endif
