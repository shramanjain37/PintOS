/* Tests producer/consumer communication with different numbers of threads.
 * Automatic checks only catch severe problems like crashes.
 */

#include "devices/timer.h"
#include "tests/threads/tests.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include <stdio.h>

#include "threads/procon.h"

#define BUFFER_SIZE 10
#define STRING "Hello world"
static struct lock index_lock;
static unsigned index = 0;

#define STRING_LEN 11
#define NUM_STRING 5
#define NUM_PRODUCER (STRING_LEN * NUM_STRING)

#define NUM_CONSUMER NUM_PRODUCER

static struct semaphore *func_ctr_sema;

static void
producer (void *arg)
{
  lock_acquire (&index_lock);
  procon_produce ((struct procon *)arg, STRING[(index++) % STRING_LEN]);
  lock_release (&index_lock);
  sema_up (func_ctr_sema);
}

static void
consumer (void *arg)
{
  printf ("%c", procon_consume ((struct procon *)arg));
  sema_up (func_ctr_sema);
}

void
test_procon_multiple (void)
{
  func_ctr_sema = malloc (sizeof (struct semaphore));
  if (func_ctr_sema == NULL)
    {
      printf ("malloc producer counter sema failed.\n");
      return;
    }
  sema_init (func_ctr_sema, 0);

  lock_init (&index_lock);
  // Init lock and condition variables
  struct procon *pc = malloc (sizeof (struct procon));

  procon_init (pc, BUFFER_SIZE);

  // Create producers and consumers
  for (unsigned i = 0; i < NUM_PRODUCER; i++)
    {
      char name[16];
      snprintf (name, sizeof (name), "producer %u\n", i);
      thread_create (name, PRI_DEFAULT, producer, (void *)pc);
    }

  for (unsigned i = 0; i < NUM_CONSUMER; i++)
    {
      char name[16];
      snprintf (name, sizeof (name), "consumer %u\n", i);
      thread_create (name, PRI_DEFAULT, consumer, (void *)pc);
    }

  // Wait until functions finish.
  for (int i = 0; i < NUM_PRODUCER + NUM_CONSUMER; i++)
    {
      sema_down (func_ctr_sema);
    }

  // Don't change the following lines otherwise tests will fail.
  printf ("\n");
}
