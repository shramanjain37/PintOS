#include "vm/swap.h"
#include <bitmap.h>
#include <debug.h>
#include <stdio.h>
#include "vm/frame.h"
#include "vm/page.h"
#include "threads/synch.h"
#include "threads/vaddr.h"

static struct lock swap_lock;
static struct block *swap_device;
static struct bitmap *swap_bitmap;

#define PAGE_SECTORS (PGSIZE / BLOCK_SECTOR_SIZE)

static void read_from_swap(struct page *p);
static bool write_to_swap(struct page *p);

void swap_init(void)
{
    swap_device = block_get_role(BLOCK_SWAP);
    if (swap_device == NULL)
    {
        swap_bitmap = bitmap_create(0);
    }
    else
    {
        swap_bitmap = bitmap_create(block_size(swap_device) / PAGE_SECTORS);
    }

    if (swap_bitmap == NULL)
        PANIC("couldn't create swap bitmap");

    lock_init(&swap_lock);
}

void swap_in(struct page *p)
{
    ASSERT(p->frame != NULL);
    ASSERT(lock_held_by_current_thread(&p->frame->lock));
    ASSERT(p->sector != (block_sector_t)-1);

    read_from_swap(p);
    bitmap_reset(swap_bitmap, p->sector / PAGE_SECTORS);
    p->sector = (block_sector_t)-1;
}

bool swap_out(struct page *p)
{
    ASSERT(p->frame != NULL);
    ASSERT(lock_held_by_current_thread(&p->frame->lock));

    lock_acquire(&swap_lock);
    bool success = write_to_swap(p);
    lock_release(&swap_lock);

    if (success)
    {
        p->private = false;
        p->file = NULL;
        p->file_offset = 0;
        p->file_bytes = 0;
    }

    return success;
}

static void read_from_swap(struct page *p)
{
    size_t i;
    for (i = 0; i < PAGE_SECTORS; i++)
    {
        block_read(swap_device, p->sector + i, p->frame->base + i * BLOCK_SECTOR_SIZE);
    }
}

static bool write_to_swap(struct page *p)
{
    size_t slot, i;

    slot = bitmap_scan_and_flip(swap_bitmap, 0, 1, false);
    if (slot == BITMAP_ERROR)
        return false;

    p->sector = slot * PAGE_SECTORS;

    for (i = 0; i < PAGE_SECTORS; i++)
    {
        block_write(swap_device, p->sector + i, (uint8_t *)p->frame->base + i * BLOCK_SECTOR_SIZE);
    }

    return true;
}
