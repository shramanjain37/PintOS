# Design Document: Project 3 - Virtual Memory


## Preliminaries

> If you have any preliminary comments on your submission, notes for the
> TAs, please give them here.

> Please cite any offline or online sources you consulted while
> preparing your submission, other than the Pintos documentation, course
> text, lecture notes, and course staff.
```

https://www.geeksforgeeks.org/paging-in-operating-system/
https://hasinisama.medium.com/page-frame-allocation-6ff9ce1c38ab
https://pasandevin.medium.com/build-your-own-os-part-7-9bf6d56720fe
https://unicorn-utterances.com/posts/virtual-memory-overview
https://wiki.osdev.org/Writing_A_Page_Frame_Allocator
https://anastas.io/osdev/memory/2016/08/08/page-frame-allocator.html
```


## Page Table Management

### Data Structures

> A1: Copy here the declaration of each new or changed `struct' or
> `struct' member, global or static variable, `typedef', or
> enumeration.  Identify the purpose of each in 25 words or less.

```
struct page 
  {
    void *addr;                 /* User virtual address */
    bool read_only;             /* Read-only page */
    struct thread *thread;      /* Owning thread */
    struct hash_elem hash_elem; /* struct thread `pages' hash element */
    struct frame *frame;        /* Page frame */
    block_sector_t sector;       /* Starting sector of swap area, or -1 */
    bool private;                /*flag to decide wether to write back to swap area or file.*/
    struct file *file;          /* File */
    off_t file_offset;          /* Offset in file */
    off_t file_bytes;           /* Bytes to read/write, 1...PGSIZE */
    
  };
```

```
struct frame 
  {
    struct lock lock;           /* Prevent simultaneous access. */
    void *base;                 /* Kernel virtual base address. */
    struct page *page;          /* Mapped process page, if any. */
  };

```

### Algorithms

> A2: In a few sentences, describe your code for accessing the data
> stored in the SPT about a given page.

```
Each page struct has a virtual user address field and a frame struct that contains kernal virtual address(which is corresponding to physical memory adress by an offset of PHYS_BASE,0xc000000).
Each page struct has hash element 'hash_elem', thus each virtual page is stored in the hash table 'pages'(a new member of thread struct).
Virtual page is created in heap area by 'page_allocate()'function. 
Virtual user address and kernal virtual address are mapped via the 'frame_alloc_and_lock()' function.

```


> A3: How does your code coordinate accessed and dirty bits between
> kernel and user virtual addresses that alias a single frame, or
> alternatively how do you avoid the issue?

```
Only user virtual addresses can update accessed and dirty bits.
In 'pagedir_set_accessed (uint32_t *pd, const void *vpage, bool accessed) ,'pagedir_is_accessed (uint32_t *pd, const void *vpage) ',
'pagedir_set_dirty (uint32_t *pd, const void *vpage, bool dirty) ','pagedir_is_dirty (uint32_t *pd, const void *vpage) 'funcion, 
it will check if the vpage is in user virtual address by using 'is_user_vaddr()'function.

```

### Synchronization

> A4: When two user processes both need a new frame at the same time,
> how are races avoided?

```
Searching for a new frame in protected via a lock called 'scan_lock' and a frame lock 'struct frame {struct lock lock}'.  Only one process can 'need a new frame at the same time'.

```

### Rationale

> A5: Why did you choose the data structure(s) that you did for
> representing virtual-to-physical mappings?

```
'struct page' has a member of 'struct frame *frame' , ' struct frame' has a member of  'struct page *page', thus they are one-one mapping. 

Reasons for Choosing These Data Structures:
1.Simplified Access: With a one-to-one mapping, accessing the corresponding physical frame for a virtual page (and vice versa) is straightforward. You can directly follow the pointers without the need for complex lookup mechanisms.

2.Ease of Debugging: When each page maps directly to a single frame, tracking and debugging memory-related issues becomes more straightforward.

3.Minimal Overhead: The additional memory overhead for maintaining these pointers is minimal compared to the benefits of fast and direct access. Given that these structures are small and efficient, the overhead in terms of memory consumption is negligible.
```

## Paging To and From Disk

### Data Structures

> B1: Copy here the declaration of each new or changed `struct' or
> `struct' member, global or static variable, `typedef', or
> enumeration.  Identify the purpose of each in 25 words or less.

```
/* The swap device. */
static struct block *swap_device;

/* Used swap pages. */
static struct bitmap *swap_bitmap;

/* Protects swap_bitmap. */
static struct lock swap_lock;
```

### Algorithms

> B2: When a frame is required but none is free, some frame must be
> evicted.  Describe your code for choosing a frame to evict.

```
Naively iterating through all the frames in frame table, with simple variant algorithm "second chance"(by iterating twice), 
checking if it has been accessed with 'page_recently_accessed()' function, which is based on the 'pagedir_is_accessed()'function by checking if 'access'bit is set. 
If it is not accessed recently, use 'page_out()' to evict the page.

```

> B3: When a process P obtains a frame that was previously used by a
> process Q, how do you adjust the page table (and any other data
> structures) to reflect the frame Q no longer has?

```
Use hash table funcion 'hash_destroy(h, destroy_page)' to remove the frame from that previous process.
```

> B4: Explain your heuristic for deciding whether a page fault for an
> invalid virtual address should cause the stack to be extended into
> the page that faulted.

```
In page_fault handler, check if the fault address larger than 'PHYS_BASE - MAX_STACK' and larger than saved user stack pointer 'thread_current()->user_esp - 32' with 'is_stack_access()' function.
user stack pointer 'thread_current()->user_esp' was abtained by reading 'intr_frame frame->esp' in 'intr_handler'funtion.
```


### Synchronization

> B5: Explain the basics of your VM synchronization design.  In
> particular, explain how it prevents deadlock.  (Refer to the
> textbook for an explanation of the necessary conditions for
> deadlock.)

```
1.Avoiding Circular Wait:
Impose a strict ordering on the acquisition of locks. Processes must acquire locks in a predefined order, preventing circular dependencies. 
Lock 'scan_lock' before locking 'frame lock', because scan_lock is only within the scope of one funcion.

2.Allowing Preemption:
In 'frame_alloc_and_lock()' function, using 'timer_msleep (1000)' to allow other process to pre-empt.

3.Mitigating Mutual Exclusion:
Reduce the scope of locks to the smallest possible critical section, allowing more parallelism and reducing the chance of contention.

```

> B6: A page fault in process P can cause another process Q's frame
> to be evicted.  How do you ensure that Q cannot access or modify
> the page during the eviction process?  How do you avoid a race
> between P evicting Q's frame and Q faulting the page back in?

```
Implement a locking mechanism on the frame that is being evicted. This lock will ensure that no other process can access or modify the frame while the eviction is in progress.
When process P determines that it needs to evict a frame that is currently used by process Q, it must first acquire the lock on that frame. This prevents process Q (or any other process) from accessing the frame during the eviction process.
```

> B7: Suppose a page fault in process P causes a page to be read from
> the file system or swap.  How do you ensure that a second process Q
> cannot interfere by e.g. attempting to evict the frame while it is
> still being read in?

```
Implement a locking mechanism.
Using a frame lock 'struct frame {struct lock lock}' and a scan_lock 'static struct lock scan_lock' to ensure that a second process cannot interfere.
Each frame has an associated lock to ensure exclusive access to that particular frame during operations like reading from disk or eviction.This prevents any other process from modifying or evicting the frame while it is being used.
Static scan_lock is used to synchronize operations that scan or modify the state of multiple frames, such as the eviction process.This lock ensures that frame selection and eviction are performed atomically and consistently.
```

> B8: Explain how you handle access to paged-out pages that occur
> during system calls.  Do you use page faults to bring in pages (as
> in user programs), or do you have a mechanism for "locking" frames
> into physical memory, or do you use some other design?  How do you
> gracefully handle attempted accesses to invalid virtual addresses?

```
Use "page_fault()" to 'page_in()'. If it is invalid virtual addresses, terminate the thread.
```

### Rationale

> B9: A single lock for the whole VM system would make
> synchronization easy, but limit parallelism.  On the other hand,
> using many locks complicates synchronization and raises the
> possibility for deadlock but allows for high parallelism.  Explain
> where your design falls along this continuum and why you chose to
> design it this way.

```
Design two locks:

1. A frame lock'struct frame {struct lock lock; }' spans over different functions to ensure no other process can access to a specific frame during one process's frame operation for a given page. Each frame has an associated lock to ensure exclusive access to that particular frame during operations like reading from disk or eviction.This prevents any other process from modifying or evicting the frame while it is being used.

2.'static struct lock scan_lock' is another lock only used for frame allocation in '*allocate_frame(struct page *page)'function, it is locked and unlocked within this function. 'scan_lock' is used to synchronize operations that scan or modify the state of multiple frames, such as the eviction process.This lock ensures that frame allocation and eviction are performed atomically and consistently.

```


## Survey Questions

Answering these questions is optional, but it will help us improve the
course in future quarters.  Feel free to tell us anything you
want--these questions are just to spur your thoughts.  You may also
choose to respond anonymously in the course evaluations at the end of
the quarter.

> In your opinion, was this assignment, or any one of the three problems
> in it, too easy or too hard?  Did it take too long or too little time?

> Did you find that working on a particular part of the assignment gave
> you greater insight into some aspect of OS design?

> Is there some particular fact or hint we should give students in
> future quarters to help them solve the problems?  Conversely, did you
> find any of our guidance to be misleading?

> Do you have any suggestions for the TAs to more effectively assist
> students, either for future quarters or the remaining projects?

> Any other comments?
