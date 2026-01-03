#include "/pintos/src/threads/procon.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"

/* Initialize producer-consumer instance. */
void
procon_init (struct procon *pc, unsigned int buffer_size)
{ pc->size=buffer_size=10;
   pc->buffer = (char*)malloc(sizeof(char)*buffer_size);
pc->count=0;
pc->con_index=0;
pc->pro_index=0;
lock_init(&pc->l);
cond_init(&pc->data_available);
cond_init(&pc->space_available);
  
}

/* Put a character into the bounded buffer. Wait if the buffer is full. */
void
procon_produce (struct procon *pc, char c)
{
  lock_acquire(&pc->l);
    while (pc->count == pc->size) {
        cond_wait(&pc->space_available, &pc->l);
    }
    pc->count++;
    pc->buffer[pc->pro_index] = c;
    pc->pro_index++;
    if (pc->pro_index == pc->size) {
        pc->pro_index = 0;
    }
    cond_signal(&pc->data_available, &pc->l);
    lock_release(&pc->l);

}

/* Pull a character out of the buffer. Wait if the buffer is empty. */
char
procon_consume (struct procon *pc)
{
   char c;
    lock_acquire(&pc->l);
    while (pc->count == 0) {
        cond_wait(&pc->data_available, &pc->l);
    }
    pc->count--;
    c = pc->buffer[pc->con_index];
    pc->con_index++;
    if (pc->con_index == pc->size) {
        pc->con_index = 0;
    }
    cond_signal(&pc->space_available, &pc->l);
    lock_release(&pc->l);
    return c;
}
