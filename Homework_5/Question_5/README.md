# Question 5
5. The Stable Marriage Problem (40 points)
Let Man[1:n] and Woman[1:n] arrays of processes. Each man ranks the women from 1 to n, and each woman ranks the men from 1 to n. (A ranking is a permutation of integers from 1 to n). A pairing is a one-to-one correspondence between men and women. A pairing is stable if, for two men, Man[i] and   Man[j], and their paired women, Woman[p] and  Woman[q], both of the following conditions are satisfied:

Man[i] ranks Woman[p] higher than Woman[q], or Woman[q] ranks Man[j] higher than Man[i]; and
Man[j] ranks Woman[q] higher than Woman[p], or Woman[p] ranks Man[i] higher than Man[j].
A solution to the stable marriage problem is a set of n pairings, all of which are stable.

Assume that the processes are distributed and can interact by message passing. Develop a distributed peer-to-peer application to solve the stable marriage problem. The men should propose, and the women should listen. A woman has to accept the first proposal she gets because a better one might not come along; however, she can dump (leave) the first man if she later gets a better proposal. [Hint: To terminate, the program needs to count the number of women that have ever accepted a proposal. This could be achieved with an extra counter process or a global variable.] You can implement your distributed application in C using the MPI library or in Java using the socket API or Java RMI. Your program should print a trace of the interesting events as they happen, but do not make the trace too verbose.