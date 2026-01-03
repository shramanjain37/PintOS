# Introduction

Welcome to Pintos. Pintos is a simple operating system framework for the
80x86 architecture. It supports kernel threads, loading and
running user programs, and a file system, but it implements all of these
in a very simple way. In the Pintos projects, you and your project team
will strengthen its support in all three of these areas. You will also
add a virtual memory implementation.

Pintos could, theoretically, run on a regular IBM-compatible PC.
Unfortunately, it is impractical to supply every student a dedicated PC
for use with Pintos. Therefore, we will run Pintos projects in a system
simulator, that is, a program that simulates an 80x86 CPU
and its peripheral devices accurately enough that unmodified operating
systems and software can run under it. In class we will use the
[QEMU](http://fabrice.bellard.free.fr/qemu/) simulator.

These projects are hard. They have a reputation of taking a lot of time,
and deservedly so. We will do what we can to reduce the workload, such
as providing a lot of support material, but there is plenty of hard work
that needs to be done. We welcome your feedback. If you have suggestions
on how we can reduce the unnecessary overhead of assignments, cutting
them down to the important underlying issues, please let us know.

This chapter explains how to get started working with Pintos. You should
read the entire chapter before you start work on any of the projects.

## Getting Started

For this class you will need to install
[Docker](https://docs.docker.com/engine/install/),
[VSCode](https://code.visualstudio.com/download),
and the
[Dev Containers](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers) 
extension for VSCode. With this setup you do not have to install any other
dependencies manually, as all required tools will be shipped with the
development container in consistent versions for everyone.

After the following the steps to install the required software,
clone the Pintos git repo:
`https://gitlab.cs.uni-saarland.de/os/os-24ss/pintos.git`

You can either clone through VSCode directly, or clone on the command line,
depending on your preference:

```
$ git clone https://gitlab.cs.uni-saarland.de/os/os-24ss/pintos.git
```

After you have finished extracting, open the folder using VSCode. If
Dev Containers was properly installed, on the bottom left corner of
the VSCode screen you should see a button to open a remote window (><).
Click on that button and select reopen in container. This should
create a container using the dev container configuration file,
that makes running the container environment relatively easy.
After VSCode has finished setting up the container, you can
interact with the container environment using the VSCode terminal.

### Devcontainers CLI Alternative

If you prefer to use the commandline directly you can use the
[Dev Containers CLI](https://github.com/devcontainers/cli). 
This will start and initialize containers, similar
to VSCode. Nonetheless, Dev Containers extension is the officially 
supported tool for this class.

First set up the container by running the following command in the root
directory of the project repo

```
devcontainer up --workspace-folder .
```

To run an interactive shell in the container, run

```
devcontainer exec --workspace-folder . bash
```

You can replace bash with any other commands if you just want to run an
individual command.

### Source Tree Overview

Let's take a look at what's inside. Here's the directory structure
that you should see in "pintos/src":

***threads/***
:   Source code for the base kernel, which you will modify starting in
    project 1.

***userprog/***
:   Source code for the user program loader, which you will modify
    starting with project 2.

***vm/***
:   An almost empty directory. You will implement virtual memory here in
    project 3.

***filesys/***
:   Source code for a basic file system. You will use this file system
    starting with project 2, but you will not modify it until project 4.

***devices/***
:   Source code for I/O device interfacing: keyboard, timer, disk, etc.
    You will modify the timer implementation in project 1. Otherwise you
    should have no need to change this code.

***lib/***
:   An implementation of a subset of the standard C library. The code in
    this directory is compiled into both the Pintos kernel and, starting
    from project 2, user programs that run under it. In both kernel code
    and user programs, headers in this directory can be included using
    the `#include <...>` notation. You should have little need to modify
    this code.

***lib/kernel/***
:   Parts of the C library that are included only in the Pintos kernel.
    This also includes implementations of some data types that you are
    free to use in your kernel code: bitmaps, doubly linked lists, and
    hash tables. In the kernel, headers in this directory can be
    included using the `#include <...>` notation.

***lib/user/***
:   Parts of the C library that are included only in Pintos user
    programs. In user programs, headers in this directory can be
    included using the `#include <...>` notation.

***tests/***
:   Tests for each project. You can modify this code if it helps you
    test your submission, but we will replace it with the originals
    before we run the tests.

***examples/***
:   Example user programs for use starting with project 2.

***utils/***
:   These files may come in handy if you decide to try working with
    Pintos on your own machine. Otherwise, you can ignore them.

### Building Pintos

As the next step, build the source code supplied for the first project.
First, `cd` into the "threads" directory. Then, issue the
`make` command. This will create a "build" directory under
"threads", populate it with a "Makefile" and a few subdirectories, and
then build the kernel inside. The entire build should take less than 30
seconds. The project can also be built with VSCode by selecting the
"Run Build Task" (ctrl + shift + B).

Following the build, the following are the interesting files in the
"build" directory:

***Makefile***
:   A copy of "pintos/src/Makefile.build". It describes how to build the
    kernel. See [Adding Source
    Files](../src/threads/README.md#how-do-i-update-the-makefiles-when-i-add-a-new-source-file),
    for more information.

***kernel.o***
:   Object file for the entire kernel. This is the result of linking
    object files compiled from each individual kernel source file into a
    single object file. It contains debug information, so you can run
    GDB (see section [GDB](3_debugging_tools.md#gdb)) or `backtrace`
    (see section [Backtraces](3_debugging_tools.md#backtraces)) on it.

***kernel.bin***
:   Memory image of the kernel, that is, the exact bytes loaded into
    memory to run the Pintos kernel. This is just "kernel.o" with debug
    information stripped out, which saves a lot of space, which in turn
    keeps the kernel from bumping up against a 512 kB size limit imposed
    by the kernel loader's design.

***loader.bin***
:   Memory image for the kernel loader, a small chunk of code written in
    assembly language that reads the kernel from disk into memory and
    starts it up. It is exactly 512 bytes long, a size fixed by the PC
    BIOS.

Subdirectories of "build" contain object files (".o") and dependency
files (".d"), both produced by the compiler. The dependency files tell
`make` which source files need to be recompiled when other source or
header files are changed.

### Running Pintos

We've supplied a program for conveniently running Pintos in a
simulator, called `pintos`. In the simplest case, you can invoke
`pintos` as `pintos argument...`. Each `argument`{.variable} is passed
to the Pintos kernel for it to act on.

Try it out. First `cd` into the newly created "build" directory. Then
issue the command `pintos run alarm-multiple`, which passes the
arguments `run alarm-multiple` to the Pintos kernel. In these arguments,
`run` instructs the kernel to run a test and `alarm-multiple` is the
test to run.

By default, tests are run with the QEMU simulator and and a new 
window opens that represents the simulated machine's display.
Then Pintos boots and runs the `alarm-multiple` test program, which
outputs a few screenfuls of text. When it's done, you can close QEMU
by pressing Ctrl-c at the terminal prompt. You can also use VSCode
shortcuts to run a test. Press Ctrl + Shift + P to open a list of
editor commands. Select "Tasks: Run Task" and a list should appear,
where you can select to run all tests or one specific test.

(If no window appeared at all, then you're probably logged in remotely
and X forwarding is not set up correctly. In this case, you can fix your
X setup, or you can use the `-v` option to disable X output:
`pintos -v -- run alarm-multiple`.)

You would have probably noticed by now that the text in the simulator 
window is also displayed in the terminal you used to run pintos. 
The first line in the terminal prompt looks similar to:

```
$ pintos run alarm-multiple
qemu -hda /tmp/kvS0tEGvzB.dsk -m 4 -net none -serial stdio
```

This indicates that Pintos sends all output to stdio and by default
qemu's serial port is connected to stdio. You can log serial output 
to a file by redirecting at the command line as:

```
$ pintos run alarm-multiple > logfile.
```

The `pintos` program offers several options for configuring the
simulator or the virtual hardware. If you specify any options, they must
precede the commands passed to the Pintos kernel and be separated from
them by `--`, so that the whole command looks like
`pintos option... -- argument...`. Invoke `pintos` without any arguments
to see a list of available options. Options can select a simulator to
use: the default is QEMU. You can
run the simulator with a debugger (see section [GDB](3_debugging_tools.md#gdb)). 
You can set the amount of memory to give
the VM. Finally, you can select how you want VM output to be displayed:
use `-v` to turn off the VGA display, or `-s` to 
suppress serial input from `stdin`
and output to `stdout`.

The Pintos kernel has commands and options other than `run`. These are
not very interesting for now, but you can see a list of them using
`-h`, e.g. `pintos -h`.

## Grading

For most problems, we will grade your assignments primarily based on test
results, but we will also evaluate the quality of your design decisions.
However, for assignment 1, the second problem will also be graded based on code
inspection.



### Testing

Your test result grade will be based on our tests. Each project has
several tests, each of which has a name beginning with "tests". To
completely test your submission, invoke `make check` from the project
"build" directory. This will build and run each test and print a
"pass" or "fail" message for each one. When a test fails,
`make check` also prints some details of the reason for failure. After
running all the tests, `make check` also prints a summary of the test
results. You can also run all tests by choosing "Tasks: Run Task"
from the VSCode editor commands and then selecting "Run all tests". 

You can also run individual tests one at a time. A given test
"t" writes its output to "t.output", then a
script scores the output as "pass" or "fail" and writes the verdict
to "t.result". To run and grade a single test, `make` the
".result" file explicitly from the "build" directory, e.g.
`make tests/threads/alarm-multiple.result`. If `make` says that the test
result is up-to-date, but you want to re-run it anyway, either run
`make clean` or delete the ".output" file by hand. A single test
can be run in VSCode by choosing "Run Test" after selecting
"Run Task" from the editor commands.

By default, each test provides feedback only at completion, not during
its run. If you prefer, you can observe the progress of each test by
specifying `VERBOSE=1` on the `make` command line, as in
`make check VERBOSE=1`. You can also provide arbitrary options to the
`pintos` run by the tests with `PINTOSOPTS='...'`, e.g.
`make check PINTOSOPTS='-s'` to disable serial I/O.

You can also debug a test by using the VSCode debugger. You can hit
F5 and then select which test to run. VSCode will run Pintos, start
and connect the debugger, and pause at the entry into the kernel.
You can then step through the code using the VSCode debugging
functionality.

All of the tests and related files are in "pintos/src/tests". Before we
test your submission, we will replace the contents of that directory by
a pristine, unmodified copy, to ensure that the correct tests are used.
Thus, you can modify some of the tests if that helps in debugging, but
we will run the originals.

All software has bugs, so some of our tests may be flawed. If you think
a test failure is a bug in the test, not a bug in your code, please
point it out. We will look at it and fix it if necessary.

Please don't try to take advantage of our generosity in giving out our
test suite. Your code has to work properly in the general case, not just
for the test cases we supply. For example, it would be unacceptable to
explicitly base the kernel's behavior on the name of the running test
case. Such attempts to side-step the test cases will receive no credit.
If you think your solution may be in a gray area here, please ask us
about it.

### Design

We will judge your design based on the design document and the source
code that you submit. We will read your entire design document and much
of your source code.

Don't forget that design quality, including the design document, is 50%
of your project grade. It is better to spend one or two hours writing a
good design document than it is to spend that time getting the last 5%
of the points for tests and then trying to rush through writing the
design document in the last 15 minutes.

#### Design Document

We provide a design document template for each project. For each
significant part of a project, the template asks questions in four
areas:

**Data Structures**

The instructions for this section are always the same:

    > Copy here the declaration of each new or changed `struct` or
    > `struct` member, global or static variable, `typedef`, or
    > enumeration. Identify the purpose of each in 25 words or less.

The first part is mechanical. Just copy new or modified declarations
into the design document, to highlight for us the actual changes to
data structures. Each declaration should include the comment that
should accompany it in the source code (see below).

We also ask for a very brief description of the purpose of each new
or changed data structure. The limit of 25 words or less is a
guideline intended to save your time and avoid duplication with
later areas.

**Algorithms**

This is where you tell us how your code works, through questions
that probe your understanding of your code. We might not be able to
easily figure it out from the code, because many creative solutions
exist for most OS problems. Help us out a little.

Your answers should be at a level below the high level description
of requirements given in the assignment. We have read the assignment
too, so it is unnecessary to repeat or rephrase what is stated
there. On the other hand, your answers should be at a level above
the low level of the code itself. Don't give a line-by-line
run-down of what your code does. Instead, use your answers to
explain how your code works to implement the requirements.

**Synchronization**

An operating system kernel is a complex, multithreaded program, in
which synchronizing multiple threads can be difficult. This section
asks about how you chose to synchronize this particular type of
activity.

**Rationale**

Whereas the other sections primarily ask "what" and "how," the
rationale section concentrates on "why." This is where we ask you
to justify some design decisions, by explaining why the choices you
made are better than alternatives. You may be able to state these in
terms of time and space complexity, which can be made as rough or
informal arguments (formal language or proofs are unnecessary).

An incomplete, evasive, or non-responsive design document or one that
strays from the template without good reason may be penalized. Incorrect
capitalization, punctuation, spelling, or grammar can also cost points.
See section [Project Documentation](5_project_doc.md#project_documentation),
for a sample design document for a fictitious project.

#### Source Code

Your design will also be judged by looking at your source code. We will
typically look at the differences between the original Pintos source
tree and your submission, based on the output of a command like
`diff -urpb pintos.orig pintos.submitted`. We will try to match up your
description of the design with the code submitted. Important
discrepancies between the description and the actual code will be
penalized, as will be any bugs we find by spot checks.

The most important aspects of source code design are those that
specifically relate to the operating system issues at stake in the
project. For example, the organization of an inode is an important part
of file system design, so in the file system project a poorly designed
inode would lose points. Other issues are much less important. For
example, multiple Pintos design problems call for a "priority queue,"
that is, a dynamic collection from which the minimum (or maximum) item
can quickly be extracted. Fast priority queues can be implemented many
ways, but we do not expect you to build a fancy data structure even if
it might improve performance. Instead, you are welcome to use a linked
list (and Pintos even provides one with convenient functions for sorting
and finding minimums and maximums).

Pintos is written in a consistent style. Make your additions and
modifications in existing Pintos source files blend in, not stick out.
In new source files, adopt the existing Pintos style by preference, but
make your code self-consistent at the very least. There should not be a
patchwork of different styles that makes it obvious that three different
people wrote the code. Use horizontal and vertical white space to make
code readable. Add a brief comment on every structure, structure member,
global or static variable, typedef, enumeration, and function
definition. Update existing comments as you modify code. Don't comment
out or use the preprocessor to ignore blocks of code (instead, remove it
entirely). Use assertions to document key invariants. Decompose code
into functions for clarity. Code that is difficult to understand because
it violates these or other "common sense" software engineering
practices will be penalized.

In the end, remember your audience. Code is written primarily to be read
by humans. It has to be acceptable to the compiler too, but the compiler
doesn't care about how it looks or how well it is written.

## Legal and Ethical Issues

Pintos is distributed under a liberal license that allows free use,
modification, and distribution. Students and others who work on Pintos
own the code that they write and may use it for any purpose. Pintos
comes with NO WARRANTY, not even for MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See `LICENSE`, for details of the license and lack of warranty.

In the context of the OS Course, please respect the spirit
and the letter of the honor code by refraining from reading any homework
solutions available online or elsewhere. Reading the source code for
other operating system kernels, such as Linux or FreeBSD, is allowed,
but do not copy code from them literally. Please cite the code that
inspired your own in your design documentation.

## Acknowledgements

At MPI-SWS/Saarland University we adapted the project from the original Stanford
version to meet our needs.

The Pintos core and this documentation were originally written by Ben
Pfaff <blp@cs.stanford.edu>.

Additional features were contributed by Anthony Romano <chz@vt.edu>.

The GDB macros supplied with Pintos were written by Godmar Back
<gback@cs.vt.edu>, and their documentation is adapted from his work.

The original structure and form of Pintos was inspired by the Nachos
instructional operating system from the University of California,
Berkeley.

The Pintos projects and documentation originated with those designed for
Nachos by current and former CS 140 teaching assistants at Stanford
University, including at least Yu Ping, Greg Hutchins, Kelly Shaw, Paul
Twohey, Sameer Qureshi, and John Rector.

Example code for monitors is from classroom slides originally by
Dawson Engler and updated by Mendel Rosenblum.

## Trivia

Pintos originated as a replacement for Nachos with a similar design.
Since then Pintos has greatly diverged from the Nachos design. Pintos
differs from Nachos in two important ways. First, Pintos runs on real or
simulated 80x86 hardware, but Nachos runs as a process on a
host operating system. Second, Pintos is written in C like most
real-world operating systems, but Nachos is written in C++.

Why the name "Pintos"? First, like nachos, pinto beans are a common
Mexican food. Second, Pintos is small and a "pint" is a small amount.
Third, like drivers of the eponymous car, students are likely to have
trouble with blow-ups.
