# Design Document: Project 4 - File System

## Preliminaries

> If you have any preliminary comments on your submission, notes for the
> TAs, or extra credit, please give them here.


> Please cite any offline or online sources you consulted while preparing your
> submission, other than the Pintos documentation, course text, lecture notes,
> and course staff.


## Indexed and Extensible Files

### Data Structures

> A1: Copy here the declaration of each new or changed `struct` or `struct`
> member, global or static variable, `typedef`, or enumeration.  Identify the
> purpose of each in 25 words or less.

```
struct inode_disk
  {
    off_t length;                       /* File size in bytes.  */
    block_sector_t direct[12];          /* Direct blocks. */
    block_sector_t indirect;            /* Singly indirect blocks. */
    block_sector_t doubly_indirect;     /* Doubly indirect blocks. */
    uint32_t is_dir;                    /* 1: directory; 0: file */
    unsigned magic;                     /* Magic number. */
    uint32_t unused[111];               /* Not used. */
  };

```
> A2: What is the maximum size of a file supported by your inode structure?
> Show your work.

```
direct-- 12 sectors * 512 bytes per sector 
          = 6144 bytes
indirect -- 512/4 * 512 
          = 65536 bytes
doubly_indirect --  512/4 * 512/4 * 512 
          = 8388608 bytes
Total   -- 8460288 bytes

```
### Synchronization

> A3: Explain how your code avoids a race if two processes attempt to extend a
> file at the same time.
```
in 'void
buffer_cache_write (block_sector_t sector, const void *source)' function, using 'lock_acquire (&buffer_cache_lock)' and  'lock_release (&buffer_cache_lock)'function, to aquire and release a lock, thus avoiding two processes attempt to extend a file at the same time.
```
> A4: Suppose processes A and B both have file F open, both positioned at
> end-of-file.  If A reads and B writes F at the same time, A may read all,
> part, or none of what B writes.  However, A may not read data other than what
> B writes, e.g. if B writes nonzero data, A is not allowed to see all zeros.
> Explain how your code avoids this race.

```
A will see no data in our implementation if it tries to read while B is writing. A will see the entire  extension if it tries to read after B has finished writing becasue of the 'lock_acquire (&buffer_cache_lock)' and 'lock_release (&buffer_cache_lock)'during 'buffer_cache_read' or 'buffer_cache_write' 

```
> A5: Explain how your synchronization design provides "fairness". File access
> is "fair" if readers cannot indefinitely block writers or vice versa.  That
> is, many processes reading from a file cannot prevent forever another process
> from writing the file, and many processes writing to a file cannot prevent
> another process forever from reading the file.

```
no condition variables for 'buffer_cache_lock' in 'buffer_cache_read' or 'buffer_cache_write' function, thus readers and writers are not treated differently when they are trying to acquire the lock 'buffer_cache_lock'.
```
### Rationale

> A6: Is your inode structure a multilevel index?  If so, why did you choose
> this particular combination of direct, indirect, and doubly indirect blocks?
> If not, why did you choose an alternative inode structure, and what advantages
> and disadvantages does your structure have, compared to a multilevel index?

```
A multilevel index. 
Direct blocks:'block_sector_t direct[12]',  
Singly indirect blocks:'block_sector_t indirect'       
Doubly indirect blocks:'block_sector_t doubly_indirect'    
to easily get the needed file size representation.
```
## Subdirectories

### Data Structures

> B1: Copy here the declaration of each new or changed `struct` or `struct`
> member, global or static variable, `typedef`, or enumeration.  Identify the
> purpose of each in 25 words or less.

```
struct inode_disk
  {
    uint32_t is_dir; /* 1: directory; 0: file */
  };

```
```
struct inode 
  {

    bool is_dir; /* 1: directory; 0: file */
  };
```
```
struct fsys {
  bool is_dir;
  struct file *file;
  struct dir *dir;
};

```

```
struct thread {
  struct dir *cur_dir; /*current directory*/
}
```

### Algorithms

> B2: Describe your code for traversing a user-specified path.  How do
> traversals of absolute and relative paths differ?

```
in 'bool dir_parse (const char *dir, char base[NAME_MAX + 1], char name[NAME_MAX + 1])'fuction, first determine the initial point for file system traversal. If the path starts with '/', begin from the root directory. Otherwise, start from the current working directory of the thread. This initial point becomes "traversal current directory".
Next, iterate over the path string, examining each token separated by '/'. For each token, look it up in the traversal current directory. If the token is found, update traversal current directory to that entry.
```

### Synchronization

> B4: How do you prevent races on directory entries?  For example, only one of
> two simultaneous attempts to remove a single file should succeed, as should
> only one of two simultaneous attempts to create a file with the same name, and
> so on.

```
Locks are used for synchronization. 
```
> B5: Does your implementation allow a directory to be removed if it is open by
> a process or if it is in use as a process's current working directory?  If so,
> what happens to that process's future file system operations?  If not, how do
> you prevent it?

```
No, it does not allowed.
it will only get deleted when it is closed by all processes who have it open.  
```
### Rationale

> B6: Explain why you chose to represent the current directory of a process the
> way you did.

```
struct thread {
  struct dir *cur_dir; /*current directory*/
}
The directory's name may change, but the sector behind it would not change.
when change the current working directory of a thread, that thread will reopen the inode of the directory it entered and close the inode of the one it left.
```
## Buffer Cache

### Data Structure

> C1: Copy here the declaration of each new or changed `struct` or `struct`
> member, global or static variable, `typedef`, or enumeration.  Identify the
> purpose of each in 25 words or less.

```
/* buffer cache entry */
struct buffer_cache_entry_t {
  bool occupied;  /*indicating whether the entry being used or not*/
  block_sector_t disk_sector; /*The on-disk sector index*/
  uint8_t buffer[BLOCK_SECTOR_SIZE]; /*data*/
  bool dirty;     /*indicating whether the entry being modified or not*/
  bool access; /*indicating whether the entry is accessed recently or not*/   
};
```

```
/*Maintain all 64 struct buffer_cache_entry_t by array*/
static struct buffer_cache_entry_t cache[BUFFER_CACHE_SIZE];

```
```
static struct lock buffer_cache_lock; /*buffer_cache_lock for prevent race condition*/

```
### Algorithm

> C2: Describe how your cache replacement algorithm chooses a cache block to
> evict.

```
Use Clock Algorithm
Keep pointer to last examined buffer cache;
Traverse buffer caches in circular buffer;
Clear 'access' bits upon traversal;
Stop when find buffer cache with already cleared 'access' bit;
replace this buffer cache; 
increment pointer;
```

> C3: Describe your implementation of write-behind.

```
in buffer cache eviction function 'static struct buffer_cache_entry_t*
buffer_cache_evict (void)', after choosing the cache to evict, chceking the 'dirty' bit,
if it is modified, call 'buffer_cache_flush ()' funtion to write data to disk.

```
> C4: Describe your implementation of read-ahead.

```
Not implemented.

```
### Algorithm

> C5: When one process is actively reading or writing data in a buffer cache
> block, how are other processes prevented from evicting that block?

```
during 'buffer_cache_read' or  'buffer_cache_write', using 'lock_acquire (&buffer_cache_lock)' after entering the funtion and 'lock_release (&buffer_cache_lock)' before leaving the funtion to ensure no other processes can intervene

```
> C6: During the eviction of a block from the cache, how are other processes
> prevented from attempting to access the block?


```
During the eviction, after entering the 'static struct buffer_cache_entry_t*
buffer_cache_evict (void)' function, first make assertion 'ASSERT (lock_held_by_current_thread(&buffer_cache_lock))' to ensure the current process hold the 'buffer_cache_lock', then start to evict, thus other processes are prevented from attempting to access the block

```

### Rationale

> C7: Describe a file workload likely to benefit from buffer caching, and
> workloads likely to benefit from read-ahead and write-behind.

```
for buffer caching, workloads where the same data is read multiple times will benifit.
for read-ahead, workloads where data is read in a sequential manner will benefit.
for write-behind, workloads where immediate persistence of data is not critical will benefit.
```
## Survey Questions

Answering these questions is optional, but it will help us improve the course in
future semester.  Feel free to tell us anything you want--these questions are
just to spur your thoughts.  You may also choose to respond anonymously in the
course evaluations at the end of the quarter.

> In your opinion, was this assignment, or any one of the three problems in it,
> too easy or too hard?  Did it take too long or too little time?

> Did you find that working on a particular part of the assignment gave you
> greater insight into some aspect of OS design?

> Is there some particular fact or hint we should give students in future
> quarters to help them solve the problems?  Conversely, did you find any of our
> guidance to be misleading?

> Do you have any suggestions for the TAs to more effectively assist students in
> future quarters?

> Any other comments?
