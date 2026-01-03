#ifndef THREADS_PROCON_H
#define THREADS_PROCON_H

#include "threads/synch.h"
#include <stdint.h>

/* State for producer-consumer mechanism */
struct procon
  { int size;
    char* buffer;
int count , pro_index , con_index;
struct lock l;
struct condition data_available;
struct condition space_available;
  };

void procon_init (struct procon *, unsigned buffer_size);
void procon_produce (struct procon *, char c);
char procon_consume (struct procon *);

#endif /* threads/procon.h */
