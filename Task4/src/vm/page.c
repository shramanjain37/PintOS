#include "vm/page.h"
#include <stdio.h>
#include <string.h>
#include "vm/frame.h"
#include "vm/swap.h"
#include "filesys/file.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"

#define MAX_STACK (1024 * 1024)
static struct page *page_lookup(const void *address);
static bool is_stack_access(const void *address);
static struct page *page_for_addr(const void *address);
static bool load_page_from_file(struct page *p);
static bool load_page(struct page *p);
static bool write_back_page_to_file(struct page *p);
static void destroy_page(struct hash_elem *p_, void *aux UNUSED);



unsigned page_hash(const struct hash_elem *e, void *aux UNUSED) {
    const struct page *p = hash_entry(e, struct page, hash_elem);
    return ((uintptr_t)p->addr) >> PGBITS;
}

bool page_less(const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED) {
    const struct page *a = hash_entry(a_, struct page, hash_elem);
    const struct page *b = hash_entry(b_, struct page, hash_elem);
    return a->addr < b->addr;
}

struct page *page_lookup(const void *address) {
    struct page p;
    struct hash_elem *e;

    p.addr = (void *)pg_round_down(address);
    e = hash_find(thread_current()->pages, &p.hash_elem);
    return e != NULL ? hash_entry(e, struct page, hash_elem) : NULL;
}

bool is_stack_access(const void *address) {
    return (uint8_t *)address >= (uint8_t *)PHYS_BASE - MAX_STACK &&
           address >= (void *)thread_current()->user_esp - 32;
}

struct page *page_for_addr(const void *address) {
    if (address < PHYS_BASE) {
        struct page *p = page_lookup(address);
        if (p) return p;

        if (is_stack_access(address)) {
            return page_allocate((void *)pg_round_down(address), false);
        }
    }
    return NULL;
}

bool load_page_from_file(struct page *p) {
    off_t read_bytes = file_read_at(p->file, p->frame->base, p->file_bytes, p->file_offset);
    off_t zero_bytes = PGSIZE - read_bytes;
    memset(p->frame->base + read_bytes, 0, zero_bytes);
    return true;
}

bool load_page(struct page *p) {
    p->frame = frame_alloc_and_lock(p);
    if (p->frame == NULL) return false;

    if (p->sector != (block_sector_t)-1) {
        swap_in(p,p->frame);
    } else if (p->file != NULL) {
        return load_page_from_file(p);
    } else {
        memset(p->frame->base, 0, PGSIZE);
    }
    return true;
}

bool page_in(void *fault_addr) {
    struct page *p;
    if (thread_current()->pages == NULL) return false;

    p = page_for_addr(fault_addr);
    if (p == NULL) return false;

    frame_lock(p);
    if (p->frame == NULL && !load_page(p)) {
        frame_unlock(p->frame);
        return false;
    }

    ASSERT(lock_held_by_current_thread(&p->frame->lock));
    bool success = pagedir_set_page(thread_current()->pagedir, p->addr, p->frame->base, !p->read_only);
    frame_unlock(p->frame);
    return success;
}

bool write_back_page_to_file(struct page *p) {
    if (pagedir_is_dirty(p->thread->pagedir, p->addr)) {
        // if (p->private) {
        //     return swap_out(p);
        // } else {
            return file_write_at(p->file, p->frame->base, p->file_bytes, p->file_offset) == p->file_bytes;
        //}
    }
    return true;
}

bool page_out(struct page *p) {
    ASSERT(p->frame != NULL);
    ASSERT(lock_held_by_current_thread(&p->frame->lock));

    pagedir_clear_page(p->thread->pagedir, p->addr);
    //bool success = p->file == NULL ? swap_out(p) : write_back_page_to_file(p);

    //if (success) p->frame = NULL;
    return 0;
}

bool page_recently_accessed(struct page *p) {
    ASSERT(p->frame != NULL);
    ASSERT(lock_held_by_current_thread(&p->frame->lock));

    bool was_accessed = pagedir_is_accessed(p->thread->pagedir, p->addr);
    if (was_accessed) pagedir_set_accessed(p->thread->pagedir, p->addr, false);
    return was_accessed;
}

struct page *page_allocate(void *vaddr, bool read_only) {
    struct thread *t = thread_current();
    struct page *p = malloc(sizeof *p);
    if (p == NULL) return NULL;

    p->addr = pg_round_down(vaddr);
    p->read_only = read_only;
    p->private = !read_only;
    p->frame = NULL;
    p->sector = (block_sector_t)-1;
    p->file = NULL;
    p->file_offset = 0;
    p->file_bytes = 0;
    p->thread = t;

    if (hash_insert(t->pages, &p->hash_elem) != NULL) {
        free(p);
        return NULL;
    }
    return p;
}

void page_deallocate(void *vaddr) {
    struct page *p = page_for_addr(vaddr);
    ASSERT(p != NULL);

    frame_lock(p);
    if (p->frame) {
        if (p->file && !p->private) page_out(p);
        frame_free(p->frame);
    }
    hash_delete(thread_current()->pages, &p->hash_elem);
    free(p);
}

bool page_lock(const void *addr, bool will_write) {
    struct page *p = page_for_addr(addr);
    if (p == NULL || (p->read_only && will_write)) return false;

    frame_lock(p);
    if (p->frame == NULL) {
        return load_page(p) && pagedir_set_page(thread_current()->pagedir, p->addr, p->frame->base, !p->read_only);
    }
    return true;
}

void page_unlock(const void *addr) {
    struct page *p = page_for_addr(addr);
    ASSERT(p != NULL);
    frame_unlock(p->frame);
}

void destroy_page(struct hash_elem *p_, void *aux UNUSED) {
    struct page *p = hash_entry(p_, struct page, hash_elem);
    frame_lock(p);
    if (p->frame) frame_free(p->frame);
    free(p);
}

void page_exit(void) {
    struct hash *h = thread_current()->pages;
    if (h != NULL) hash_destroy(h, destroy_page);
}


static unsigned
spt_hash (const struct hash_elem *element, void *aux UNUSED)
{
  struct sptEntry *spte = hash_entry (element, struct sptEntry, hash_elem);
  return hash_bytes (&spte->page, sizeof (void *));
}

static bool
spt_less (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
  struct sptEntry *spte_a = hash_entry (a, struct sptEntry, hash_elem);
  struct sptEntry *spte_b = hash_entry (b, struct sptEntry, hash_elem);
  
  return spte_a->page < spte_b->page;
}

SupPageTable*
spt_create ()
{
  SupPageTable *spt = malloc (sizeof (SupPageTable));
  if (spt == NULL)
    PANIC ("not enough memory for spt");
  hash_init (spt, spt_hash, spt_less, NULL);
  return spt;
}

bool
spt_add_installed (SupPageTable *spt, void *page, void *frame)
{
  struct sptEntry *new;
  new = malloc (sizeof (struct sptEntry));
  new->page = page;
  new->frame = frame;
  new->status = INSTALLED;
  new->dirty = false;
  new->writable = true;

  return hash_insert (spt, &new->hash_elem) == NULL;
}

bool
spt_add_filesys (SupPageTable *spt, void *page, struct file *file,
		 off_t offset, uint32_t read_bytes, uint32_t zero_bytes,
		 bool writable)
{
  struct sptEntry *new;
  new = malloc (sizeof (struct sptEntry));
  new->page = page;
  new->file = file;
  new->ofs = offset;
  new->read_bytes = read_bytes;
  new->zero_bytes = zero_bytes;
  new->writable = writable;
  new->status = FSYS;
  new->dirty = false;

  return hash_insert (spt, &new->hash_elem) == NULL;
}
struct sptEntry*
spt_get_entry (SupPageTable *spt, void *page)
{
  struct sptEntry tmp;
  struct hash_elem *tmp_elem;

  tmp.page = page;
  tmp_elem = hash_find (spt, &tmp.hash_elem);
  if (tmp_elem == NULL) return NULL;
  return hash_entry (tmp_elem, struct sptEntry, hash_elem);
}

bool
spt_set_swapped (SupPageTable *spt, void *page, block_sector_t block_idx)
{
  struct sptEntry *target;
  target = spt_get_entry (spt, page);
  
  if (target == NULL) return false;
  target->frame = NULL;
  target->status = SWAPPED;
  target->block_idx = block_idx;
  return true;
}
bool
spt_add_allzero (SupPageTable *spt, void *page, void *frame)
{
  struct sptEntry *new;
  new = malloc (sizeof (struct sptEntry));
  new->page = page;
  new->frame = frame;
  new->status = ALLZERO;
  new->dirty = false;
  new->writable = true;

  return hash_insert (spt, &new->hash_elem) == NULL;
}

bool
spt_delete_entry (SupPageTable *spt, void *page)
{
  struct sptEntry del;
  struct hash_elem *del_elem;
  
  del.page = page;
  del_elem = hash_find (spt, &del.hash_elem);
  return hash_delete (spt, del_elem) != NULL;
}