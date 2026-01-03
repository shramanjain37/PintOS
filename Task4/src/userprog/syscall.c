#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "filesys/inode.h"
#include <stdio.h>
#include <stdlib.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include <stdbool.h>
#include "devices/shutdown.h"
#include "devices/input.h"
#include "devices/block.h"
#include "filesys/directory.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "lib/string.h"

#define USER_LOWER_BOUND 0x08048000

void syscall_init (void);

void halt (void);
void exit (int);
pid_t exec (const char *);
int wait (pid_t);
bool create (const char *, unsigned);
bool remove (const char *);
int open (const char *);
int filesize (int);
int read (int, void *, unsigned);
int write (int, const void *, unsigned);
void seek (int, unsigned);
unsigned tell (int);
void close (int);
#ifdef FILESYS
bool chdir (const char *);
bool mkdir (const char *);
bool readdir (int, char *);
bool isdir (int);
int inumber (int);
#endif

struct fsys {
  bool is_dir;
  struct file *file;
  struct dir *dir;
};

static void syscall_handler (struct intr_frame *);
static struct lock filesys_lock;
static bool is_directory (struct inode* );

static void
is_valid_ptr (const void *ptr)
{
  if (!(is_user_vaddr (ptr) && ptr > (void *)USER_LOWER_BOUND)) {
    exit (-1);
  }
#ifndef VM
  if (pagedir_get_page (thread_current ()->pagedir, ptr) == NULL) {
    exit (-1);
  }
#endif
}

static void
is_valid_buf (const char *buf, unsigned size)
{
  for (unsigned i=0; i<size; i++) {
    is_valid_ptr (buf + i);
  }
}

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init (&filesys_lock);
}

static void
syscall_handler (struct intr_frame *f) 
{
  /* retrieve syscall number from intr_frame */
 is_valid_ptr (f->esp);
#ifdef VM
 thread_current ()->esp = (f->esp);
#endif
  int syscall_num = *((int *)(f->esp));
  uint32_t arg0, arg1, arg2;
  uint32_t *esp = f->esp;

  is_valid_ptr (f->esp + 4);
  is_valid_ptr (f->esp + 8);
  is_valid_ptr (f->esp + 12);
  arg0 = *(uint32_t *)(f->esp + 4);
  arg1 = *(uint32_t *)(f->esp + 8);
  arg2 = *(uint32_t *)(f->esp + 12);


 
  switch (syscall_num)
  {
    case SYS_HALT:
      halt ();
      break;

    case SYS_EXIT:
      exit ((int)arg0);
      (f->eax) = (int)arg0;
      break;

    case SYS_EXEC:

      (f->eax) = exec ((char *)arg0);
 
      break;

    case SYS_WAIT:

      (f->eax) = wait ((pid_t)arg0);
      break;

    case SYS_CREATE:

      (f->eax) = create ((char *)arg0, (unsigned)arg1);
  
      break;

    case SYS_REMOVE:

      (f->eax) = remove ((char *)arg0);
      break;

    case SYS_OPEN:
 
      (f->eax) = open ((char *)arg0);
      break;

    case SYS_FILESIZE:

      (f->eax) = filesize ((int)arg0);
      break;

    case SYS_READ:
   
      is_valid_buf ((char *)arg1, (unsigned)arg2);
      (f->eax) = read ((int)arg0, (void *)arg1, (unsigned)arg2);
      break;

    case SYS_WRITE:
   
      is_valid_buf ((char *)arg1, (unsigned)arg2);
      (f->eax) = write ((int)arg0, (void *)arg1, (unsigned)arg2);
      break;

    case SYS_SEEK:  
   
      seek ((int)arg0, (unsigned)arg1);
      break;

    case SYS_TELL:
    
      (f->eax) = tell ((int)arg0);
      break;

    case SYS_CLOSE:
     
      close ((int)arg0);
      break;

    case SYS_CHDIR:
 
     (f->eax) = chdir ((char *)arg0);
      break;

    case SYS_MKDIR:
    
      (f->eax) = mkdir ((char *)arg0);
      break;

    case SYS_READDIR:
    
      (f->eax) = readdir ((int)arg0, (char *)arg1);
      break;
      
    case SYS_ISDIR: 

      (f->eax) = isdir ((int)arg0);
      break;
      
    case SYS_INUMBER:
   
      (f->eax) = inumber ((int)arg0);
      break;
  }
}

void
halt (void)
{
  shutdown_power_off ();
}

void
exit (int status)
{
  struct thread *cur = thread_current ();

  /* wait until parent info is set */
  sema_down (&cur->parent_sema);

  tid_t parent_tid = cur->parent_tid;
  struct thread *parent_t = get_thread_from_tid (parent_tid);
  if (parent_t != NULL) {
    struct child *ch = NULL;
    struct list *child_list_of_par = &parent_t->child_list;
    struct list_elem *e;
  
    for (e = list_begin (child_list_of_par); e != list_end (child_list_of_par);
         e = list_next (e)) {
      ch = list_entry (e, struct child, elem);
      if (ch->child_tid == cur->tid)
        break;
    }

    ASSERT (ch->child_tid == cur->tid);

    ch->status = status;
    sema_up (&ch->sema);
  }

  thread_exit ();
}

pid_t
exec (const char *cmd_line)
{
  return process_execute (cmd_line);
}

int
wait (pid_t pid)
{
  return process_wait (pid);
}

bool
create (const char *file, unsigned initial_size)
{
  if (file == NULL) {
    exit (-1);
  }

  lock_acquire (&filesys_lock);
  bool success = filesys_create (file, initial_size);
  lock_release (&filesys_lock);
  return success;
}

bool
remove (const char *file)
{
  if (file == NULL) {
    exit (-1);
  }

  lock_acquire (&filesys_lock);
  bool success = filesys_remove (file);
  lock_release (&filesys_lock);
  return success;
}

int
open (const char *file)
{
  if (file == NULL) {
    return -1;
  }

  lock_acquire (&filesys_lock);
  struct inode *inode = filesys_open_path (file);
  struct fsys *opened_file = (struct fsys *)malloc(sizeof (struct fsys));

  if (inode == NULL) {
    lock_release (&filesys_lock);
    return -1;
  }
  if ((opened_file->is_dir = is_directory (inode)) == true) {
    opened_file->dir = dir_open (inode);
  }
  else {
    opened_file->file = file_open (inode);
  }
  lock_release (&filesys_lock);

  struct thread *t = thread_current ();
  int fd;

  if (opened_file == NULL) {
    return -1;
  }

  /* find empty entry in fd_table */
  for (fd = 2; fd < MAX_FD; fd++) {
    if (t->fd_table[fd] == NULL) break;
  }
  if (fd == MAX_FD) {
    /* fd_table is full */
    return -1;
  }
  else {
    t->fd_table[fd] = opened_file;
    return fd;
  }
}

int
filesize (int fd)
{
  if (fd >= MAX_FD || fd < 2) {
    return 0;
  }

  struct thread *t = thread_current ();
  struct fsys *opened_fsys = t->fd_table[fd];

  if (opened_fsys == NULL || opened_fsys->is_dir)
    return 0;

  struct file *opened_file = opened_fsys->file;
  int length;

  if (opened_file == NULL) {
    return 0;
  }

  lock_acquire (&filesys_lock);
  length = file_length (opened_file);
  lock_release (&filesys_lock);

  return length;
}

int
read (int fd, void *buffer, unsigned size)
{
  if (fd >= MAX_FD || fd < 0) {
    return 0;
  }

  struct file *file;
  struct thread *t = thread_current ();
  unsigned read_cnt = 0;

  lock_acquire (&filesys_lock);
  if (fd == 0) {
    while (read_cnt <= size) {
      /* read key by input_getc() and write it into buffer at appropriate position */
      *(char *)(buffer + read_cnt++) = input_getc ();
    }
    lock_release (&filesys_lock);
    return read_cnt;
  }
  if (fd == 1) {
    lock_release (&filesys_lock);
    return 0;
  }

  /* get file from fd */
  struct fsys *opened_fsys = t->fd_table[fd];

  if (opened_fsys == NULL) {
    lock_release (&filesys_lock);
    return 0;
  }
  if (opened_fsys->is_dir) {
    lock_release (&filesys_lock);
    return -1;
  }
  file = opened_fsys->file;

  if (file == NULL) {
    lock_release (&filesys_lock);
    return 0;
  }

  read_cnt = file_read (file, buffer, size);
  lock_release (&filesys_lock);
  return (int)read_cnt;
}

int
write (int fd, const void *buffer, unsigned size)
{
  if (fd >= MAX_FD || fd < 0) {
    return 0;
  }

  struct file *file;
  struct thread *t = thread_current ();
  int write_cnt = size;
  
  lock_acquire (&filesys_lock);
  if (fd == 1) {
    putbuf (buffer, size);
    lock_release (&filesys_lock);
    return write_cnt;
  }
  if (fd == 0) {
    lock_release (&filesys_lock);
    return 0;
  }

  /* get file from fd */
  struct fsys *opened_fsys = t->fd_table[fd];
  if (opened_fsys == NULL) {
    lock_release (&filesys_lock);
    return 0;
  }
  if (opened_fsys->is_dir) {
    lock_release (&filesys_lock);
    return -1;
  }
  file = opened_fsys->file;

  if (file == NULL) {
    lock_release (&filesys_lock);
    return 0;
  }

  write_cnt = file_write (file, buffer, size);
  lock_release (&filesys_lock);
  return write_cnt;
}

void
seek (int fd, unsigned position)
{
  if (fd >= MAX_FD || fd < 2) {
    return;
  }

  struct thread *t = thread_current ();
  struct fsys *opened_fsys = t->fd_table[fd];

  if (opened_fsys == NULL || opened_fsys->is_dir)
    return;

  struct file *opened_file = opened_fsys->file;
  
  if (opened_file == NULL) {
    return;
  }

  lock_acquire (&filesys_lock);
  file_seek (opened_file, position);
  lock_release (&filesys_lock);
}

unsigned
tell (int fd)
{
  if (fd >= MAX_FD || fd < 2) {
    return 0;
  }

  struct thread *t = thread_current ();
  struct fsys *opened_fsys = t->fd_table[fd];

  if (opened_fsys == NULL || opened_fsys->is_dir)
    return 0;

  struct file *opened_file = opened_fsys->file;
  int next;
  
  if (opened_file == NULL) {
    return 0;
  }

  lock_acquire (&filesys_lock);
  next = file_tell (opened_file);
  lock_release (&filesys_lock);

  return (unsigned) next;
}

void
close (int fd)
{
  if (fd >= MAX_FD || fd <= 1)
    return;

  struct thread *t = thread_current ();
  struct fsys *opened_fsys = t->fd_table[fd];

  if (opened_fsys == NULL)
    return;

  if (opened_fsys->is_dir) {
    lock_acquire (&filesys_lock);
    dir_close (opened_fsys->dir);
    lock_release (&filesys_lock);
    t->fd_table[fd] = NULL;
    return;
  }

  struct file *opened_file = opened_fsys->file;
 
  if (opened_file == NULL) {
    return;
  }

  lock_acquire (&filesys_lock);
  file_close (opened_file);
  lock_release (&filesys_lock);
  t->fd_table[fd] = NULL;
}

bool
chdir (const char *dir)
{
  struct dir *new_dir;

  if (thread_current ()->cur_dir == NULL)
    return false;

  if ((new_dir = dir_open_dir (dir)) != NULL) {
    dir_close (thread_current ()->cur_dir);
    thread_current ()->cur_dir = new_dir;
    return true;
  }
  else
    return false;
}

bool
mkdir (const char *dir)
{
  bool success;
  char *base_dir, new_dir[NAME_MAX + 1];
  struct dir *base;
  block_sector_t sector = -1;;

  base_dir = (char *)calloc (1, strlen (dir) + 1);
  success = dir_parse (dir, base_dir, new_dir);
  if (!success) {
    free (base_dir);
    return success;
  }

  success = ((base = dir_open_dir (base_dir)) != NULL);
  if (success) {
    success = free_map_allocate (1, &sector);
    success = success && dir_sub_create (sector, new_dir, base);
    dir_close (base);
  }
 
  free (base_dir);
  return success;
}

bool
readdir (int fd ,char *name)
{
  if (fd >= MAX_FD || fd < 2) {
    return false;
  }

  struct thread *t = thread_current ();
  struct fsys *opened_fsys = t->fd_table[fd];

  if (opened_fsys == NULL)
    return false;

  if (!opened_fsys->is_dir)
    return false;

  struct dir *dir = opened_fsys->dir;

  return dir_readdir (dir, name);
}

bool
isdir (int fd)
{
  if (fd >= MAX_FD || fd < 2) {
    return false;
  }

  struct thread *t = thread_current ();
  struct fsys *opened_fsys = t->fd_table[fd];

  return opened_fsys != NULL && opened_fsys->is_dir;
}

int
inumber (int fd)
{
  if (fd >= MAX_FD || fd < 2) {
    return false;
  }

  struct thread *t = thread_current ();
  struct fsys *opened_fsys = t->fd_table[fd];
  struct inode *inode;

  if (opened_fsys == NULL)
    return -1;

  if (opened_fsys->is_dir)
    inode = dir_get_inode (opened_fsys->dir);
  else
    inode = file_get_inode (opened_fsys->file);

  return inode_get_inumber (inode);
}

static bool
is_directory (struct inode* inode)
{
  return inode_is_dir (inode);
}
