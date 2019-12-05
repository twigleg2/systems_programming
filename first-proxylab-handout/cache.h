
#ifndef __CACHE_H__
#define __CACHE_H__

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <semaphore.h>

typedef struct{
    int size;
    char *url;
    char *content;
} cache_object_t;

typedef struct{
    cache_object_t *buf;    /* Buffer array */
    int n;       /* Maximum number of slots */
    int size;    /* Maximum total size of all content in cache */
    int front;   /* buf[(front+1)%n] is first item */
    int rear;    /* buf[rear%n] is last item */
    sem_t mutex; /* Protects accesses to buf */
    sem_t rw;    /* tracks read and write access */
} cache_t;

void cache_init(cache_t *sp, int n);
void cache_deinit(cache_t *sp);
void cache_insert(cache_t *sp, cache_object_t object);
cache_object_t cache_build_object(int size, char* url, char* content);
cache_object_t *cache_find_object(cache_t *sp, char* url);

// cache_object_t cache_remove(cache_object_t *sp); // not needed for lab


#endif //__CACHE_H__