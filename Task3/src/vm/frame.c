#include "vm/frame.h"
#include <stdio.h>
#include "vm/page.h"
#include "devices/timer.h"
#include "threads/init.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"

static struct frame *frames;
static size_t frame_cnt;
static struct lock scan_lock;
static size_t hand;

static void frame_allocate_initial(void);
static struct frame *find_free_frame(struct page *page);
static struct frame *find_eviction_frame(struct page *page);
static struct frame *allocate_frame(struct page *page);
static bool is_frame_free(struct frame *f);
static bool try_lock_frame(struct frame *f);
static void release_frame_lock(struct frame *f);
static bool evict_frame(struct frame *f, struct page *page);

void frame_init(void) {
    lock_init(&scan_lock);
    frames = malloc(sizeof *frames * init_ram_pages);
    if (frames == NULL)
        PANIC("out of memory allocating page frames");

    frame_allocate_initial();
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

void frame_free(struct frame *f) {
    ASSERT(lock_held_by_current_thread(&f->lock));
    f->page = NULL;
    lock_release(&f->lock);
}

void frame_unlock(struct frame *f) {
    ASSERT(lock_held_by_current_thread(&f->lock));
    lock_release(&f->lock);
}
