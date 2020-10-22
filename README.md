# File System Checker

## Introduction

This is a basic file system checker written in C language. Its job is to detect errors in a file system image provided to it as a binary. It is not targeted towards any of mainstream in-use filesystems out there (eg ext3/4, FAT etc), but an imaginary filesystem that has many of the important features of real-world file systems. As a result, this checker solves many of the same problems as any other real-world file system checker out there. For details on the types of sanity checks it performs refer to [project requirements](https://github.com/kasliwalr/intro_os_course/tree/master/project_assignments/p5/filesystems-checker) at Prof. Remzi's (U. Wisconsin) github repository. At this time, it has not been thoroughly tested, as for that I would need carefully generated "bad image" test cases, which I did not have, so this code has been run against a single "good image". Therefore, I can say with some confidence that it does not generate false error detections.  

Overall, this is work in progress, and I hope to add more thorough testing in future. Here are the instructions to use it.



## Installation and Demo


1. Clone the repository

```
> git clone https://github.com/kasliwalr/filesystem-checker.git # this will create a filesystem-checker repository folder
```


2. Repository tree

```
> cd filesytem-checker
> tree -L 1 ./
./
├── docs
│   └── README.md
├── LICENSE
├── Makefile
├── README.md
├── src
│   ├── fileio.c
│   ├── fileio.h
│   ├── fs.h
│   └── xcheck.c
├── test
│   └── fs.img
└── TODO.md
```

The above shows the organization of the project. `src` directory contains all the src code (.c, .h files). `xcheck.c` contains the `main()` function. `fileio.h` contains function declartions for helper functions. To easily build the binary, you will need the `Makefile`
You will find sufficient comment in the src code, to understand what is going on. `test` directory contains the good file system image `fs.img`

3. Installing & running

```
> cd filesystem-checker
> ls 
docs  LICENSE  Makefile  README.md  src  test  TODO.md
> make all # builds the executable named "xcheck"
> ls
docs  fileio.o  LICENSE  Makefile  README.md  src  test  TODO.md  xcheck  xcheck.o
> ./xcheck test/fs.img # run file system checker by passing the fs image to it
done # ran successfully without any errors
```

## Summary

This file system checker is targeted towards more traditional file systems such as ext2/ext3 and not the more recent variety of log-structured filesystems. It also does not handle network file systems. This is a work in progress, ultimately, I intend to build a full fledged filesystem checker and recovery tool targted toward one of the mainstream filesystems



