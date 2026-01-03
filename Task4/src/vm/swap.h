#ifndef VM_SWAP_H
#define VM_SWAP_H 1

#include <stdbool.h>
struct page;
void swap_init (void);
void swap_out (struct page *);
void swap_in (void *, void *);

#endif 
