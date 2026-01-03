# Question 3
3. Compute pi  (20 points)
The points on a unit circle centered at the origin are defined by the function f(x) = sqrt(1-x2).
Recall that the area of a circle is pi*r2, where r is the radius. The adaptive quadrature routine
described in Lecture 1 can approximate the pi value, computing the area of the upper-right quadrant
of a unit circle and then multiplying the result by 4. Develop a multithreaded program (in C using
Pthreads or in Java) to compute pi for a given epsilon using a given number of processes (threads)
np which is assumed to be a command-line argument (i.e., it's given). Performance evaluation: 
Measure and print also the execution time of your program using the timesLinks to an external site.
function or the gettimeofdayLinks to an external site. function (see how it is done in matrixSum.c Download matrixSum.c.). 
To calculate the execution time, read the clock after initializing all variables and just before creating the threads. 
Read the clock again after the computation is complete and the worker threads have terminated.