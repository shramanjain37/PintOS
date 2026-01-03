# Design Document: Project 1 - Threads

## Preliminaries

> If you have any preliminary comments on your submission, notes for the
> TAs, or extra credit, please give them here.

> Please cite any offline or online sources you consulted while
> preparing your submission, other than the Pintos documentation, course
> text, lecture notes, and course staff.

online sources

https://dev.to/rinsama77/process-synchronization-with-busy-waiting-4gho

https://www.geeksforgeeks.org/introduction-of-process-synchronization/

https://www.scaler.com/topics/operating-system/process-synchronization-in-os/

https://www.scaler.com/topics/operating-system/priority-scheduling-algorithm/#

https://www.studytonight.com/operating-system/priority-scheduling




## Alarm Clock

### Data Structures

> A1: Copy here the declaration of each new or changed `struct` or
> `struct` member, global or static variable, `typedef`, or
> enumeration.  Identify the purpose of each in 25 words or less.

Added to 'struct thread':
```
  int64_t sleep_ticks; /* store 'ticks' that thread should sleep.*/
```
New 'sleep_list' is a list in ascending order by sleep_ticks, used to store threads that are put to sleep.
```
  static struct sleep_list;
```


### Algorithms

> A2: Briefly describe what happens in a call to `timer_sleep()`,
> including the effects of the timer interrupt handler.

In a call to timer_sleep(), if ticks>0, a function called thread_sleep(ticks) is called. In that function, the current thread's sleep ticks is set to sleep_ticks+timer_ticks(). Interrupts are turned off (necessary to call thread_block). The thread is inserted to sleep_list. An ordered list is created according to sleep_ticks of thread. Then thread_block is called to block the thread. Finally, interrupts are set to old level.

In the timer interrupt handler, the list is checked if any thread needs to woken up. If any, thread's sleep_ticks are reset. Interrupts are disabled and thread is removed from the sleep list. The thread is unblocked and interrupts are set to old level.

> A3: What steps are taken to minimize the amount of time spent in
> the timer interrupt handler?

An ordered list is used instead of a regular list for sleep_ticks. This prevents checking all later threads in sleep list, thus saving time.

### Synchronization

> A4: How are race conditions avoided when multiple threads call
> `timer_sleep()` simultaneously?

The interrupt is disabled when list operations happen. 

> A5: How are race conditions avoided when a timer interrupt occurs
> during a call to `timer_sleep()`?

Race conditions are avoided by disabling the interrupt.

### Rationale

> A6: Why did you choose this design?  In what ways is it superior to
> another design you considered?

The method used is straightforward. Other than using regular list than ordered list(which is superior as discussed above), no other designs were considered.



## Producer-Consumer

### Synchronization

> B1: How does your solution guarantee that consumers will hold until
> there is something to consume in the buffer?

The function cond_wait is used to actually check and wait, and consume only when data is available. 

> B2: How does your solution guarantee that producers will hold until
> there is some free space in the buffer?
 
 Similar to consumer, the function cond_wait is used to actually check and wait, and produce only when there is space available.

> B3: How does your solution preserve a FIFO semantics i.e., the first
> character produced will be the first to be consumed?

By maintaining index for both producer and consumer, it is ensured that the characters will be removed in the same order added.

### Rationale

> B4: Give an intuition for why your program preserves safety.

Usage of synchronization and condition variables help preserve the safety of the program. Condition variables prevents overflowing or underflowing of the buffer while synchronization help avoid deadlocks, thus preserving safety.

> B5: Why did you choose this design? Did you consider other design
> alternatives? In what ways is it superior to another design you considered?

It was mentioned to use only locks and condition variables, therefore no other design choices were considered.

## Priority Scheduling

### Data Structures

> C1: Copy here the declaration of each new or changed `struct` or
> `struct' member, global or static variable, `typedef`, or
> enumeration.  Identify the purpose of each in 25 words or less.

Added to 'struct thread':
```
  int original_priority; /* used to return original priority(when thread is created) when donation is completed. */

  struct list donations; /* list of priority doner threads.*/

  struct lock* wait_on_lock; /* lock that a thread has requested but hold by another thread. */
   
  struct list_elem donation_elem; /* list element in 'donations'list.*/
```


> C2: Explain the data structure used to track priority donation.
> Use ASCII art to diagram a nested donation.  (Alternately, submit a
> .png file.)

In general,'struct thread','struct lock' and 'struct semaphore' are used to track priority donation.

In 'struct thread':

When a thread is created, 'priority' and 'original_priority' are set the same. 'original_priority' is used as a reference when thread update its 'priority' when donation is completed. 
'wait_on_lock' stores a lock that a thread has requested but occupied by another thread.  
'donations' list store the priority doner threads. 

In'struct lock' and 'struct semaphore':

'holder' keeps track of the lock holder.
'semaphore.value' keeps track of the lock state.
'semaphore.waiters' is a list filled with blocked threads waiting for hold the lock.

Diagram a nested donation:

Before nesting:

1 'thread' 1 hold 'lock a' , 'thread 2' hold 'lock b' and tried to hold 'lock a'.

2  higher priority 'thread 2' tried to hold 'lock a', which hold by 'thread 1', thus 'thread 2' is blocked.

3  priority donation happens,'priority' of 'thread 1'is set to the 'priority' of 'thread 2'.

4 'lock a' is hold by 'thread 1',and its 'semaphore.waiters'has 'thread 2'.

5  'thread 1' has 'thread 2 ' in its 'donations'list.

6  'thread 2' has 'lock a' in 'wait_on_lock' field.

7  'lock b' is hold by 'thread 2',and its 'semaphore.waiters'has no waiting threads.

```
.---------------------------------------------------.
|                Thread 1                           |      
+-------------------+-------------------------------+
| member            | value                         |
+-------------------+-------------------------------+
| priority          |32                             |
| original_priority |31                             |
| wait_on_lock      |NULL	                          |
| donations         |Thread 2                       |   
'-------------------+-------------------------------'

.---------------------------------------------------.
|                Lock a                             |      
+-------------------+-------------------------------+
| member            | value                         |
+-------------------+-------------------------------+
| holder            |Thread 1                       |
| semaphore.value   |0                              |
| semaphore.waiters |Thread 2	                      |
'-------------------+-------------------------------'

.---------------------------------------------------.
|                Thread 2                           |      
+-------------------+-------------------------------+
| member            | value                         |
+-------------------+-------------------------------+
| priority          |32                             |
| original_priority |32                             |
| wait_on_lock      |Lock a	                        |
| donations         |NULL                           |      
'-------------------+-------------------------------'

.---------------------------------------------------.
|                Lock b                             |      
+-------------------+-------------------------------+
| member            |value                          |
+-------------------+-------------------------------+
| holder            |Thread 2                       |
| semaphore.value   |0                              |
| semaphore.waiters |NULL	                          |
'-------------------+-------------------------------'
```

After nesting

 1 'thread' 1 hold 'lock a' , 'thread 2' hold 'lock b', and tried to hold 'lock a', 'thread 3' tried to hold 'lock b'.

 2  priority donation happens,'priority' of 'thread 1' and 'thread 2' are set the 'priority' of 'thread 3'.

 3 'lock a' is hold by 'thread 1',and its 'semaphore.waiters'has 'thread 2'.

 4  'thread 2' has 'thread 3 ' in 'donations' list.

 5  'lock b' is hold by 'thread 2' and 'semaphore.waiters' has 'thread 3'.

 6  'thread 3' has 'lock b' in 'wait_on_lock' field.


```
.---------------------------------------------------.
|                Thread 1                           |      
+-------------------+-------------------------------+
| member            | value                         |
+-------------------+-------------------------------+
| priority          |33                             |
| original_priority |31                             |
| wait_on_lock      |NULL	                          |
| donations         |Thread 2                       |
'-------------------+-------------------------------'

.---------------------------------------------------.
|                Lock a                             |      
+-------------------+-------------------------------+
| member            | value                         |
+-------------------+-------------------------------+
| holder            |Thread 1                       |
| semaphore.value   |0                              |
| semaphore.waiters |Thread 2	                      |
'-------------------+-------------------------------'

.---------------------------------------------------.
|                Thread 2                           |      
+-------------------+-------------------------------+
| member            | value                         |
+-------------------+-------------------------------+
| priority          |33                             |
| original_priority |32                             |
| wait_on_lock      |Lock a	                        |
| donations         |Thread 3                       |
'-------------------+-------------------------------'

.---------------------------------------------------.
|                Lock b                             |      
+-------------------+-------------------------------+
| member            |value                          |
+-------------------+-------------------------------+
| holder            |Thread 2                       |
| semaphore.value   |0                              |
| semaphore.waiters |Thread 3	                      |
'-------------------+-------------------------------'
.---------------------------------------------------.
|                Thread 3                           |      
+-------------------+-------------------------------+
| member            | value                         |
+-------------------+-------------------------------+
| priority          |33                             |
| original_priority |33                             |
| wait_on_lock      |lock b	                        |
| donations         |NULL                           |
'-------------------+-------------------------------'
```

After 'thread 1'release 'lock a','thread 2' hold 'lock a'.

1 'thread 1' set the 'priority' to 'original_priority'.

2 'thread 1' remove all other threads in 'donations' list.

3 'lock a' has no 'semaphore.waiters'.

4 'thread 2' has no 'wait_on_lock'.

```
.---------------------------------------------------.
|                Thread 1                           |      
+-------------------+-------------------------------+
| member            | value                         |
+-------------------+-------------------------------+
| priority          |31                             |
| original_priority |31                             |
| wait_on_lock      |NULL	                          |
| donations         |NULL                           |
'-------------------+-------------------------------'

.---------------------------------------------------.
|                Lock a                             |      
+-------------------+-------------------------------+
| member            | value                         |
+-------------------+-------------------------------+
| holder            |Thread 2                       |
| semaphore.value   |0                              |
| semaphore.waiters |NULL	                          |
'-------------------+-------------------------------'

.---------------------------------------------------.
|                Thread 2                           |      
+-------------------+-------------------------------+
| member            | value                         |
+-------------------+-------------------------------+
| priority          |33                             |
| original_priority |32                             |
| wait_on_lock      |NULL	                          |
| donations         |Thread 3                       |
'-------------------+-------------------------------'

.---------------------------------------------------.
|                Lock b                             |      
+-------------------+-------------------------------+
| member            |value                          |
+-------------------+-------------------------------+
| holder            |Thread 2                       |
| semaphore.value   |0                              |
| semaphore.waiters |Thread 3	                      |
'-------------------+-------------------------------'
.---------------------------------------------------.
|                Thread 3                           |      
+-------------------+-------------------------------+
| member            | value                         |
+-------------------+-------------------------------+
| priority          |33                             |
| original_priority |33                             |
| wait_on_lock      |lock b	                        |
| donations         |NULL                           |
'-------------------+-------------------------------'
```

After 'thread 2'release 'lock a' and release 'lock b'.'thread 3' hold 'lock b'.

1 'thread 2' set the 'priority' to 'original_priority'.

2 'thread 2' remove all other threads in 'donations' list.

3 'lock b' has no 'semaphore.waiters'.

4 'thread 3' has no 'wait_on_lock'.


```
.---------------------------------------------------.
|                Thread 1                           |      
+-------------------+-------------------------------+
| member            | value                         |
+-------------------+-------------------------------+
| priority          |31                             |
| original_priority |31                             |
| wait_on_lock      |NULL	                          |
| donations         |NULL                           |
'-------------------+-------------------------------'

.---------------------------------------------------.
|                Lock a                             |      
+-------------------+-------------------------------+
| member            | value                         |
+-------------------+-------------------------------+
| holder            |NULL                           |
| semaphore.value   |0                              |
| semaphore.waiters |NULL	                          |
'-------------------+-------------------------------'

.---------------------------------------------------.
|                Thread 2                           |      
+-------------------+-------------------------------+
| member            | value                         |
+-------------------+-------------------------------+
| priority          |32                             |
| original_priority |32                             |
| wait_on_lock      |NULL	                          |
| donations         |NULL                           |
'-------------------+-------------------------------'

.---------------------------------------------------.
|                Lock b                             |      
+-------------------+-------------------------------+
| member            |value                          |
+-------------------+-------------------------------+
| holder            |Thread 3                       |
| semaphore.value   |0                              |
| semaphore.waiters |NULL	                          |
'-------------------+-------------------------------'
.---------------------------------------------------.
|                Thread 3                           |      
+-------------------+-------------------------------+
| member            | value                         |
+-------------------+-------------------------------+
| priority          |33                             |
| original_priority |33                             |
| wait_on_lock      |NULL	                          |
| donations         |NULL                           |
'-------------------+-------------------------------'
```


### Algorithms

> C3: How do you ensure that the highest priority thread waiting for
> a lock, semaphore, or condition variable wakes up first?

The waiters list for the lock is changed to sort in descending order based on the priority.

> C4: Describe the sequence of events when a call to `lock_acquire()`
> causes a priority donation.  How is nested donation handled?

When requesting a lock, if there is already a thread occupying the lock (holder), priority is donated to the holder. Then it is set to wait_on_lock. Current thread is added to holder's donations in descendeing order based on priority. If the holder has also requested a lock, priority donations must also be made to the holder of the lock requested by the holder. For this, a recursive donation function priority_donate() is created which donates recursively up to 8 depth (as mentioned in assignment). After obtaining the lock and also executing sema_down function, wait_on_lock is deleted.

> C5: Describe the sequence of events when `lock_release()` is called
> on a lock that a higher-priority thread is waiting for.

 When the current thread returns the lock, if there is a thread that requested this lock and a donation has been received, the donation received from that thread must be withdrawn. The thread that requested the lock is removed from the donations list. A new function remove_donor is created and the lock holder is removed after calling the function. After removal, the priority of the current thread is reassigned to the priority of the thread with the highest priority among the remaining donations list.

### Synchronization

> C6: Describe a potential race in `thread_set_priority()` and explain
> how your implementation avoids it.  Can you use a lock to avoid
> this race?

During priority donation, the lock holder’s priority may be set by it’s donor, at the mean time, the thread itself may want to change the priority. If the donor and the thread itself set the priority in a different order, may cause a different result.
We use the functions priority_update_for_donations() and thread_priority_yield() to avoid it. No, a lock cannot be used to avoid this race. 

### Rationale

> C7: Why did you choose this design?  In what ways is it superior to
> another design you considered?

The design follows textbook priority scheduling while implementing donation and overcoming priority inversion. No other design were considered.

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
