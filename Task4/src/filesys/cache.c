#include <debug.h>
#include <string.h>
#include "filesys/cache.h"
#include "filesys/filesys.h"
#include "threads/synch.h"

#define BUFFER_CACHE_SIZE 64

struct buffer_cache_entry_t {
  bool occupied;  

  block_sector_t disk_sector;
  uint8_t buffer[BLOCK_SECTOR_SIZE];

  bool dirty;     
  bool access;    
};
static struct lock buffer_cache_lock;
static struct buffer_cache_entry_t cache[BUFFER_CACHE_SIZE];



void
buffer_cache_init (void)
{
  lock_init (&buffer_cache_lock);

  size_t i;
  for (i = 0; i < BUFFER_CACHE_SIZE; ++ i)
  {
    cache[i].occupied = false;
  }
}


static void
buffer_cache_flush (struct buffer_cache_entry_t *entry)
{
  ASSERT (lock_held_by_current_thread(&buffer_cache_lock));
  ASSERT (entry != NULL && entry->occupied == true);

  if (entry->dirty) {
    block_write (fs_device, entry->disk_sector, entry->buffer);
    entry->dirty = false;
  }
}

void
buffer_cache_close (void)
{
  lock_acquire (&buffer_cache_lock);

  size_t i;
  for (i = 0; i < BUFFER_CACHE_SIZE; ++ i)
  {
    if (cache[i].occupied == false) continue;
    buffer_cache_flush( &(cache[i]) );
  }

  lock_release (&buffer_cache_lock);
}



static struct buffer_cache_entry_t*
buffer_cache_lookup (block_sector_t sector)
{
  size_t i;
  for (i = 0; i < BUFFER_CACHE_SIZE; ++ i)
  {
    if (cache[i].occupied == false) continue;
    if (cache[i].disk_sector == sector) {
      return &(cache[i]);
    }
  }
  return NULL; 
}


static struct buffer_cache_entry_t*
buffer_cache_evict (void)
{
  ASSERT (lock_held_by_current_thread(&buffer_cache_lock));

  static size_t clock = 0;
  while (true) {
    if (cache[clock].occupied == false) {
      return &(cache[clock]);
    }

    if (cache[clock].access) {
      cache[clock].access = false;
    }
    else break;

    clock ++;
    clock %= BUFFER_CACHE_SIZE;
  }

  struct buffer_cache_entry_t *slot = &cache[clock];
  if (slot->dirty) {
    buffer_cache_flush (slot);
  }

  slot->occupied = false;
  return slot;
}


void
buffer_cache_read (block_sector_t sector, void *target)
{
  lock_acquire (&buffer_cache_lock);

  struct buffer_cache_entry_t *slot = buffer_cache_lookup (sector);
  if (slot == NULL) {
    slot = buffer_cache_evict ();
    ASSERT (slot != NULL && slot->occupied == false);

    slot->occupied = true;
    slot->disk_sector = sector;
    slot->dirty = false;
    block_read (fs_device, sector, slot->buffer);
  }

  slot->access = true;
  memcpy (target, slot->buffer, BLOCK_SECTOR_SIZE);


  lock_release (&buffer_cache_lock);
}

void
buffer_cache_write (block_sector_t sector, const void *source)
{
  lock_acquire (&buffer_cache_lock);

  struct buffer_cache_entry_t *slot = buffer_cache_lookup (sector);
  if (slot == NULL) {
    slot = buffer_cache_evict ();
    ASSERT (slot != NULL && slot->occupied == false);

    slot->occupied = true;
    slot->disk_sector = sector;
    slot->dirty = false;
    block_read (fs_device, sector, slot->buffer);
  }

  slot->access = true;
  slot->dirty = true;
  memcpy (slot->buffer, source, BLOCK_SECTOR_SIZE);

  lock_release (&buffer_cache_lock);
}
