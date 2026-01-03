#include "threads/procon.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"

/* Initialize producer-consumer instance. */
void
procon_init (struct procon *pc, unsigned int buffer_size)
{
  // FILL ME IN and remove
  PANIC ("not yet implemented\n");
}

/* Put a character into the bounded buffer. Wait if the buffer is full. */
void
procon_produce (struct procon *pc, char c)
{
  // FILL ME IN and remove
  PANIC ("not yet implemented\n");
}

/* Pull a character out of the buffer. Wait if the buffer is empty. */
char
procon_consume (struct procon *pc)
{
  // FILL ME IN and remove
  PANIC ("not yet implemented\n");
}
