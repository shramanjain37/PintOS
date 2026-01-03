#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "devices/block.h"
#include "devices/shutdown.h"
#include "userprog/pagedir.h"
#include <stdlib.h>
#include "lib/stdio.h"

/////

const int MIN_FILENAME = 1;
const int MAX_FILENAME = 14;

typedef int pid_t;

struct file_desc
{
  int fd;
  struct file *file;
  struct list_elem elem;

};

static struct lock filesys_lock; 

bool is_valid_ptr(const void *ptr);
bool is_valid_filename(const void *file);

static int open(const char *file);
static void close(int fd);

static bool create(const char *file, unsigned initial_size);
static bool remove(const char *file);

static void halt(void);

static pid_t exec(const char *cmd_line);
static int wait(pid_t pid);

static filesize(int fd);
static int read(int fd, void *buffer, unsigned size);
static int write(int fd, const void *buffer, unsigned size);

static void seek(int fd, unsigned position);
static unsigned tell(int fd);

////

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");

  ///
  lock_init(&filesys_lock);
  ///
}

/////

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
    exit(-1);
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


bool 
is_valid_filename(const void *file)
{
  if (!is_valid_ptr(file)) 
    exit(-1);

  int len = strlen(file);
  return len >= MIN_FILENAME && len <= MAX_FILENAME;
}


int 
assign_fd() 
{
  struct list *list = &thread_current()->open_fd;
  if (list_empty(list)) 
    return 2; 
  else
  {
    struct file_desc *f = 
        list_entry(list_back(list), struct file_desc, elem);
    
    return f->fd + 1;
  }
}


bool 
cmp_fd(const struct list_elem *a, const struct list_elem *b, void *aux)
{
  struct file_desc *left = list_entry(a, struct file_desc, elem);
  struct file_desc *right = list_entry(b, struct file_desc, elem);
  return left->fd < right->fd;
}



struct file_desc *
get_openfile(int fd)
{
  struct list *list = &thread_current()->open_fd;
  for (struct list_elem *e = list_begin (list); 
                          e != list_end (list); 
                          e = list_next (e))
  {
    struct file_desc *f = 
        list_entry(e, struct file_desc, elem);
    if (f->fd == fd)
      return f;
    else if (f->fd > fd)
      return NULL;
  }
  return NULL;
}

void 
close_openfile(int fd)
{
  struct list *list = &thread_current()->open_fd;
  for (struct list_elem *e = list_begin (list); 
                          e != list_end (list); 
                          e = list_next (e))
  {
    struct file_desc *f = 
        list_entry(e, struct file_desc, elem);
    if (f->fd == fd)
    { 
      list_remove(e);
      file_close(f->file);
      free(f);
      return;
    }
    else if (f->fd > fd)
      return ;
  }
  return ;
}

static int 
open(const char *file)
{
  int fd = -1;

  if (!is_valid_filename(file))
    return fd;

  lock_acquire(&filesys_lock);
  struct list *list = &thread_current()->open_fd;
  struct file *file_struct = filesys_open(file);
  if (file_struct != NULL) 
  {
    struct file_desc *tmp = malloc(sizeof(struct file_desc));    
    tmp->fd = assign_fd();
    tmp->file = file_struct;
    fd = tmp->fd;
    list_insert_ordered(list, &tmp->elem, (list_less_func *)cmp_fd, NULL);
  }
  lock_release(&filesys_lock);

  return fd;
}

static void 
close(int fd)
{
  lock_acquire(&filesys_lock);
  close_openfile(fd);
  lock_release(&filesys_lock);

}

static bool 
create(const char *file, unsigned initial_size)
{
  if (!is_valid_filename(file))
    return false;

  lock_acquire(&filesys_lock);

  block_sector_t inode_sector = 0;
  struct dir *dir = dir_open_root ();
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size)
                  && dir_add (dir, file, inode_sector));
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  dir_close (dir);

  lock_release(&filesys_lock);

  return success;
}


static bool 
remove(const char *file)
{
  if (!is_valid_filename(file))
    return false;

  bool status;

  lock_acquire(&filesys_lock);
  status = filesys_remove(file);
  lock_release(&filesys_lock);

  return status;
}

static void 
halt(void) 
{
  shutdown_power_off();
}


static int 
wait(pid_t pid)
{
  return process_wait(pid);
}

void 
exit(int status)
{
  struct thread *cur = thread_current();

  printf("%s: exit(%d)\n", cur->name, status);

   
  if (cur->parent != NULL)
  {
    cur->parent->child_exit_status = status;
  }

  while (!list_empty(&cur->open_fd)) 
  {
    close(list_entry(list_begin(&cur->open_fd), struct file_desc, elem)->fd);  
  }

  file_close(cur->file);
 

  thread_exit();
}

static pid_t 
exec(const char *cmd_line)
{  

  if (!is_valid_ptr(cmd_line))
    exit(-1);

  lock_acquire(&filesys_lock);
  tid_t tid = process_execute(cmd_line);
  lock_release(&filesys_lock);
  
  return tid;
}





static int
filesize(int fd)
{
  int size = -1;

  lock_acquire(&filesys_lock);

  struct file_desc *file_desc = get_openfile(fd);
    if (file_desc != NULL)
      size = file_length(file_desc->file);

  lock_release(&filesys_lock);
  
  return size;
}

static int 
read(int fd, void *buffer, unsigned size)
{
  int status = -1;

  if (!is_valid_ptr(buffer) || !is_valid_ptr(buffer + size - 1)) 
    exit(-1);

  lock_acquire(&filesys_lock);
  if (fd == STDIN_FILENO) 
  {
    uint8_t *p = buffer;
    uint8_t c;
    unsigned counter = 0;
    while (counter < size && (c = input_getc()) != 0)
    {
      *p = c;
      p++;
      counter++;
    }
    *p = 0;
    status = size - counter;

  } else if (fd != STDOUT_FILENO)
  { 
    struct file_desc *file_desc = get_openfile(fd);
    if (file_desc != NULL)
      status = file_read(file_desc->file, buffer, size);
  }

  lock_release(&filesys_lock);

  return status;
}

static int
write(int fd, const void *buffer, unsigned size) 
{
  int status = 0;

  if (buffer == NULL || !is_valid_ptr(buffer) || !is_valid_ptr(buffer + size - 1)) 
    exit(-1);

  lock_acquire(&filesys_lock);
	if (fd == STDOUT_FILENO) 
	{
		putbuf(buffer, size);
		status = size;
	} else if (fd != STDIN_FILENO) 
  {
    struct file_desc *file_desc = get_openfile(fd);
    if (file_desc != NULL)
      status = file_write(file_desc->file, buffer, size);
  }

  lock_release(&filesys_lock);

  return status;
}


static void 
seek(int fd, unsigned position)
{
  lock_acquire(&filesys_lock);
  struct file_desc *file_desc = get_openfile(fd);
    if (file_desc != NULL)
      file_seek(file_desc->file, position);
  lock_release(&filesys_lock);

  return ;
}


static unsigned 
tell(int fd)
{
  int status = -1;

  lock_acquire(&filesys_lock);

  struct file_desc *file_desc = get_openfile(fd);
    if (file_desc != NULL)
      status = file_tell(file_desc->file);

  lock_release(&filesys_lock);

  return status;
}
////////////

