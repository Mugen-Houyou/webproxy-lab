#ifndef __CACHE_H__
#define __CACHE_H__

#include "csapp.h"

#define MAX_CACHE_SIZE (1 << 20) // 1MB
#define MAX_OBJECT_SIZE (1 << 10) // 100KB

typedef struct cache_entry {
    char uri[MAXLINE];                     // 요청 URI (key)
    char content[MAX_OBJECT_SIZE];        // 실제 컨텐츠 데이터
    int content_length;                   // 컨텐츠 길이

    struct cache_entry *prev;             // LRU 이전 노드
    struct cache_entry *next;             // LRU 다음 노드
} cache_entry_t;

typedef struct {
    cache_entry_t *head;                  // LRU 리스트의 가장 최근 노드
    cache_entry_t *tail;                  // LRU 리스트의 가장 오래된 노드
    size_t total_cached_bytes;            // 총 캐시 바이트 수
    pthread_rwlock_t ptrwlock;            // 쓰기/읽기 락
} cache_t;

// === 캐시 관련 API ===
void cache_init(cache_t *cache); 
void cache_deinit(cache_t *cache); // 전체 캐시 해제

int cache_get(cache_t *cache, const char *uri, char *buf_out, int *size_out);
void cache_put(cache_t *cache, const char *uri, const char *buf, int size);

cache_entry_t *cache_lookup(cache_t *cache, const char *uri, const int use_lock, const int update_lru);
void cache_insert_unmanaged(cache_t *cache, const char *uri, const char *buf, int size);
void cache_evict_policy_unmanaged(cache_t *cache, int required_size);
int cache_size(cache_t *cache);

void cache_remove(cache_t *cache, const char *uri);
void cache_remove_by_entry_unmanaged(cache_t *cache, cache_entry_t *entry);

void debug_print_cache(cache_t *cache); // 디버깅용 출력

#endif /* __CACHE_H__ */
