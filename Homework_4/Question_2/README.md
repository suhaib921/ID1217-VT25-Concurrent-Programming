# Question 2
2. A Repair Station (40 points)
A vehicle repair station can repair vehicles of three types: A, B, and C. The station has the following capacity:

It can repair in parallel at most a, b, and c vehicles of type A, B, and C, correspondingly;
It can repair in parallel at most v vehicles of different types.
If a vehicle cannot be repaired because of any (or both) of the above limitations, the vehicle has to wait until it can get a place to be repaired.

(a) Develop a concurrent object class (a monitor with condition variables) that controls the station so that several vehicles can be repaired in parallel according to the station's capacity as described above. Define the monitor's operations and their implementation. Represent vehicles as processes and show how they can use the monitor. Is your solution fair? Explain (in comments in the source code).

Develop and implement a multithreaded application (in Java or C++) that simulates the actions of the vehicles represented as concurrent threads. Your simulation program should implement all "real world" concurrency in the actions of the vehicles as described in the above scenario. Represent the repair station as a monitor (a synchronized object) containing a set of counters that define available free places for vehicles of different types and the total number of available places. The monitor should control access to the repair station so that several vehicles can be repaired in parallel according to the station's capacity, as described above. Develop and implement the monitor's methods. The vehicle threads call the monitor methods to request and release access to the station to be repaired.

In your simulation program, assume that each vehicle arrives at the station periodically to get repaired. Have the vehicles sleep (pause) for a random amount of time between arriving at the station to simulate the time it takes to travel, and have the vehicles sleep (pause) for a smaller random amount of time to simulate the time it takes to be repaired at the station. Stop the simulation after each vehicle has arrived at the station the given number of times. Your program should print a trace of the interesting events in the program.
Is your solution fair? Explain when presenting homework.