# Design Document: Project 2 - User Programs

## Preliminaries

> If you have any preliminary comments on your submission, notes for the
> TAs, or extra credit, please give them here.

> Please cite any offline or online sources you consulted while
> preparing your submission, other than the Pintos documentation, course
> text, lecture notes, and course staff.

https://student.cs.uwaterloo.ca/cs350/F20/assignments/a2b-hints.shtml#:~:text=passing%20arguments,to%20the%20actual%20argument%20strings.
https://lass.cs.umass.edu/~shenoy/courses/fall13/lectures/Lec03_notes.pdf
https://www.geeksforgeeks.org/command-line-arguments-in-c-cpp/
https://www.geeksforgeeks.org/introduction-of-system-call/
https://www.prepbytes.com/blog/operating-system/system-calls-in-operating-system/
https://www.skillvertex.com/blog/input-output-system-calls-in-c-create-open-close-read-write/
https://student.cs.uwaterloo.ca/~cs350/S18/notes/proc-2up.pdf



## Argument Passing



### Data Structures

> A1: Copy here the declaration of each new or changed `struct' or
> `struct' member, global or static variable, `typedef', or
> enumeration.  Identify the purpose of each in 25 words or less.

None.

### Algorithms

> A2: Briefly describe how you implemented argument parsing.  
>
Implementing argument passing can be divided into 2 main tasks.
The first task is to split the file_name with arguments provided by the function process_execute(). The file_name is used as the thread name and arguments are passed down to functions start_process(), load() and setup_stack(). Function strtok_r() is used to split the command line string.
The second task is to implement/modify the setup_stack() function. The arguments and commands are pushed into the stack after initializing the page.


> How do you arrange for the elements of argv[] to be in the right order?
>
We check the string backwards, i.e the first string is the last command and the last string is the first command. We keep decreasing the esp pointer to set up the argv[] elements.


> How do you avoid overflowing the stack page?
>
We decided to terminate process if it provides too many arguments. This is handled in page fault exception. We execute exit(-1) whenever overflow occurs.


### Rationale

> A3: Why does Pintos implement strtok_r() but not strtok()?

We need to put address of the arguments somewhere we can access later. Save_ptr stores the address of the argument for future use, which is returned by strtok_r().


> A4: In Pintos, the kernel separates commands into a executable name
> and arguments.  In Unix-like systems, the shell does this
> separation.  Identify at least two advantages of the Unix approach.

1. Validity checking: Unix approach checks the validity of arguments and see they are not over the limit.
2. Kernel cost: The time inside kernel is shorter in Unix approach.


## System Call

### Data Structures

> B1: Copy here the declaration of each new or changed `struct' or
> `struct' member, global or static variable, `typedef', or
> enumeration.  Identify the purpose of each in 25 words or less.

in struct thread:

struct thread *parent;              /* Parent thread*/ 
    
    struct list children;               /* list of children */

    struct list_elem child_elem;        /* List element for list children. */

    int child_load_status;              /* Load status of its child*/

    int child_exit_status;              /* Exit status of its child*/ 
    
    struct list open_fd;                /* File descriptors that the thread opens*/

    struct file *file;                  /* Executable file of thread. */
    
    struct semaphore process_wait;      /* Determine whether thread should wait. */


in syscall.c:

struct file_desc
{
  int fd; // file descriptor number
  struct file *file;
  struct list_elem elem;

}; // struct for file descriptors

static struct lock filesys_lock;  /* lock used by syscalls involving file system to ensure only one thread at a time is accessing file system */

> B2: Describe how file descriptors are associated with open files.
> Are file descriptors unique within the entire OS or just within a
> single process?

 
File descriptors have one to one mapping to each file opened through syscall. Each file is allocated to a unique descriptor as a number, therefore file descriptor is unique within entire OS.



### Algorithms

> B3: Describe your code for reading and writing user data from the
> kernel.

Both read and write operations are similar. First, buffer pointer range is checked, and if buffer and buffer+size are both valid pointers. If not then exit(-1) is executed. Next, see if buffer pointer is doing writing or reading operation. Then lock (filesys_lock) is acquired and the descriptor of the thread holds the lock when the file is being read or written. STDOUT_FILENO and STDIN_FILENO are used. STDOUT_FILENO is standard output and in this case, release the lock and return -1. STDIN_FILENO is standard input and in this case, take keys from standard input, release lock and return 0;


> B4: Suppose a system call causes a full page (4,096 bytes) of data
> to be copied from user space into the kernel.  What is the least
> and the greatest possible number of inspections of the page table
> (e.g. calls to pagedir_get_page()) that might result?  What about
> for a system call that only copies 2 bytes of data?  Is there room
> for improvement in these numbers, and how much?

The least number of inspecion is 1. Pagedir_get_page gets a page head back in the first inspection.
For a full page of data, the greatest number might be 4096. In this case, we have to check every address to ensure access is valid. The greatest number will be 2 if its contiguous, if we get kernel virtual address that is not a page head.

For 2 bytes of data, the least number is 1 and greatest number will be 2. The operation is same for a system call that copies only 2 bytes of data.
There is no room for improvement.


> B5: Briefly describe your implementation of the "wait" system call
> and how it interacts with process termination.

Wait system call is implemented by process _wait() function. Corresponding child thread is found under the parent thread, and then process_wait is executed.
All held resources will be released when the child thread is terminated. If parent terminates early, the list and all the structs in it will be free, then the child will find out that the parent already exit and give up setting the status and continue to execute.

> B6: Any access to user program memory at a user-specified address
> can fail due to a bad pointer value.  Such accesses must cause the
> process to be terminated.  System calls are fraught with such
> accesses, e.g. a "write" system call requires reading the system
> call number from the user stack, then each of the call's three
> arguments, then an arbitrary amount of user memory, and any of
> these can fail at any point.  This poses a design and
> error-handling problem: how do you best avoid obscuring the primary
> function of code in a morass of error-handling?  Furthermore, when
> an error is detected, how do you ensure that all temporarily
> allocated resources (locks, buffers, etc.) are freed?  In a few
> paragraphs, describe the strategy or strategies you adopted for
> managing these issues.  Give an example.

The buffer pointer is checked before accessing memory to make sure that all the arguuments of syscall are in the user memory and  ot in the kernel memory. All the pointers in kernel memory or with NULL value will point to kernel to cause page fault. Is_valid_ptr() is used to avoid bad user memory access. Sys_exit() function will be called to exit once the page fault occurs. For example, in “write” system call, the esp pointer and the three arguments pointer will be checked first and if anything is found invalid then terminate the process. Then after enter into write function, the buffer beginning pointer and the buffer ending pointer(buffer + size - 1) will be checked before being used. 

### Synchronization

> B7: The "exec" system call returns -1 if loading the new executable
> fails, so it cannot return before the new executable has completed
> loading.  How does your code ensure this?  How is the load
> success/failure status passed back to the thread that calls "exec"?

When the status of thread child is changed, the child_status in struct thread is updated. When the child thread is created, its status would be set to LOADING. Process_execute() would return the thread id of the thread if the thread is successfully executed. Process_execute() would return -1 if the value of the status is FAILED.



> B8: Consider parent process P with child process C.  How do you
> ensure proper synchronization and avoid race conditions when P
> calls wait(C) before C exits?  After C exits?  How do you ensure
> that all resources are freed in each case?  How about when P
> terminates without waiting, before C exits?  After C exits?  Are
> there any special cases?

We implement concept of semaphores to ensure proper synchronization and avoid race conditions. We maintain child_load_status and child_exit_status. The expected or ideal situation is that the parent process executes wait() before child process exits. Then parent retrieves the child's exit status. If P executes wait() after C exits, then P would search the exit list and see that C already exit. When P terminates without waiting in both before or after C exits, it will be ignored by C and execution will continue.



### Rationale

> B9: Why did you choose to implement access to user memory from the
> kernel in the way that you did?

Validating arguments and status is straightforward.

> B10: What advantages or disadvantages can you see to your design
> for file descriptors?

Advantages:
1. Global view: the design is able to control threads and corresponding attributes from a global view.
2. Space: Thread struct's space is minimized.

Disadvantages:
1. User program may open lots of files to crash kernel.
2. Cost: the cost is a little high.

> B11: The default tid_t to pid_t mapping is the identity mapping.
> If you changed it, what advantages are there to your approach?

We didn't change.


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
