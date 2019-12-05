#include "cache.h"

// Recommended max cache and object sizes
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400


void cache_init(cache_t *sp, int n){
    sp->buf = calloc(n, sizeof(cache_object_t*));
    sp->n = n;                  /* Buffer holds max of n items */
    sp->size = 0;               /* current size of all data in cache */
    sp->front = sp->rear = 0;   /* Empty buffer iff front == rear */
    sem_init(&sp->mutex, 0, 1); /* Binary semaphore for locking */
    sem_init(&sp->rw, 0, 1);    /* Allows either one writer or multiple readers at a time */
}

void cache_deinit(cache_t *sp) {
    free(sp->buf);
}

void cache_insert(cache_t *sp, cache_object_t object){
    if(object.size > MAX_OBJECT_SIZE){
        return;
    }
    if(object.size + sp->size > MAX_CACHE_SIZE) {
        return;
    }
    sem_wait(&sp->rw);                        /* Lock the buffer */
    sp->buf[(++sp->rear) % (sp->n)] = object; /* Insert the item */
    sp->size += object.size;                  /* update cache size */
    sem_post(&sp->rw);                        /* Unlock the buffer */
}

cache_object_t cache_build_object(int size, char *url, char *content){
    cache_object_t object;
    object.size = size;
    object.url = url;
    object.content = content;
    return object;
}

cache_object_t *cache_find_object(cache_t *sp, char *url){
    static int readers = 0;
    cache_object_t *object = NULL;

    sem_wait(&sp->mutex);
    readers++;
    if(readers == 1){
        sem_wait(&sp->rw);
    }
    sem_post(&sp->mutex);

    int index = sp->front;
    while(index != sp->rear){
        if(strcmp(sp->buf[index].url, url) == 0){
            object = &(sp->buf[index]);
            break;
        }
        index = (index + 1) % (sp->n);
    }
    
    sem_wait(&sp->mutex);
    readers--;
    if(readers == 0){
        sem_post(&sp->rw);
    }
    sem_post(&sp->mutex);

    return object;
}