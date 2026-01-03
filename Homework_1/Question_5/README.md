# Question 5
5. The Linux diff command (20 points)
Develop a parallel multithreaded program (in C using Pthreads or in Java) that implements a simplified version of the Linux diff command for comparing two text files. The command is invoked as follows:

diff filename1 filename2
The diff command compares corresponding lines in the given files and prints both lines to the standard output if the lines differ. If one of the files is longer than the other, diff prints all extra lines in the longer file to the standard output.