# CSC360

##Operating Systems

[![Language](https://img.shields.io/badge/language-C-brightblue.svg)](https://gcc.gnu.org/)

###Assignment 1: Realistic Shell Interpreter

Navigate to the folder and use the makefile to compile the project. Only guarenteed to run on linux platforms. Also dependent on the Readline.h library.

$ make

$ ./RSI

###Assignment 2: Train Scheduler

Navigate to the folder and use the makefile to compile the project. Only guarenteed to run on linux platforms. Also dependent on the pthread.h library

$ make

$ ./mts

###Assignment 3: File System

Navigate to the folder and use the makefile to compile the project. Only guarenteed to run on linux platforms. 

$ make

Gets information from the superblock about the disk and prints to screen.

$ ./diskinfo [disk img]

Lists the files/directories and accompanying information contained in the given directory.

$ ./disklist [disk img] [directory]

Creates a local copy of the specified file.

$ ./diskget [disk img] [file]

Copies a local file onto the disc in the given subdirectory.

$ ./diskput [disk img] [local file] [directory]
