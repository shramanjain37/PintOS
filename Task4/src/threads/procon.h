#ifndef THREADS_PROCON_H
#define THREADS_PROCON_H

#include "threads/synch.h"
#include <stdint.h>

/* State for producer-consumer mechanism */
struct procon
  {
    // FILL ME IN
  };

void procon_init (struct procon *, unsigned buffer_size);
void procon_produce (struct procon *, char c);
char procon_consume (struct procon *);

#endif /* threads/procon.h */
