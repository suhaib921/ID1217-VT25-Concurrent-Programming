# Question 4
4. The Linux tee command (20 points)
Develop a parallel multithreaded program (in C using Pthreads or in Java) that implements the Linux tee command, which is invoked as follows:

tee filename
The tee command reads the standard input and writes it to both the standard output and the file filename. Your parallel tee program should have three threads: one for reading standard input, one for writing to standard output, and one for writing to file filename. 