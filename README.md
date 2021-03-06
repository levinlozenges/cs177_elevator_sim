Description
------
Simulation made to model a set of elevators servicing passengers between the lobby and the upper floors of a building. Collaborated with fellow student Adam Chao(@thinkaliker) for this project. Made using the CSIM library package in C++.

The program operates through a central control process that wakes up elevators when passengers arrive at an elevator and press appropriate UP or DOWN buttons to call an elevator.

The control wakes up(or checks at every simulated second) whether or not a passenger has pushed an elevator call button. The control then determines which elevator would be closest to service said passenger, then wakes up said elevator to service the passenger.

Elevator processes are created at the start of the simulation, but wait until the control signals them to wake up. When woken, the elevator retrieves specific information set by the control to identify where the passenger is and which direction it is heading.

The elevator then transititons into a loop to travel to the passenger, taking simulated time to do so. When it arrives at the passenger, it waits for any and all passengers to board that are heading in the same direction. 

Once a passenger gets into the elevator, they will push the button that corresponds to the floor they wish to get off on. The elevator will take only passengers that want to go in a similar direction to the first passenger accepted, and will not take in passengers if it is full.

The elevator then travels either UP or DOWN, picking up additional passengers(if any) in subsequent floors that they drop passengers off on(note that these passengers would have to be going in a similar direction to the passengers already on-board). The elevator does not stop until all passengers have been dropped off at their appropriate destination.

If an elevator's passenger count hits a negative number, or if the elevator has reached the top/lobby of the building and still has passengers on board, the simulation will stop.

After the elevator services its passengers, it will signal the control that it has finished and it is available to accept another request. If there are no requests available, the elevator will wait at the floor it stopped on.

Passengers are generated using an exponential distribution in groups of 1-5.

If 5 seconds have passed and there are no requests to call an elevator, the control will signal the available elevators to realign themselves at n/NUM_floors intervals between floors. This is done to better service passengers in order to cut the time it takes to travel to the passenger first before carrying them to their intended destination.

Instructions
-----
Compile using "make". Note that CSIM library packages are required and will need to be directed towards the main.cpp file in order for it to compile.

Change the value of identifier NUM_floors to set the amount of floors to be used in the simulation.

Change the value of identifier NUM_elevators to set the amount of elevators to be used in the simulation.

Change the value of identifier NUM_space to set the capacity of an elevator.

CSIM will create and output a log file at the end of the simulation; specific logs for each stage of the boarding process will be output before then.

Bugs
-----
The program is not fool-proof, and tends to break when the floors are set to values higher than 20.

The elevators rarely take on ghost passengers or duplicates, which cause the program to preemptively quit since said ghost passenger will not get off the elevator due to a lack of a corporeal form.
