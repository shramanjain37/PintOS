#include "vm/frame.h"
#include <stdio.h>
#include "vm/page.h"
#include "devices/timer.h"
#include "threads/init.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "vm/swap.h"
#include "threads/thread.h"
static struct list all_frames;
static struct frame *frames;
static size_t frame_cnt;
static struct lock scan_lock;
static size_t hand;
static struct lock frame_alloc_lock;
static void frame_allocate_initial(void);
static struct frame *find_free_frame(struct page *page);
static struct frame *find_eviction_frame(struct page *page);
static struct frame *allocate_frame(struct page *page);
static bool is_frame_free(struct frame *f);
static bool try_lock_frame(struct frame *f);
static void release_frame_lock(struct frame *f);
static bool evict_frame(struct frame *f, struct page *page);
static frameTable ft;

static struct lock frame_free_lock;

static unsigned
ft_hash (const struct hash_elem *element, void *aux UNUSED)
{
  struct frameTableEntry *fte = hash_entry (element, struct frameTableEntry, hash_elem);
  return hash_bytes (&fte->frame, sizeof (void*));
}

static bool
ft_less (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
  struct frameTableEntry *fte_a = hash_entry (a, struct frameTableEntry, hash_elem);
  struct frameTableEntry *fte_b = hash_entry (b, struct frameTableEntry, hash_elem);
  
  return fte_a->frame < fte_b->frame;
}
// void frame_init(void) {
//     lock_init(&scan_lock);
//     frames = malloc(sizeof *frames * init_ram_pages);
//     if (frames == NULL)
//         PANIC("out of memory allocating page frames");

//     frame_allocate_initial();
// }

void
ft_init ()
{
  hash_init (&ft, ft_hash, ft_less, NULL);
  list_init (&all_frames);
  lock_init (&frame_alloc_lock);
  lock_init (&frame_free_lock);
}

static void frame_allocate_initial(void) {
    void *base;
    while ((base = palloc_get_page(PAL_USER)) != NULL) {
        struct frame *f = &frames[frame_cnt++];
        lock_init(&f->lock);
        f->base = base;
        f->page = NULL;
    }
}

static struct frame *find_free_frame(struct page *page) {
    for (size_t i = 0; i < frame_cnt; i++) {
        struct frame *f = &frames[i];
        if (try_lock_frame(f) && is_frame_free(f)) {
            f->page = page;
            return f;
        }
        release_frame_lock(f);
    }
    return NULL;
}

static struct frame *find_eviction_frame(struct page *page) {
    for (size_t i = 0; i < frame_cnt * 2; i++) {
        struct frame *f = &frames[hand];
        hand = (hand + 1) % frame_cnt;

        if (try_lock_frame(f)) {
            if (is_frame_free(f)) {
                f->page = page;
                return f;
            }
            if (!page_recently_accessed(f->page) && evict_frame(f, page)) {
                return f;
            }
            release_frame_lock(f);
        }
    }
    return NULL;
}

static bool is_frame_free(struct frame *f) {
    return f->page == NULL;
}

static bool try_lock_frame(struct frame *f) {
    return lock_try_acquire(&f->lock);
}

static void release_frame_lock(struct frame *f) {
    if (lock_held_by_current_thread(&f->lock))
        lock_release(&f->lock);
}

static bool evict_frame(struct frame *f, struct page *page) {
    if (!page_out(f->page)) {
        release_frame_lock(f);
        return false;
    }
    f->page = page;
    return true;
}

static struct frame *allocate_frame(struct page *page) {
    lock_acquire(&scan_lock);

    struct frame *f = find_free_frame(page);
    if (f == NULL) {
        f = find_eviction_frame(page);
    }

    lock_release(&scan_lock);
    return f;
}

struct frame *frame_alloc_and_lock(struct page *page) {
    for (size_t try = 0; try < 3; try++) {
        struct frame *f = allocate_frame(page);
        if (f != NULL) {
            ASSERT(lock_held_by_current_thread(&f->lock));
            return f;
        }
        timer_msleep(1000);
    }
    return NULL;
}

void frame_lock(struct page *p) {
    struct frame *f = p->frame;
    if (f != NULL) {
        lock_acquire(&f->lock);
        if (f != p->frame) {
            release_frame_lock(f);
            ASSERT(p->frame == NULL);
        }
    }
}

// void frame_free(struct frame *f) {
//     ASSERT(lock_held_by_current_thread(&f->lock));
//     f->page = NULL;
//     lock_release(&f->lock);
// }

void frame_unlock(struct frame *f) {
    ASSERT(lock_held_by_current_thread(&f->lock));
    lock_release(&f->lock);
}


static bool
ft_add_entry (void *frame, void *page, uint32_t *pagedir)
{
  struct frameTableEntry *fte;
  fte = malloc (sizeof (struct frameTableEntry));
  if (fte == NULL) return false;
  fte->frame = frame;
  fte->pagedir = pagedir;
  fte->page = page;

  if (hash_insert (&ft, &fte->hash_elem) == NULL) {
    list_push_back (&all_frames, &fte->list_elem);
    return true;
  }
  return false;
}

static void*
select_victim (void)
{
  ASSERT (!list_empty (&all_frames));

  struct list_elem *victim_elem = list_pop_front (&all_frames);
  list_push_back (&all_frames, victim_elem);
  
  struct frameTableEntry *victim = list_entry (victim_elem, struct frameTableEntry, list_elem);
  return victim;
}
void*
frame_alloc (enum palloc_flags flags, void *page)
{
  lock_acquire (&frame_alloc_lock);
  void *frame = palloc_get_page (flags);

  if (frame == NULL) {
    struct frameTableEntry *victim = select_victim ();
    swap_out (victim->page);

    frame_free (victim->frame);
    frame = palloc_get_page (flags);
    ASSERT (frame != NULL);
  }
  ft_add_entry (frame, page, thread_current ()->pagedir);
  
  lock_release (&frame_alloc_lock);
  return frame;
}
static struct frameTableEntry*
ft_get_entry (void *frame)
{
  struct frameTableEntry fte;
  struct hash_elem *fte_elem;

  fte.frame = frame;
  fte_elem = hash_find (&ft, &fte.hash_elem);

  return hash_entry (fte_elem, struct frameTableEntry, hash_elem);
}

static bool
ft_delete_entry (void *frame)
{
  struct frameTableEntry *fte = ft_get_entry (frame);
  if (hash_delete (&ft, &fte->hash_elem) == NULL
	|| list_remove (&fte->list_elem) == NULL)
    return false;
  return true;
}
void
frame_free (void *frame)
{
  lock_acquire (&frame_free_lock);

  struct frameTableEntry *fte = ft_get_entry (frame);
  pagedir_clear_page (fte->pagedir, fte->page);
  palloc_free_page (frame);
  ft_delete_entry (frame);

  lock_release (&frame_free_lock);
}