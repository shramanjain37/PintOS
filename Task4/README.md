# Course Project Operating Systems Summer 2024

Welcome to the course project for the
[OS course](https://cms.sic.saarland/os_24/)!

## Documentation

You can find valuable resources for the project in the `doc/` folder. The
documentaiton has the following chapters:

1. [Introduction](doc/1_introduction.md): **Start here**
2. [Reference Guide](doc/2_reference_guide.md): design and inner workings of the
    key Pintos components and the code.
3. [Debugging Tools](doc/3_debugging_tools.md): tools and suggestions for
    debugging Pintos
5. [Project Design Document](doc/5_project_doc.md): example design document for
    your projects
6. [Coding Standards](doc/6_coding_standards.md): suggestions for writing clean
    and easy to read code
7. [Bibliography](doc/8_bibliography.md): pointers to further documentation and
    resources.


## Project Releases

We will release the four milestones step-by-step through the semester.
Currently, this version of the repository contains the following projects:

* [Project 1: Threads, Synchronization, Scheduling](src/threads/README.md)
* [Project 2: User Programs](src/userprog/README.md)
* [Project 3: Virtual Memory](src/vm/README.md)
* [Project 4: File Systems](src/filesys/README.md)

## Group Repositories
We will create repositories in the
[SIC gitlab group for the course](https://gitlab.cs.uni-saarland.de/os/os-24ss).
Please make sure that you log into your SIC gitlab accounts first, so your
account is fully initialized in gitlab.

You are welcome to use these repositories for collaboration within your groups,
create branches etc.,  as you see fit

### GitlabCI / Automatic Tests

If you push a branch that starts with `pX-`, your group repo will be configured
to automatically run tests for the corresponding project (`p1` - `p4`). You can
see the result in your commit status in gitlab or by looking at the CI section
in gitlab for your project.

## Submission

To submit your project, push your code to the `pX-submission` branch. After you
push, gitlab CI will run the automatic tests again, and soon after these results
will show up in CMS for you. (note that the automated scores are contingent on
the TAs not detecting any tampering with the auto-grading.) Part of the score,
for code quality and design document is assigned manually at a later point by
the TAs.

Note that the `pX-submission` branches are configured to not allow force-pushes
or deletion. We will use the last push date for determining on-time submission
and calculating your slip days.

## Getting Help

If you run into problems with Pintos or projects you should consult the
[debugging guide](doc/3_debugging_tools.md), then use the
[forum](https://os-discourse.saarland-informatics-campus.de/) to look for others
who may have the same problem or to ask others about it. If that does not help,
consult the TAs during their [office hours](https://cms.sic.saarland/os_24/).
