#include "swap.h"
#include "page.h"
#include "frame.h"
#include "devices/block.h"
#include "lib/kernel/bitmap.h"
#include "lib/kernel/hash.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "lib/stdbool.h"

struct block *swap_device;
struct bitmap *swap_slot;

static void
swap_device_init (void)
{
  swap_device = block_get_role (BLOCK_SWAP);
  if (swap_device == NULL) {
    PANIC ("No such block device.");
  }
}

static void
swap_slot_init (void)
{
  swap_slot = bitmap_create (block_size (swap_device));
  if (swap_slot == NULL) {
    PANIC ("Not enough memory for swap slot.");
  }
}

void
swap_init (void)
{
  swap_device_init ();
  swap_slot_init ();
}

void
swap_out (struct page *page)
{
  ASSERT (swap_device);
  ASSERT (pg_ofs (page) == 0);
  
  size_t idx = bitmap_scan_and_flip (swap_slot, 0, PGSIZE / BLOCK_SECTOR_SIZE, false);
  block_sector_t block_idx = idx;
  void *frame = pagedir_get_page (thread_current ()->pagedir, page);

  for (int i = 0; i < PGSIZE / BLOCK_SECTOR_SIZE; i++) {
    block_write (swap_device, block_idx + i, frame + i * BLOCK_SECTOR_SIZE);
  }
  /* Update Supplemental Page Table */
  SupPageTable *spt = thread_current ()->spt;
  spt_set_swapped (spt, page, block_idx);
}

void
swap_in (void *page, void *frame)
{
  ASSERT (swap_device);
  ASSERT (pg_ofs (page) == 0);

  struct thread *cur = thread_current ();
  struct sptEntry *target = spt_get_entry (cur->spt, page);

  block_sector_t block_idx = target->block_idx;
  size_t idx = block_idx;

  for (int i = 0; i < PGSIZE / BLOCK_SECTOR_SIZE; i++) {
    block_read (swap_device, block_idx + i, frame + i * BLOCK_SECTOR_SIZE);
  }
  bitmap_set_multiple (swap_slot, idx, PGSIZE / BLOCK_SECTOR_SIZE, false);
}
 
