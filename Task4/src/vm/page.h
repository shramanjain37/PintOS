#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include "devices/block.h"
#include "filesys/off_t.h"
#include "threads/synch.h"

typedef struct hash SupPageTable;

enum STATUS
{
  INSTALLED,
  SWAPPED,
  FSYS,
  ALLZERO
};

struct sptEntry
{
  void *page;
  void *frame;
  enum STATUS status;
  
  struct hash_elem hash_elem;
  block_sector_t block_idx;
  bool dirty;
  struct file *file;
  off_t ofs;
  uint32_t read_bytes;
  uint32_t zero_bytes;
  bool writable;
};

struct page 
  {
    void *addr;                 
    bool read_only;            
    struct thread *thread;      

    struct hash_elem hash_elem; 

    
    struct frame *frame;       

    block_sector_t sector;       
    
    bool private;               
    struct file *file;          
    off_t file_offset;          
    off_t file_bytes;           
  };

void page_exit (void);

bool page_lock (const void *, bool will_write);
void page_unlock (const void *);

struct page *page_allocate (void *, bool read_only);
void page_deallocate (void *vaddr);

bool page_in (void *fault_addr);
bool page_out (struct page *);
bool page_recently_accessed (struct page *);
SupPageTable* spt_create (void);
bool spt_add_installed (SupPageTable*, void*, void*);
bool spt_add_filesys (SupPageTable*, void*,
			struct file*, off_t, uint32_t, uint32_t, bool);
bool spt_add_allzero (SupPageTable*, void*, void*);
bool spt_delete_entry (SupPageTable*, void*);
struct sptEntry* spt_get_entry (SupPageTable*, void*);
bool spt_set_swapped (SupPageTable*, void*, block_sector_t);

hash_hash_func page_hash;
hash_less_func page_less;

#endif 
