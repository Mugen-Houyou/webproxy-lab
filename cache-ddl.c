#include "cache.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

static void move_to_front_unmanaged(cache_t* cache, cache_entry_t* entry);
static void evict_lru_unmanaged(cache_t* cache);

void cache_init(cache_t* cache) {
    cache->head = cache->tail = NULL;
    cache->total_cached_bytes = 0;
    pthread_rwlock_init(&cache->ptrwlock, NULL);
}

void cache_deinit(cache_t* cache) {
    pthread_rwlock_wrlock(&cache->ptrwlock);

    cache_entry_t* curr = cache->head;
    while (curr) {
        cache_entry_t* next = curr->next;
        free(curr);
        curr = next;
    }

    cache->head = cache->tail = NULL;
    cache->total_cached_bytes = 0;
    pthread_rwlock_unlock(&cache->ptrwlock);
    pthread_rwlock_destroy(&cache->ptrwlock);
}

int cache_get(cache_t* cache, const char* uri, char* buf_out, int* size_out) {
    pthread_rwlock_rdlock(&cache->ptrwlock);
    cache_entry_t* entry = cache_lookup(cache, uri, 0, 0);
    int found = 0;

    if (entry) {
        memcpy(buf_out, entry->content, entry->content_length);
        *size_out = entry->content_length;
        move_to_front_unmanaged(cache, entry);
        found = 1;
    }

    pthread_rwlock_unlock(&cache->ptrwlock);
    return found;
}

void cache_put(cache_t* cache, const char* uri, const char* buf, int size) {
    if (size > MAX_OBJECT_SIZE)
        return;

    pthread_rwlock_wrlock(&cache->ptrwlock);
    cache_insert_unmanaged(cache, uri, buf, size);
    pthread_rwlock_unlock(&cache->ptrwlock);
}

cache_entry_t* cache_lookup(cache_t* cache, const char* uri, const int use_lock, const int update_lru) {
    if (use_lock)
        pthread_rwlock_rdlock(&cache->ptrwlock);

    cache_entry_t* entry = cache->head;
    while (entry) {
        if (strcmp(entry->uri, uri) == 0)
            break;
        entry = entry->next;
    }

    if (entry && update_lru)
        move_to_front_unmanaged(cache, entry);

    if (use_lock)
        pthread_rwlock_unlock(&cache->ptrwlock);
    return entry;
}

void cache_insert_unmanaged(cache_t* cache, const char* uri, const char* buf, int size) {
    cache_entry_t* exist = cache_lookup(cache, uri, 0, 0);
    if (exist)
        cache_remove_by_entry_unmanaged(cache, exist);

    cache_evict_policy_unmanaged(cache, size);

    cache_entry_t* entry = malloc(sizeof(cache_entry_t));
    strcpy(entry->uri, uri);
    memcpy(entry->content, buf, size);
    entry->content_length = size;
    entry->prev = entry->next = NULL;

    // Insert at front
    entry->next = cache->head;
    if (cache->head)
        cache->head->prev = entry;
    cache->head = entry;
    if (cache->tail == NULL)
        cache->tail = entry;

    cache->total_cached_bytes += size;
}

void cache_evict_policy_unmanaged(cache_t* cache, int required_size) {
    while (cache->total_cached_bytes + required_size > MAX_CACHE_SIZE) {
        if (cache->tail == NULL) break;
        evict_lru_unmanaged(cache);
    }
}

inline int cache_size(cache_t* cache) {
    return cache->total_cached_bytes;
}

void cache_remove(cache_t* cache, const char* uri) {
    pthread_rwlock_wrlock(&cache->ptrwlock);
    cache_entry_t* entry = cache_lookup(cache, uri, 0, 0);
    if (entry)
        cache_remove_by_entry_unmanaged(cache, entry);
    pthread_rwlock_unlock(&cache->ptrwlock);
}

void cache_remove_by_entry_unmanaged(cache_t* cache, cache_entry_t* entry) {
    if (entry->prev)
        entry->prev->next = entry->next;
    else
        cache->head = entry->next;

    if (entry->next)
        entry->next->prev = entry->prev;
    else
        cache->tail = entry->prev;

    cache->total_cached_bytes -= entry->content_length;
    free(entry);
}

void debug_print_cache(cache_t* cache) {
    pthread_rwlock_rdlock(&cache->ptrwlock);
    printf("[Cache state] total bytes = %zu\n", cache->total_cached_bytes);
    cache_entry_t* curr = cache->head;
    while (curr) {
        printf(" -> URI: %s (size: %d)\n", curr->uri, curr->content_length);
        curr = curr->next;
    }
    pthread_rwlock_unlock(&cache->ptrwlock);
}

static void move_to_front_unmanaged(cache_t* cache, cache_entry_t* entry) {
    if (entry == cache->head) return;

    if (entry->prev)
        entry->prev->next = entry->next;
    if (entry->next)
        entry->next->prev = entry->prev;

    if (cache->tail == entry)
        cache->tail = entry->prev;

    entry->prev = NULL;
    entry->next = cache->head;
    if (cache->head)
        cache->head->prev = entry;
    cache->head = entry;
}

static void evict_lru_unmanaged(cache_t* cache) {
    if (cache->tail)
        cache_remove_by_entry_unmanaged(cache, cache->tail);
}
