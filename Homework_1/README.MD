# Homework Assignment

## Objectives

In this homework, you will learn how to:

- **Solve the critical section problem**, i.e., access shared variables with mutual exclusion.
- **Program condition synchronization** (waiting for conditions) using condition variables.
- **Build a concurrent (parallel) program** of multiple threads with shared variables, including:
  - Creating and joining concurrent threads.
  - Passing input parameters and results.
- **Synchronize threads** with locks, barriers, and condition variables.
- **Compile and run a concurrent program** using Pthreads (or Java threads and concurrent utilities).

## Reading/Links

- **Lecture 6:** [Introduction to Pthreads](https://canvas.kth.se/courses/52315/pages/6-tutorial-introduction-to-pthreads)
- **Lecture 7:** [Threads, Locks, and Conditions in Java SE JDK](https://canvas.kth.se/courses/52315/pages/7-tutorial-threads-locks-and-conditions-in-java-se-jdk)
- **Tutorial:** [POSIX Threads Programming](https://hpc-tutorials.llnl.gov/posix/)
- **Threads API Manual**(https://pubs.opengroup.org/onlinepubs/9699919799/idx/threads.html)
- **Pthread APIs - IBM:** [Link](https://www.ibm.com/docs/en/i/7.5?topic=category-pthread-apis)
- **Chapter 4:** *Shared-memory programming with Pthreads* in "An Introduction to Parallel Programming" by Peter S. Pacheco and Matthew Malensek. Available online via KTH library.

## Programming Environments

- **Languages and Libraries:**
  - C/C++ with the Pthread library.
  - **Optional:** Java using threads and concurrent utilities, namely the `ReentrantLock`(https://docs.oracle.com/en/java/javase/21/docs/api/java.base/java/util/concurrent/locks/ReentrantLock.html) class and the `Condition`(https://docs.oracle.com/en/java/javase/21/docs/api/java.base/java/util/concurrent/locks/Condition.html) interface from the `java.util.concurrent.locks`(https://docs.oracle.com/en/java/javase/21/docs/api/java.base/java/util/concurrent/locks/package-summary.html) package.
  
- **Related Links:** See "Programming Environments. Related Links"(https://canvas.kth.se/courses/52315/pages/computing-and-programming-environments-related-links).

## Task

There are **7 problems** below, worth the following points:

| Problems | Points |
|----------|--------|
| 1-5      | 20 each|
| 6, 7     | 40 each|

You are to solve any combination of the problems that add up to **40 points**. Read also general requirements for homework, grading, and bonus policies. You may discuss the meaning of questions with classmates, but the work you turn in must be yours alone.

## Problems

### 1. Compute the Sum, Min, and Max of Matrix Elements (20 points)

The purpose of this problem is to introduce you to the Pthreads library. 

- **Download, compile, and run** a program [`matrixSum.c`](Download%20matrixSum.c). The program computes the sum of matrix elements in parallel using Pthreads.
- **Develop the following three modified versions of the program:**
  
  a. **Extend the program** so that in addition to the sum, it finds and prints:
     - The value and position (indexes) of the **maximum element** of the matrix.
     - The value and position (indexes) of the **minimum element** of the matrix.
     
     *To check your solution, initialize matrix elements to random values (rather than all ones like in the above example).*
  
  b. **Modify the program** developed in (a) so that the **main thread** prints the final results. Do not call the Barrier function, and do not use arrays for partial results, such as sums in the above example.
  
  c. **Modify the program** developed in (b) to use a **"bag of tasks"** represented as a row counter, initialized to 0. A worker gets a task (i.e., the number of the row to process) by reading the value of the counter and incrementing the counter as described for the matrix multiplication problem in Slides 27-29 about the "bag of tasks" in Lecture 5(https://canvas.kth.se/courses/52315/pages/5-barriers-flags-data-parallelisms-bag-of-tasks).

### 2. Quicksort (20 points)

The quicksort algorithm sorts a list of numbers by first dividing the list into two sublists so that all numbers in one sublist are smaller than all numbers in the other sublist. This is done by selecting a **pivot** number against which all other numbers are compared: numbers less than the pivot are placed in one sublist, and numbers greater than the pivot are placed in another sublist. The pivot can either be placed in one sublist or withheld and placed in its final position.

**Task:**

Develop a parallel multithreaded program (in C using Pthreads or in Java) with recursive parallelism that implements the quicksort algorithm for sorting an array of *n* values.

**Performance Evaluation:**

- Measure and print the execution time of your program using the `times` function or the `gettimeofday` function (see how it is done in [`matrixSum.c`](Download%20matrixSum.c)).
- To calculate the execution time:
  - Read the clock after initializing all variables and just before creating the threads.
  - Read the clock again after the computation is complete and the worker threads have terminated.

### 3. Compute π (20 points)

The points on a unit circle centered at the origin are defined by the function *f(x) = sqrt(1 - x²)*. Recall that the area of a circle is πr², where *r* is the radius. The adaptive quadrature routine described in Lecture 1 can approximate the value of π by computing the area of the upper-right quadrant of a unit circle and then multiplying the result by 4.

**Task:**

Develop a multithreaded program (in C using Pthreads or in Java) to compute π for a given epsilon using a given number of processes (threads) `np`, which is provided as a command-line argument.

**Performance Evaluation:**

- Measure and print the execution time of your program using the `times` function or the `gettimeofday` function (see how it is done in [`matrixSum.c`](Download%20matrixSum.c)).
- To calculate the execution time:
  - Read the clock after initializing all variables and just before creating the threads.
  - Read the clock again after the computation is complete and the worker threads have terminated.

### 4. The Linux `tee` Command (20 points)

**Task:**

Develop a parallel multithreaded program (in C using Pthreads or in Java) that implements the Linux `tee` command, which is invoked as follows:

```bash
tee filename
