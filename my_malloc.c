#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include "my_malloc.h"


#define META_SIZE sizeof(struct _meta_block)
#define INT_MAX  2147483647

static pthread_mutex_t init_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t sbrk_mutex = PTHREAD_MUTEX_INITIALIZER;

void * head_ptr = NULL;
void * tail_ptr = NULL;
unsigned long heapsize = 0;
unsigned long freesize = 0;

__thread meta_block * thread_head_ptr = NULL;
__thread meta_block * thread_tail_ptr = NULL;
__thread meta_block * curr = NULL;
__thread meta_block * newblock = NULL;
__thread char * temp = NULL;


meta_block* request_heap(meta_block * last, size_t size){
  meta_block * curr;
  if(!last){
    head_ptr = (meta_block*)sbrk(0);
    curr = head_ptr;
  }else{
     char * temp = (char *)last + META_SIZE + last->size;
     curr = (meta_block*)temp;
    // curr = (meta_block*)sbrk(0);
  }
  size = size + META_SIZE;
  void * request = sbrk(size);
  if(request == (void*)-1){
    return NULL;
  }
  heapsize += size;
  curr->size = size - META_SIZE;
  curr->free = 0;
  if(!last){
    head_ptr = curr; 
  }else{
  last -> next = curr;
  }
  curr->prev = last;
  tail_ptr = curr;
  curr->next = NULL;

  return curr;
}

void split_space(meta_block *curr, size_t size){
  meta_block* newblock;
  char * temp = (char *)curr->buffer + size;
  newblock = (meta_block*)temp;
  newblock->size = curr->size - size - META_SIZE;
  newblock -> next = curr -> next;
  newblock->prev = curr;
  if(curr -> next){
    curr->next->prev = newblock;
  }
  curr->next = newblock;
  newblock->free = 1;
  curr->size = size;
  freesize -= (size + META_SIZE);
  if(newblock->next && newblock->next->free){
    newblock = merge(newblock);
  }
  if(curr == tail_ptr){
    tail_ptr = newblock;
  }
}

meta_block* merge(meta_block*  curr){
  curr->size = curr->size + curr->next->size + META_SIZE;
  if(curr->next->next){
    curr->next->next->prev = curr;
  }else{
    tail_ptr = curr;
  }
  curr->next = curr->next->next;
  return curr;
}
char* get_block(void * ptr){
  return (temp = ptr -= META_SIZE - 8);
}
void ts_free_lock(void * ptr){
  if(ptr == NULL){
    return;
  }
  
  //  char* temp = (char *)ptr - 28;
  // meta_block * curr = (meta_block*)temp;
  // freesize += curr->size + META_SIZE;
  
  meta_block * curr = (meta_block*)get_block(ptr);
  curr->free = 1;
  if(curr -> next != NULL && curr -> next -> free){
    curr = merge(curr);
  }
  if(curr -> prev != NULL && curr -> prev -> free){
    curr = curr -> prev;
    curr = merge(curr);
  }
}

meta_block* find_minimal(size_t size){
   meta_block * curr = head_ptr;
  meta_block * mini = NULL;
  size_t minimal = SIZE_MAX;
  while(curr){
    if(curr -> size >= size && curr -> free){
      if(curr->size == size){
         mini = curr;
	 break;
      }
     	if(curr -> size - size < minimal){
	  minimal = curr -> size -size;
      mini = curr;
	}
    }
    curr = curr -> next;
  }
  return mini;
}
/*
meta_block* request_heap_nolock(meta_block * last, size_t size){
  void * request;
  meta_block * curr;
  size = size+META_SIZE;
  if(!last){
    pthread_mutex_lock(&sbrk_mutex);
    head_ptr = (meta_block*)sbrk(0);
    request = sbrk(size);
    pthread_mutex_unlock(&sbrk_mutex);
    curr = thread_head_ptr;
  }else{
    pthread_mutex_lock(&sbrk_mutex);
     curr = (meta_block*)sbrk(0);
     request = sbrk(size);
     pthread_mutex_unlock(&sbrk_mutex);
  }
  if(request == (void*)-1){
    return NULL;
  }
  heapsize += size;
  curr->size = size - META_SIZE;
  curr->free = 0;
  if(!last){
    thread_head_ptr = curr;
  }else{
  last -> next = curr;
  }
  curr->prev = last;
  thread_tail_ptr = curr;
  curr->next = NULL;

  return curr;
}
void split_space_nolock(meta_block *curr, size_t size){
  char * temp = (char *)curr->buffer + size;
  newblock = (meta_block*)temp;
  newblock->size = curr->size - size - META_SIZE;
  newblock -> next = curr -> next;
  newblock->prev = curr;
  if(curr -> next){
    curr->next->prev = newblock;
  }
  curr->next = newblock;
  newblock->free = 1;
  curr->size = size;
  freesize -= (size + META_SIZE);
   if(curr == tail_ptr){
    thread_tail_ptr = newblock;
  }
}

meta_block* find_minimal_nolock(size_t size){
   curr = thread_head_ptr;
  meta_block * mini = NULL;
  size_t minimal = SIZE_MAX;
  while(curr){
    if(curr -> size >= size && curr -> free){
      if(curr->size == size){
         mini = curr;
         break;
      }
        if(curr -> size - size < minimal){
          minimal = curr -> size -size;
      mini = curr;
        }
    }
    curr = curr -> next;
  }
  return mini;
  }*/
void* ts_malloc_lock(size_t size){
  pthread_mutex_lock(&init_mutex);
  assert(size >= 0);
  meta_block * curr;
  if(head_ptr == NULL){
    curr = request_heap(NULL, size);
    if(!curr){
      pthread_mutex_unlock(&init_mutex);
      return NULL;
    }
  }else{
    meta_block * minimal = find_minimal(size);
    if(!minimal){
      curr = request_heap(tail_ptr, size);
      if(!curr){
	pthread_mutex_unlock(&init_mutex);
	return NULL;
      }
    }else{
    curr = minimal;
    if(curr->size - size >= META_SIZE + 4){
        split_space(curr, size);
    }else{
      freesize -= (minimal->size + META_SIZE);
    }
    }
  }
  curr->free = 0;
  pthread_mutex_unlock(&init_mutex);
  return curr->buffer;
}
/*
void * ts_malloc_nolock(size_t size){
  assert(size >= 0);
  if(thread_head_ptr == NULL){
    curr = request_heap_nolock(thread_tail_ptr, size);
    if(curr == NULL){
      return NULL;
    }
  }else{
    curr = find_minimal_nolock(size);
    if(curr == NULL){
      curr = request_heap_nolock(thread_tail_ptr, size);
    }else{
      if(curr->size - size >= META_SIZE + 4){
	split_space_nolock(curr, size);
      }
    }
  }
  curr->free = 0;
  return curr->buffer;
  }*/

void * ts_malloc_nolock(size_t size){
  assert(size >= 0);
  if(thread_head_ptr == NULL){
    size = size + META_SIZE;
    pthread_mutex_lock(&sbrk_mutex);
    thread_head_ptr = (meta_block *)sbrk(0);
    void * request = sbrk(size);
    pthread_mutex_unlock(&sbrk_mutex);
    curr = thread_head_ptr;
    size -= META_SIZE;
    if(request == (void *)-1){
      return NULL;
    }
    curr->size = size;
    curr->free = 0;
    curr->next = NULL;
    thread_tail_ptr = curr;
    curr->prev = NULL;
  }else{
    curr = thread_head_ptr;
    meta_block* minimal = NULL;
  int min = INT_MAX;
  while(curr){
    if(curr->size >= size && curr->free){
      if(curr->size == size){
      minimal = curr;
      break;
    }
      if(curr->size - size < min){
      minimal = curr;
      min = curr->size - size;
    }
    }
    curr = curr->next;
  }
  if(minimal == NULL){
    size += META_SIZE;
    pthread_mutex_lock(&sbrk_mutex);
    curr = (meta_block*)sbrk(0);
    void * request = sbrk(size);
    pthread_mutex_unlock(&sbrk_mutex);
    if(request == (void*)-1){
      return NULL;
    }
    size -= META_SIZE;
    curr->size = size;
    curr->free = 0;
    thread_tail_ptr->next = curr;
    curr->next = NULL;
    curr->prev = thread_tail_ptr;
    thread_tail_ptr = curr;
  }else{
    curr = minimal;
    if(minimal->size - size >= META_SIZE + 4){
      temp = (char*)curr->buffer + size;
      newblock = (meta_block*)temp;
      newblock->next = curr->next;
      newblock->prev = curr;
      if(curr->next){
	curr->next->prev = newblock;
      }else{
        thread_tail_ptr = newblock;
      }
      curr->next = newblock;
      newblock->free = 1;
      newblock->size = curr->size - size - META_SIZE;
      curr->size = size;
      curr->free = 0;
      if(newblock->next && newblock->next->free){
	newblock->size = newblock->size + newblock->next->size + META_SIZE;
	newblock->next = newblock->next->next;
	if(newblock->next->next){
	  newblock->next->next->prev = newblock;
	}else{
          thread_tail_ptr = newblock;
	}
      }
    }
  } 
  }
  return curr->buffer;
  }
char* get_block_nolock(void * ptr){
  return (temp = ptr -= META_SIZE - 8);
}
void ts_free_nolock(void * ptr){
  if(ptr == NULL){
    return;
  }
  
  // temp = (char*)ptr - 28;
  // curr = (meta_block*)temp;
 
  temp = ptr - META_SIZE - 8;
  curr = (meta_block*)temp;
  curr->free = 1;
  /*  
  if(curr->next != NULL){
    if(curr->next->free){
      curr->size = curr->size + curr->next->size + META_SIZE;
      curr->next = curr->next->next;
      if(curr->next->next){
	curr->next->next->prev = curr;
      }else{
        thread_tail_ptr = curr;
      }
    }
  }
  if(curr->prev != NULL){
    if(curr->prev->free){
    curr = curr->prev;
    curr->size	= curr->size + curr->next->size	+ META_SIZE;
      curr->next = curr->next->next;
      if(curr->next->next){
      	curr->next->next->prev	= curr;
      }else{
      	thread_tail_ptr	= curr;
      }
    }
    }
  return;
  */
}
