#ifndef __MY_MALLOC_H__
#define __MY_MALLOC_H__
typedef struct _meta_block{
  size_t size;
  struct _meta_block * prev;
  struct _meta_block * next;
  int free;
  void *ptr;
  char buffer[1];
}meta_block;

extern meta_block* request_heap(meta_block * last, size_t size);
extern void split_space(meta_block * curr, size_t size);
extern meta_block* merge(meta_block* curr);
extern meta_block* find_minimal(size_t size);
extern void * ts_malloc_lock(size_t size);
extern void ts_free_lock(void * ptr);
extern meta_block* request_heap_nolock(meta_block * last, size_t size);
extern void split_space_nolock(meta_block * curr, size_t size);
extern meta_block* merge_nolock(meta_block* curr);
extern meta_block* find_minimal_nolock(size_t size);
extern void * ts_malloc_nolock(size_t size);
extern void ts_free_nolock(void *ptr);
#endif
