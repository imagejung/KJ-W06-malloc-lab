# KJ-W06-malloc-lab

##1.mm.c
- mm.c 파일 수정을 통한 malloc 메모리 할당 구현
- 파일제목 mm.c 파일만 malloc 관련하여 make 파일통하여 실행됨
- make 이후 ./mdriver 실행시 점수 출력

##2.mm_implicit_firstfit.c
- implicit_firtstfit 방식으로 구현
- 점수 :

##3.mm_implicit_nextfit.c
- implicit_nextfit 방식으로 구현
- 점수 : 

##4.mm_explicit_firstfit.c
- explicit_nextfit 방식으로 구현
- 점수 : 



------
원본 




#####################################################################
# CS:APP Malloc Lab
# Handout files for students
#
# Copyright (c) 2002, R. Bryant and D. O'Hallaron, All rights reserved.
# May not be used, modified, or copied without permission.
#
######################################################################

***********
Main Files:
***********

mm.{c,h}	
	Your solution malloc package. mm.c is the file that you
	will be handing in, and is the only file you should modify.

mdriver.c	
	The malloc driver that tests your mm.c file

short{1,2}-bal.rep
	Two tiny tracefiles to help you get started. 

Makefile	
	Builds the driver

**********************************
Other support files for the driver
**********************************

config.h	Configures the malloc lab driver
fsecs.{c,h}	Wrapper function for the different timer packages
clock.{c,h}	Routines for accessing the Pentium and Alpha cycle counters
fcyc.{c,h}	Timer functions based on cycle counters
ftimer.{c,h}	Timer functions based on interval timers and gettimeofday()
memlib.{c,h}	Models the heap and sbrk function

*******************************
Building and running the driver
*******************************
To build the driver, type "make" to the shell.

To run the driver on a tiny test trace:

	unix> mdriver -V -f short1-bal.rep

The -V option prints out helpful tracing and summary information.

To get a list of the driver flags:

	unix> mdriver -h

