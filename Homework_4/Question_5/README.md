# Question 5
5. The Hungry Birds Problem (20 points)
Given are n baby birds and one parent bird. The baby birds eat out of a common dish that initially contains W worms. Each baby bird repeatedly takes a worm, eats it, sleeps for a while, takes another worm, and so on. When the dish is empty, the baby bird who discovers the empty dish chirps loudly to awaken the parent bird. The parent bird flies off, gathers W more worms, puts them in the dish, and then waits for the dish to be empty again. This pattern repeats forever.

Represent the birds as concurrent processes (i.e., an array of "baby bird" processes and a "parent bird" process) and the dish as a concurrent object (a monitor) that can be accessed by at most one bird at a time. 

Develop a monitor (with condition variables) to synchronize the actions of the birds, i.e., develop a monitor that represents the dish. Define the monitor's operations and their implementation. Implement a multithreaded application in Java or C++ to simulate the actions of the birds represented as concurrent threads and the dish represented as the developed monitor. Is your solution fair? Explain in comments in the source code.