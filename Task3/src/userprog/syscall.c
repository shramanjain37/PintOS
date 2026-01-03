#include "userprog/syscall.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include "devices/input.h"
#include "devices/shutdown.h"
#include "filesys/directory.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "vm/page.h"




static int open (const char *ufile);
static int close (int handle);

static int create (const char *ufile, unsigned initial_size);
static int remove (const char *ufile);

static int halt (void);

static int exec (const char *ufile);
static int wait (tid_t);

static int seek (int handle, unsigned position);
static int tell (int handle);


static int read (int handle, void *udst_, unsigned size);
static int write (int handle, void *usrc_, unsigned size);


static int exit (int status);

static int filesize (int handle);
static void syscall_handler (struct intr_frame *);
static void copy_in (void *, const void *, size_t);
bool is_valid_ptr(const void *ptr);
static struct lock filesys_lock;

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init (&filesys_lock);
}


static void
syscall_handler (struct intr_frame *f) 
{
 
  uint32_t *esp = f->esp;
  uint32_t *argv0 = esp + 1;
  uint32_t *argv1 = esp + 2;
  uint32_t *argv2 = esp + 3;

  if (!is_valid_ptr(esp) || !is_valid_ptr(argv0) 
    || !is_valid_ptr(argv1) || !is_valid_ptr(argv2)) 
  {
    thread_exit();
  }

  uint32_t syscall_num = *esp;

  switch (syscall_num) 
  {
  	case SYS_HALT:
      halt();
  		break;
  	case SYS_EXIT:
      exit(*argv0);
  		break;
  	case SYS_EXEC:
      f->eax = exec((char *)*argv0);
  		break;
  	case SYS_WAIT:
      f->eax = wait(*argv0);
  		break;
  	case SYS_CREATE:
      f->eax = create((char *)*argv0, *argv1);
  		break;
  	case SYS_REMOVE:
      f->eax = remove((char *)*argv0);
  		break;
  	case SYS_OPEN:
      f->eax = open((char *)*argv0);
  		break;
  	case SYS_FILESIZE:
      f->eax = filesize(*argv0);
  		break;
  	case SYS_READ:
      f->eax = read(*argv0, (void *)*argv1, *argv2);
  		break;
  	case SYS_WRITE:
  		f->eax = write(*argv0, (void *)*argv1, *argv2);
  		break;
  	case SYS_SEEK:
      seek(*argv0, *argv1);
  		break;
  	case SYS_TELL:
      f->eax = tell(*argv0);
  		break;
  	case SYS_CLOSE:
      close(*argv0);
  		break; 
  	default:
  		break; 		  	
  }
}


bool 
is_valid_ptr(const void *ptr) 
{
  if (ptr == NULL 
    || !is_user_vaddr(ptr) 
    || pagedir_get_page(thread_current()->pagedir, ptr) == NULL)
    return false;

  return true;
}


static void
copy_in (void *dst_, const void *usrc_, size_t size)
{
  uint8_t *dst = dst_;
  const uint8_t *usrc = usrc_;

  while (size > 0)
    {
      size_t chunk_size = PGSIZE - pg_ofs (usrc);
      if (chunk_size > size)
        chunk_size = size;

      if (!page_lock (usrc, false))
        thread_exit ();
      memcpy (dst, usrc, chunk_size);
      page_unlock (usrc);

      dst += chunk_size;
      usrc += chunk_size;
      size -= chunk_size;
    }
}


static char *
copy_in_string (const char *us)
{
  char *ks;
  char *upage;
  size_t length;

  ks = palloc_get_page (0);
  if (ks == NULL)
    thread_exit ();

  length = 0;
  for (;;)
    {
      upage = pg_round_down (us);
      if (!page_lock (upage, false))
        goto lock_error;

      for (; us < upage + PGSIZE; us++)
        {
          ks[length++] = *us;
          if (*us == '\0')
            {
              page_unlock (upage);
              return ks;
            }
          else if (length >= PGSIZE)
            goto too_long_error;
        }

      page_unlock (upage);
    }

 too_long_error:
  page_unlock (upage);
 lock_error:
  palloc_free_page (ks);
  thread_exit ();
}


static int
halt (void)
{
  shutdown_power_off ();
}



static int
exec (const char *ufile)
{
  tid_t tid;
  char *kfile = copy_in_string (ufile);

  lock_acquire (&filesys_lock);
  tid = process_execute (kfile);
  lock_release (&filesys_lock);

  palloc_free_page (kfile);

  return tid;
}

static int
wait (tid_t child)
{
  return process_wait (child);
}

static int
create (const char *ufile, unsigned initial_size)
{
  char *kfile = copy_in_string (ufile);
  bool ok;

  lock_acquire (&filesys_lock);
  ok = filesys_create (kfile, initial_size);
  lock_release (&filesys_lock);

  palloc_free_page (kfile);

  return ok;
}

static int
remove (const char *ufile)
{
  char *kfile = copy_in_string (ufile);
  bool ok;

  lock_acquire (&filesys_lock);
  ok = filesys_remove (kfile);
  lock_release (&filesys_lock);

  palloc_free_page (kfile);

  return ok;
}

struct file_descriptor
  {
    struct list_elem elem;      
    struct file *file;          
    int handle;                 
  };

  static struct file_descriptor *
lookup_fd (int handle)
{
  struct thread *cur = thread_current ();
  struct list_elem *e;

  for (e = list_begin (&cur->open_fd); e != list_end (&cur->open_fd);
       e = list_next (e))
    {
      struct file_descriptor *fd;
      fd = list_entry (e, struct file_descriptor, elem);
      if (fd->handle == handle)
        return fd;
    }

  thread_exit ();
}


static int
open (const char *ufile)
{
  char *kfile = copy_in_string (ufile);
  struct file_descriptor *fd;
  int handle = -1;

  fd = malloc (sizeof *fd);
  if (fd != NULL)
    {
      lock_acquire (&filesys_lock);
      fd->file = filesys_open (kfile);
      if (fd->file != NULL)
        {
          struct thread *cur = thread_current ();
          handle = fd->handle = cur->next_handle++;
          list_push_front (&cur->open_fd, &fd->elem);
        }
      else
        free (fd);
      lock_release (&filesys_lock);
    }

  palloc_free_page (kfile);
  return handle;
}

static int
close (int handle)
{
  struct file_descriptor *fd = lookup_fd (handle);
  lock_acquire (&filesys_lock);
  file_close (fd->file);
  lock_release (&filesys_lock);
  list_remove (&fd->elem);
  free (fd);
  return 0;
}


static int
filesize (int handle)
{
  struct file_descriptor *fd = lookup_fd (handle);
  int size;

  lock_acquire (&filesys_lock);
  size = file_length (fd->file);
  lock_release (&filesys_lock);

  return size;
}

static int
read (int handle, void *udst_, unsigned size)
{
  uint8_t *udst = udst_;
  struct file_descriptor *fd;
  int bytes_read = 0;

  fd = lookup_fd (handle);
  while (size > 0)
    {
      size_t page_left = PGSIZE - pg_ofs (udst);
      size_t read_amt = size < page_left ? size : page_left;
      off_t retval;

      if (handle != STDIN_FILENO)
        {
          if (!page_lock (udst, true))
            thread_exit ();
          lock_acquire (&filesys_lock);
          retval = file_read (fd->file, udst, read_amt);
          lock_release (&filesys_lock);
          page_unlock (udst);
        }
      else
        {
          size_t i;

          for (i = 0; i < read_amt; i++)
            {
              char c = input_getc ();
              if (!page_lock (udst, true))
                thread_exit ();
              udst[i] = c;
              page_unlock (udst);
            }
          bytes_read = read_amt;
        }

      if (retval < 0)
        {
          if (bytes_read == 0)
            bytes_read = -1;
          break;
        }
      bytes_read += retval;
      if (retval != (off_t) read_amt)
        {
          break;
        }

      udst += retval;
      size -= retval;
    }

  return bytes_read;
}

static int
write (int handle, void *usrc_, unsigned size)
{
  uint8_t *usrc = usrc_;
  struct file_descriptor *fd = NULL;
  int bytes_written = 0;

  if (handle != STDOUT_FILENO)
    fd = lookup_fd (handle);

  while (size > 0)
    {
      size_t page_left = PGSIZE - pg_ofs (usrc);
      size_t write_amt = size < page_left ? size : page_left;
      off_t retval;

      if (!page_lock (usrc, false))
        thread_exit ();
      lock_acquire (&filesys_lock);
      if (handle == STDOUT_FILENO)
        {
          putbuf ((char *) usrc, write_amt);
          retval = write_amt;
        }
      else
        retval = file_write (fd->file, usrc, write_amt);
      lock_release (&filesys_lock);
      page_unlock (usrc);

      if (retval < 0)
        {
          if (bytes_written == 0)
            bytes_written = -1;
          break;
        }
      bytes_written += retval;

      if (retval != (off_t) write_amt)
        break;

      usrc += retval;
      size -= retval;
    }

  return bytes_written;
}

static int
seek (int handle, unsigned position)
{
  struct file_descriptor *fd = lookup_fd (handle);

  lock_acquire (&filesys_lock);
  if ((off_t) position >= 0)
    file_seek (fd->file, position);
  lock_release (&filesys_lock);

  return 0;
}

static int
tell (int handle)
{
  struct file_descriptor *fd = lookup_fd (handle);
  unsigned position;

  lock_acquire (&filesys_lock);
  position = file_tell (fd->file);
  lock_release (&filesys_lock);

  return position;
}



static int
exit (int exit_status)
{
  thread_current ()->exit_status = exit_status;
  thread_exit ();
  NOT_REACHED ();
}