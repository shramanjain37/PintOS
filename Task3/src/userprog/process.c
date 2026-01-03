#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "vm/page.h"
#include "vm/frame.h"

static thread_func start_process NO_RETURN;
static bool load (const char *file_name_, void (**eip) (void), void **esp);


struct exec_info 
  {
    const char *file_name_;             
    struct semaphore load_done;         
    struct wait_status *wait_status;   
    bool success;                      
  };

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name_) 
{
  struct exec_info exec;
  char thread_name[16];
  char *save_ptr;
  tid_t tid;

  /* Initialize exec_info. */
  exec.file_name_ = file_name_;
  sema_init (&exec.load_done, 0);

  /* Create a new thread to execute FILE_NAME. */
  strlcpy (thread_name, file_name_, sizeof thread_name);
  strtok_r (thread_name, " ", &save_ptr);
  tid = thread_create (thread_name, PRI_DEFAULT, start_process, &exec);
  if (tid != TID_ERROR)
    {
      sema_down (&exec.load_done);
      if (exec.success)
        list_push_back (&thread_current ()->children, &exec.wait_status->elem);
      else 
        tid = TID_ERROR;
    }

  return tid;
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *exec_)
{
  struct exec_info *exec = exec_;
  struct intr_frame if_;
  bool success;

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  success = load (exec->file_name_, &if_.eip, &if_.esp);

  if (success)
    {
      exec->wait_status = thread_current ()->wait_status
        = malloc (sizeof *exec->wait_status);
      success = exec->wait_status != NULL; 
    }

  if (success) 
    {
      lock_init (&exec->wait_status->lock);
      exec->wait_status->ref_cnt = 2;
      exec->wait_status->tid = thread_current ()->tid;
      sema_init (&exec->wait_status->dead, 0);
    }
  
  exec->success = success;
  sema_up (&exec->load_done);
  if (!success) 
    thread_exit ();

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}


static void
release_child (struct wait_status *cs) 
{
  int new_ref_cnt;
  
  lock_acquire (&cs->lock);
  new_ref_cnt = --cs->ref_cnt;
  lock_release (&cs->lock);

  if (new_ref_cnt == 0)
    free (cs);
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting. */
int
process_wait (tid_t child_tid) 
{
  struct thread *cur = thread_current ();
  struct list_elem *e;

  for (e = list_begin (&cur->children); e != list_end (&cur->children);
       e = list_next (e)) 
    {
      struct wait_status *cs = list_entry (e, struct wait_status, elem);
      if (cs->tid == child_tid) 
        {
          int exit_status;
          list_remove (e);
          sema_down (&cs->dead);
          exit_status = cs->exit_status;
          release_child (cs);
          return exit_status;
        }
    }
  return -1;
}

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *cur = thread_current ();
  struct list_elem *e, *next;
  uint32_t *pd;

  printf ("%s: exit(%d)\n", cur->name, cur->exit_status);

  if (cur->wait_status != NULL) 
    {
      struct wait_status *cs = cur->wait_status;
      cs->exit_status = cur->exit_status;
      sema_up (&cs->dead);
      release_child (cs);
    }

  for (e = list_begin (&cur->children); e != list_end (&cur->children);
       e = next) 
    {
      struct wait_status *cs = list_entry (e, struct wait_status, elem);
      next = list_remove (e);
      release_child (cs);
    }

  page_exit ();
  
  /* Close executable (and allow writes). */
  file_close (cur->file);

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  if (pd != NULL) 
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp, const char *file_name);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, void (**eip) (void), void **esp) 
{
  struct thread *t = thread_current ();
  char file_name_[NAME_MAX + 2];
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  char *cp;
  int i;

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL) 
    goto done;
  process_activate ();

  t->pages = malloc (sizeof *t->pages);
  if (t->pages == NULL)
    goto done;
  hash_init (t->pages, page_hash, page_less, NULL);

  while (*file_name == ' ')
    file_name++;
  strlcpy (file_name_, file_name, sizeof file_name_);
  cp = strchr (file_name_, ' ');
  if (cp != NULL)
    *cp = '\0';

  /* Open executable file. */
  t->file = file = filesys_open (file_name_);
  if (file == NULL) 
    {
      printf ("load: %s: open failed\n", file_name_);
      goto done; 
    }
  file_deny_write (t->file);

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", file_name_);
      goto done; 
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
  if (!setup_stack (esp, file_name))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
  /* We arrive here whether the load is successful or not. */
  return success;
}

/* load() helpers. */


static bool install_page (void *upage, void *kpage, bool writable);


/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  while (read_bytes > 0 || zero_bytes > 0) 
    {
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;
      struct page *p = page_allocate (upage, !writable);
      if (p == NULL)
        return false;
      if (page_read_bytes > 0) 
        {
          p->file = file;
          p->file_offset = ofs;
          p->file_bytes = page_read_bytes;
        }
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      ofs += page_read_bytes;
      upage += PGSIZE;
    }
  return true;
}

static void *
push (uint8_t *kpage, size_t *ofs, const void *buf, size_t size) 
{
  size_t padsize = ROUND_UP (size, sizeof (uint32_t));
  if (*ofs < padsize)
    return NULL;

  *ofs -= padsize;
  memcpy (kpage + *ofs + (padsize - size), buf, size);
  return kpage + *ofs + (padsize - size);
}


static void
reverse (int argc, char **argv) 
{
  for (; argc > 1; argc -= 2, argv++) 
    {
      char *tmp = argv[0];
      argv[0] = argv[argc - 1];
      argv[argc - 1] = tmp;
    }
}



static bool
init_file_name (uint8_t *kpage, uint8_t *upage, const char *file_name,
               void **esp) 
{
  size_t ofs = PGSIZE;
  char *const null = NULL;
  char *file_name_copy;
  char *karg, *saveptr;
  int argc;
  char **argv;

  file_name_copy = push (kpage, &ofs, file_name, strlen (file_name) + 1);
  if (file_name_copy == NULL)
    return false;

  if (push (kpage, &ofs, &null, sizeof null) == NULL)
    return false;


  argc = 0;
  for (karg = strtok_r (file_name_copy, " ", &saveptr); karg != NULL;
       karg = strtok_r (NULL, " ", &saveptr))
    {
      void *uarg = upage + (karg - (char *) kpage);
      if (push (kpage, &ofs, &uarg, sizeof uarg) == NULL)
        return false;
      argc++;
    }

  argv = (char **) (upage + ofs);
  reverse (argc, (char **) (kpage + ofs));

  if (push (kpage, &ofs, &argv, sizeof argv) == NULL
      || push (kpage, &ofs, &argc, sizeof argc) == NULL
      || push (kpage, &ofs, &null, sizeof null) == NULL)
    return false;

  *esp = upage + ofs;
  return true;
}

/* Create a minimal stack for T by mapping a page at the
   top of user virtual memory.  Fills in the page using CMD_LINE
   and sets *ESP to the stack pointer. */
static bool
setup_stack (void **esp, const char *file_name) 
{
  struct page *page = page_allocate (((uint8_t *) PHYS_BASE) - PGSIZE, false);
  if (page != NULL) 
    {
      page->frame = frame_alloc_and_lock (page);
      if (page->frame != NULL)
        {
          bool ok;
          page->read_only = false;
          page->private = false;
          ok = init_file_name (page->frame->base, page->addr, file_name, esp);
          frame_unlock (page->frame);
          return ok;
        }
    }
  return false;
}
