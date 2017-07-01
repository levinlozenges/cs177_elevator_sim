//
// Adam Chao
// Jeffrey Ng
// CS 177 Elevator Project
//

#include <iostream>
#include "cpp.h"
#include <string.h>
#include <vector>
#include <cmath>

using namespace std;

#define NUM_space 15      	// number of space available on elevator
#define TINY 1.e-20      	// a very small time period
#define NUM_floors 8		// # of floors in building
#define NUM_elevators 2		// # of elevators in building

// Facilites for communication between control and elevator
facility_set rest ("rest", NUM_elevators);           // dummy facility indicating an idle elevator
facility_ms elevator_reserve("Elevators in use", NUM_elevators);

// Events for communication between control and elevator
event_set elevator_operator("Elevator Stimuli", NUM_elevators);
event_set elevator_moved("Elevator Floor Change Stimuli", NUM_elevators);

// Mailboxes for each floor; one for each direction and one for sending messages in/sending messages out
mailbox_set mail_in_UP("Elevator Control - UP", NUM_floors);
mailbox_set mail_in_DOWN("Elevator Control - DOWN", NUM_floors);
mailbox_set mail_out_UP("Boarding Passengers going UP", NUM_floors);
mailbox_set mail_out_DOWN("Boarding Passengers going DOWN", NUM_floors);

// Structs for book-keeping relevant elevator-status values
struct elevator_struct {
	long status;		// Status of Elevator #X -> #0 = Sleeping, #1 = In Service
	long location;		// Location of Elevator #X from Lobby - Floor 40
	long direction;		// Elevator currently going UP(1) or DOWN(0)
	long floor_dest;	// Floor Elevator is currently traveling to
	long space_used;	// # of Passengers on Elevator
	
	elevator_struct( long s, long l, long f, long d, long su) : status(s), location(l), floor_dest(f), direction(d), space_used(su) {}
};

struct service_request {
	long elevator_sent;
	long direction;
	long floor;
	
	service_request(long e, long d, long f ) : elevator_sent(e), direction(d),  floor(f) {}
};

// Storage containers to hold elevators/requests
vector<elevator_struct> elevator_list;
vector<service_request> requests;

// Storage containers to hold passenger requests
int want_off[NUM_elevators][NUM_floors];
int want_up[NUM_floors];
int want_dn[NUM_floors];

void make_passengers(long whereami);      // passenger generator
vector<string> places;
long group_size();

void passenger(long start_pos, long end_pos, long passenger_type);    // passenger process
string people[3] = {"in","out", "inter"}; // who was generated

void elevator_control(); 	// The control model to direct elevator activity
long send_elevator(long floor, long direction); //Called by control model to send an elevator to a floor
void elevator(int index);            // The elevator model; up to NUM_elevators
void elevator_loop(long index); // Take passengers to floors within the building
void load_elevator(long floor, long & space_used, long index, long direction); // Load passengers in from selected floors

qtable elevator_occ("elevator occupancy");  // time average of how full is the elevator

extern "C" void sim()      // main process
{
	create("sim");
	elevator_occ.add_histogram(NUM_space+1,0,NUM_space);
	
	max_processes(10000);
	max_messages(1000000);
	
	cout << "Initialize Starting Values..." << endl;
	for(int i = 0; i < NUM_floors; i++) {
		for(int j = 0; j < NUM_elevators; j++)
		{
			want_off[j][i] = 0;
		}
		places.push_back(to_string(i));
		want_up[i] = 0;
		want_dn[i] = 0;
	}
	
	// Generate more elevators(if any). First elevator is always at the lobby.
	elevator_struct *elevator_one = new elevator_struct(0, 0, -1, -1, 0);
	elevator_list.push_back(*elevator_one);
	for(int i = 1; i < NUM_elevators; i++) {
		elevator_struct *elevator_temp = new elevator_struct(0, (long)(uniform( (NUM_floors/2), NUM_floors) + 0.5), -1, -1, 0);
		elevator_list.push_back(*elevator_temp);
	}

	cout << "Generating passengers..." << endl;
	make_passengers(0);  // generate a stream of incoming
	make_passengers(1);  // generate a stream of outgoing
	make_passengers(2);  // generate a stream of interfloor

	cout << "Starting up elevator control..." << endl;
	elevator_control();
	for(int i = 0; i < NUM_elevators; i++) {
		elevator(i);
	}
	//hold(100);
	hold (86400);              // wait for a whole day (in seconds) to pass

	cout << "End of scenario!" << endl;
	report();
	report_classes();
	//add_histogram();
	status_facilities();
}

// Model segment 1: generate groups of new passengers at specified location
void make_passengers(long passenger_type)
{
	long start_pos = -1;
	long end_pos = -1;
	const char* myName=places[passenger_type].c_str(); // hack because CSIM wants a char*
	create(myName);

	while(clock < 86400) {       // run for one day (in minutes)
		hold(exponential(150));           // exponential interarrivals, mean 15 minutes
		//hold(900);
		long group = group_size();
		for (long i=0;i<group;i++) { // create each member of the group
			if(passenger_type == 0) { // Incoming: Call from upper floor to go to lobby
				//cout << "!Passenger: Creating group of Incoming at SIZE: " << group << endl;
				start_pos = (long)(uniform(1, NUM_floors));
				end_pos = 0;
			}
			if(passenger_type == 1) { // Outgoing: Call from lobby to go to upper floor
				//cout << "!Passenger: Creating group of Outgoing at SIZE: " << group << endl;
				start_pos = 0;
				end_pos = (long)(uniform(1, NUM_floors));
			}
			if(passenger_type == 2) { // Interfloor: Call from some upper floor to another upper floor
				//cout << "!Passenger: Creating group of Interfloor at SIZE: " << group << endl;
				start_pos = (long)(uniform(1, NUM_floors));
				end_pos = (long)(uniform(1, NUM_floors));
				while(start_pos == end_pos) {
					end_pos = (long)(uniform(1, NUM_floors));
				}
			}
			passenger(start_pos, end_pos, passenger_type);      // new passenger appears at this location
		}
	}
}

// Model segment 2: activities followed by an individual passenger
void passenger(long start_pos, long end_pos, long passenger_type)
{
	const char* myName=places[passenger_type].c_str(); // hack because CSIM wants a char*
	create(myName);
	cout << "Passenger: !!! Passenger created with type: " << passenger_type << endl;
	cout << "Passenger: start_pos: " << start_pos << " | end_pos: " << end_pos << endl;

	long on_which_elevator = -1;

	if(start_pos < end_pos) { // Going up
		want_up[start_pos]++;
		mail_in_UP[start_pos].send(end_pos);
		cout << "Passenger: pressing button at floor to go UP: " << start_pos << endl;
		cout << "Passenger: Waiting for elevator..." << endl;
		mail_out_UP[start_pos].receive(&on_which_elevator);
	}
	else { // Going down
		want_dn[start_pos]++;
		mail_in_DOWN[start_pos].send(end_pos);
		cout << "Passenger: pressing button at floor to go DOWN: " << start_pos << endl;
		cout << "Passenger: Waiting for elevator..." << endl;
		mail_out_DOWN[start_pos].receive(&on_which_elevator);
	}
	
	if(elevator_list[on_which_elevator].location != start_pos) {
		cout << "Sanity check: the elevator isn't on the same floor I'm on!" << endl;
		exit(1);
	}
	cout << "Passenger: Elevator arrived to pick up passenger at floor: " << start_pos << endl;
	cout << "Passenger: On elevator: " << on_which_elevator << endl;
	cout << "Passenger: Dest_floor: " << end_pos <<  endl;
	
	//On elevator; check when the elevator stops if current floor is passenger destination.
	while(on_which_elevator >= 0) {
		elevator_moved[on_which_elevator].wait();
		cout << "Passenger: Elevator \"" << on_which_elevator << "\" stopped. Comparing floors:" << endl;
		cout << "Passenger:		My stop: " << end_pos << endl;
		cout << "Passenger:		Current floor: " << elevator_list[on_which_elevator].location << endl;
		if(elevator_list[on_which_elevator].location == end_pos) {
			cout << "Pasenger: I'm getting off here." << endl;
			on_which_elevator = -1;
			break; // Get off elevator
		}
	}
}

void elevator_control() {
 /* The elevator control manages the available elevators and responds to
  * passenger requests to get to different floors in the building.
  * 
  * The control listens/checks for passenger requests by checking
  * want_up/want_dn at regular intervals(every second).
  * 
  * When a request is detected, the control calls send_elevator to send
  * the nearest elevator to service that passenger request. It also logs
  * said request as a service_req struct.
  * 
  * The control does not update want_up/want_dn until the passengers are
  * confirmed to be on the elevator. Thus, the requests are used to prevent
  * the elevator from sending the same elevator/different elevator to
  * that floor.
  * 
  * The control also checks for a vareity of different scenarios to prevent
  * errors and avoid elevator hang.
  * 
  * Every 5 seconds of no passenger activity, the control will command
  * all of the elevators to reorganize themselves to better serve passengers
  * by being in locations where the distance to the passenger is roughly
  * even across all other nearby locations.
  */
  create ("elevator_control");
  cout << "Elevator Control initalized!" << endl;
  int idle_count = 0;
  bool already_organized = false;
  while(1) {  // loop forever
		// Each second, check for incoming passenger requests
		bool passengers_present = 0;
		long floor_with_request = -1;
		long direction = -1;
		long elevator_sent = -1;
		for(int i = 0; i < NUM_floors; i++) {
			if(want_up[i] > 0) {
				cout << "Control: Request in want_up[" << i << "]." << endl;
				passengers_present = 1;
				floor_with_request = i;
				direction = 0;
				break;
			}
			else if(want_dn[i] > 0) {
				cout << "Control: Request in want_dn[" << i << "]." << endl;
				passengers_present = 1;
				floor_with_request = i;
				direction = 1;
				break;
			}
			else {
				continue;
			}
		}
		
		if(passengers_present) {
			idle_count = 0;
			// Check if elevators are available - if not, wait until one is.
			while(elevator_reserve.status() != FREE) {
				hold(1);
			}
			// Check if the request has already been filed.
			bool request_present = 0;
			for(int i = 0; i < requests.size(); i++) {
				// Request is filed if the floor/direction match the current request.
				if(requests[i].floor == floor_with_request && requests[i].direction == direction) {
					request_present = 1;
				}
			}
			if(!request_present) {
				cout << "Control: Sending closest elevator to: " << floor_with_request << endl;
				// Send closest elevator to service the request
				elevator_sent = send_elevator(floor_with_request, direction);
				if(elevator_sent != -1) {
					elevator_operator[elevator_sent].set();
					// Add to list of serviced requests
					service_request *req = new service_request(elevator_sent, direction, floor_with_request);
					requests.push_back(*req);
					hold(TINY); // Let elevator process the event
					already_organized = false;
				}
				else { // Error Case: 2 Requests
					cout << "Control: 2 Requests caught " << endl;
					exit(1);
				}

			}
			else {
				cout << "Control: Request already present. Wait." << endl;
				// Request already filled. Wait for an elevator to move again before checking OR 5 secs
				elevator_moved.timed_wait_any(5);
				cout << "Control: Elevator finished! Taking control again." << endl;
			}
		}
		else {
			cout << "Control: No passengers present. Clock: " << clock << endl;
			// Check again in a second
			hold(1);
			
			idle_count++;
			// Check if the control has had no requests in 5 seconds
			if (idle_count >= 5 && !already_organized) {
				bool idle_check = true;
				// Check if all elevators are not busy
				for (int i = 0; i < NUM_elevators; ++i)
				{
					if (elevator_list[i].status) {
						idle_check = false;
						//cout << "DBG elevator " << i << " status" << endl;
					}
				}
				//cout << "DBG idle_check " << idle_check << endl;
				
				// Reorganize elevator positions to service passengers fairly
				if(idle_check) {
					for(int i = 0; i < NUM_elevators; i++) {
						double newfloor = (double(i+1)/(double(NUM_elevators)+1)) * double(NUM_floors);
						elevator_list[i].location = int(newfloor);
						cout << "Control: Setting elevator " << i << " to floor " << int(newfloor) << endl;
					}
				}
				idle_count = 0;
				already_organized = true;
			}
		}
	}
}

long send_elevator(long floor, long direction) {
 /* This function holds the functionality to finalize passenger requests
  * and send them out to an available elevator. Only the closest elevator
  * will be sent to the passenger, unless no other elevator is available
  * for comparison.
  */	
	long closest_elevator_index = -1;
	long closest_elevator_dist = 999;
	for(int j = 0; j < NUM_elevators; j++) {
		// Check if elevators are busy. Skip busy elevators.
		// Calculate closeset elevator to send by checking the distance of available
		// elevators to the intended floor.
		cout << "Status of elevator: " << j << " is equal to "  << elevator_list[j].status << endl;
		if(elevator_list[j].status == 1) {
			continue;
		}
		else {
			// Check which elevator is not busy and closest
			int dist = abs(floor - elevator_list[j].location);
			if(dist < closest_elevator_dist) {
				closest_elevator_index = j;
				closest_elevator_dist = dist;
				//cout << "checkallplaces: index: " << closest_elevator_index << endl;
				//cout << "checkallplaces: Distance: " << closest_elevator_dist << endl;
			}
		}
	}
	// No elevator is actually available if index == -1 > SHOUDLNT HAPPEN
	if(closest_elevator_index != -1) {
		if(direction == 0) { // Handle UP cases
			want_up[floor] = 0; // Clear all instances of passengers waiting at this floor since elevator will service them all
			elevator_list[closest_elevator_index].status = 1;
			elevator_list[closest_elevator_index].direction = 0;
			elevator_list[closest_elevator_index].floor_dest = floor;
			elevator_operator[closest_elevator_index].set();
			cout << "send_elevator: Set elevator going UP:	" << closest_elevator_index << " active for floor: " << floor << endl;
			return closest_elevator_index;
		}
		else { //Handle DOWN cases
			want_dn[floor] = 0; // Clear all instances of passengers waiting at this floor since elevator will service them all
			elevator_list[closest_elevator_index].status = 1;
			elevator_list[closest_elevator_index].direction = 1;
			elevator_list[closest_elevator_index].floor_dest = floor;
			elevator_operator[closest_elevator_index].set();
			cout << "send_elevator: Set elevator going DOWN:	" << closest_elevator_index << " active for floor: " << floor << endl;
			return closest_elevator_index;
		}
	}
	else { //More than 1 request, but unable to service it.
		cout << "send_elevator: ERROR: No elevator deemed available. STOP." << endl;
		return -1;
	}
}

// Model segment 3: the elevator elevator
void elevator(int index) {
  create ("elevator");
  cout << "Elevator: " << index << " initalized!" << endl;
  while(1) {  // loop forever
		// start off in idle state, waiting for the first call...
		rest[index].reserve();                   // relax at garage till called from somewhere
		cout << "Elevator: " << index << " resting..." << endl;
		elevator_operator[index].wait(); // Wait for elevator control to notify me to go somewhere
		rest[index].release();                   // and back to work we go!
		
		cout << "Elevator: " << index << " in service." << endl;
		elevator_list[index].space_used = 0;              // elevator is initially empty
		elevator_occ.note_value(elevator_list[index].space_used);
		
		elevator_reserve.reserve();
		elevator_loop((long)index);
		cout << "Elevator: " << index << " finished task! Awaiting further orders..." << endl;
		elevator_list[index].status = 0;
		// Remove request associated with job - There should really only be up to NUM_elevator requests in the job at all times
		for(int i = 0; i < requests.size(); i++) {
			if(requests[i].elevator_sent == index) {
				cout << "Elevator: " << index << " Removing Request: " << endl;
				cout << "		-Floor: " << requests[i].floor << endl;
				cout << "		-Direction: " << requests[i].direction << endl;
				requests.erase(requests.begin()+i);
				break;
			}
		}
		elevator_operator[index].clear();
		elevator_reserve.release();
	}
}

long group_size() {  // calculates the number of passengers in a group
  double x = prob();
  if (x < 0.3) return 1;
  else {
    if (x < 0.7) return 2;
    else return 5;
  }
}

void elevator_loop(long index) { 
 /* One elevator wakes up and services passengers; Elevator must first
  * travel to the location of the request.
  * Elevator picks up passengers present on current floor; updating the
  * values in want_up/want_dn, and want_off to reflect the amount of
  * passengers boarded.
  * 
  * space_used sets the limit of how many passengers can enter the
  * elevator at a time. If the elevator is full, passengers will continue
  * to wait for an elevator to service them.
  * 
  * From there, the elevator travels in one direction, dropping passengers
  * off at their corresponding destination floors. The passengers are
  * notified when an elevator reaches a new floor by the event elevator_moved.
  * 
  * Once there are 0 passengers on the elevator, the elevator stops and
  * signals the control that it has finished its assigned job.
  */
	
	cout << "		Loop \"" << index << "\": Travel to Passenger" << endl;
	cout << "		Loop \"" << index << "\": Floor_dest: " << elevator_list[index].floor_dest << endl;
	cout << "		Loop \"" << index << "\":Elevator location: " << elevator_list[index].location << endl;
	
	// Elevator travels UP to reach passenger
	if(elevator_list[index].floor_dest >= elevator_list[index].location) {
		long tempA = elevator_list[index].location;
		long tempB = elevator_list[index].floor_dest;
		// Represent time to travel from floor x to floor y
		hold(5 * sqrt(abs(tempB - tempA)) );
		for(int i = elevator_list[index].location; i <= elevator_list[index].floor_dest; i++) {
			//cout << "	-On floor: " << i << endl;
			hold(TINY);
			elevator_list[index].location = i;
			if(i == elevator_list[index].floor_dest) {
				cout << "		Loop \"" << index << "\": At floor: " << i << endl;
				cout << "		Loop \"" << index << "\": Picking up passenger." << endl;
				load_elevator(elevator_list[index].location, elevator_list[index].space_used, index, elevator_list[index].direction);
				if(elevator_list[index].space_used != 0) {
					elevator_occ.note_value(elevator_list[index].space_used);
				}
				// Time spent loading passengers
				hold(uniform(5, 15));
			}
		}

	}
	// Elevator travels DOWN to reach passenger
	else { 
		long tempA = elevator_list[index].location;
		long tempB = elevator_list[index].floor_dest;
		// Represent time to travel from floor x to floor y
		hold(5 * sqrt(abs(tempB - tempA)) );
		for(int i = elevator_list[index].location; i >= elevator_list[index].floor_dest; i--) {
			//cout << "	-On floor: " << i << endl;
			hold(TINY);
			elevator_list[index].location = i;
			if(i == elevator_list[index].floor_dest) {
				cout << "		Loop \"" << index << "\": At floor: " << i << endl;
				cout << "		Loop \"" << index << "\": Picking up passenger." << endl;
				load_elevator(elevator_list[index].location, elevator_list[index].space_used, index, elevator_list[index].direction);
				if(elevator_list[index].space_used != 0) {
					elevator_occ.note_value(elevator_list[index].space_used);
				}
				// Time spent loading passengers
				hold(uniform(5, 15));
			}
		}
	}
	
	// Elevator heads UP to passenger destinations
	if(elevator_list[index].direction == 0) {
		for(int i = elevator_list[index].location; i < NUM_floors; i++) {
			elevator_list[index].location = i;
			//cout << "On floor: " << i << endl;
			if(want_off[index][i] > 0) {
				cout << "		Loop \"" << index << "\": At floor: " << i << endl;
				cout << "		Loop \"" << index << "\": Floor has passengers to drop off at." << endl;
				elevator_moved[index].set();
				cout << "		Loop \"" << index << "\": Notifying passengers.." << endl;
				hold(uniform(5, 15));
				elevator_list[index].space_used -= want_off[index][i];
				// Represent time spent unloading passengers
				want_off[index][i] = 0;
				cout << "		Loop \"" << index << "\": Passengers left the elevator." << endl;
				cout << "		Loop \"" << index << "\": 	-space_used: " << elevator_list[index].space_used << endl;
				
				// Check if passengers are still on board to keep going
				if(elevator_list[index].space_used != 0) {
					elevator_occ.note_value(elevator_list[index].space_used);
				}
				if(elevator_list[index].space_used > 0) {
					cout << "		Loop \"" << index << "\": Passengers still on board. Continuing in same direction." << endl;
					continue;
				}
				else {
					cout << "		Loop \"" << index << "\": No Passengers on board. Stopping." << endl;
					break;
				}
			}
		}
	}
	// Elevator heads DOWN to passenger destinations
	else if(elevator_list[index].direction == 1) {	
		for(int i = elevator_list[index].location; i >= 0; i--) {
			elevator_list[index].location = i;
			//cout << "On floor: " << i << endl;
			if(want_off[index][i] > 0) {
				cout << "		Loop \"" << index << "\": At floor: " << i << endl;
				cout << "		Loop \"" << index << "\": Floor has passengers to drop off at." << endl;
				elevator_moved[index].set();
				cout << "		Loop \"" << index << "\": Notifying passengers.." << endl;
				// Represent time spent unloading passengers
				hold(uniform(5, 15));
				elevator_list[index].space_used -= want_off[index][i];
				want_off[index][i] = 0;
				cout << "		Loop \"" << index << "\": Passengers left the elevator." << endl;
				cout << "		Loop \"" << index << "\": 	-space_used: " << elevator_list[index].space_used << endl;
				// Check if passengers are still on board to keep going
				if(elevator_list[index].space_used != 0) {
					elevator_occ.note_value(elevator_list[index].space_used);
				}
				if(elevator_list[index].space_used > 0) {
					cout << "		Loop \"" << index << "\": Passengers still on board. Continuing in same direction." << endl;
					continue;
				}
				else {
					cout << "		Loop \"" << index << "\": No Passengers on board. Stopping." << endl;
					break;
				}
			}
		}
	}
	// Error: Not telling elevator which direction to head in
	else { 
		cerr << "Error: Elevator loop";
		exit(1);
	}
	cout << "		Loop \"" << index << "\": Loop Finished." << endl;
}

void load_elevator(long floor, long & space_used,  long index, long direction) {
 /* This function is called when the elevator arrives at a floor a set
  * of passengers are waiting on in order to pick them up.
  * 
  * The function checks for requests sent in by the passengers on that
  * specific floor's mailbox. From there, it sends mail back on the floor's
  * other mailbox to acknowledge the passenger's board request. The passenger
  * "gets" on, and the control updates relevant array values with space_used,
  * want_up/want_dn, and want_off in order for the elevator to know where it
  * will be traveling to next.
  * 
  * The elevator will not load in more than it's maximum capacity, dictated
  * by NUM_space. Passengers that don't get picked up due to the capacity limit
  * will wait for the next elevator to service them.
  */ 
  
	// invite passengers to enter, one at a time, until all space are full
	if(direction == 0) { // Grab data from the UP mailboxes
		cout << "			Load \"" << index << "\": Init UP" << endl;
		cout << "			Load \"" << index << "\": Messages in mailbox: " << mail_in_UP[floor].msg_cnt() << endl;
		
		// Let all the passengers that are going up come in
		while(mail_in_UP[floor].msg_cnt() > 0 && space_used <= NUM_space) {
			// Recieve the floor the passenger wants to go to
			long dest_floor;
			mail_in_UP[floor].receive(&dest_floor);
			// Let passenger onboard
			mail_out_UP[floor].send(index);
			cout << "			Load UP \"" << index << "\": Sent request to board elevator. " << endl;
			cout << "			Load UP \"" << index << "\": Passenger boarded." << endl;
			space_used++;
			want_up[floor]--;
			want_off[index][dest_floor]++;
			cout << "			Load UP \"" << index << "\": 	-space_used: " << space_used << endl;
			hold(TINY);  // let next passenger on board
		}
	}
	else if(direction == 1) {	// Grab data from the DOWN mailboxes
		cout << "			Load \"" << index << "\": Init DOWN" << endl;
		cout << "			Load \"" << index << "\": Messages in mailbox: " << mail_in_DOWN[floor].msg_cnt() << endl;
		while(mail_in_DOWN[floor].msg_cnt() > 0 && space_used <= NUM_space) {
			// Recieve the floor the passenger wants to go to
			long dest_floor;
			mail_in_DOWN[floor].receive(&dest_floor);
			// Let passenger onboard
			mail_out_DOWN[floor].send(index);
			cout << "			Load DOWN \"" << index << "\": Sent request to board elevator. " << endl;
			cout << "			Load DOWN \"" << index << "\": Passenger boarded." << endl;
			space_used++;
			want_dn[floor]--;
			want_off[index][dest_floor]++;
			cout << "			Load DOWN \"" << index << "\": 	-space_used: " << space_used << endl;
			hold(TINY);  // let next passenger on board
		}
	}
	else // No UP or DOWN direction - shouldn't ever happen
	{
		cerr << "Load: Error" << endl;
		exit(1);
	}
}
