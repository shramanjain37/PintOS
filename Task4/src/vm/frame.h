#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <stdbool.h>
#include "threads/synch.h"
#include "lib/kernel/hash.h"
#include "lib/kernel/list.h"
#include "threads/palloc.h"
#include "threads/thread.h"

struct frame 
  {
    struct lock lock;           
    void *base;                
    struct page *page;        
  };

  struct frameTableEntry {
  void *frame;
  void *page;
  uint32_t *pagedir;

  struct hash_elem hash_elem;
  struct list_elem list_elem;
};
typedef struct hash frameTable;
void frame_init (void);

void frame_lock (struct page *);
void frame_unlock (struct frame *);

struct frame *frame_alloc_and_lock (struct page *);

void frame_free (void *);

void* frame_alloc (enum palloc_flags, void*);

#endif 
