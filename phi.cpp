#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>
#include <condition_variable> 

#include <csignal>
#include <cstdio> // I really like printf()
#include <cmath>

// TODO: encapsulate globals 
int TABLE_SIZE = 10;

const int STARVATION_TIME = 15000; // ms
const int THINKING_TIME =   10;    // ms
const int EATING_TIME =     500;   // ms


// track starved threads (dead)
std::atomic<int> deaths(0);


/* 
 * class for debugging / tracking the sticks 
 * (adds no functionality) 
 */
struct StickTracker
{
	std::atomic<int> state;

	StickTracker() : state(0) { }

	void set_right()
	{
		state.store(1, std::memory_order_relaxed);
	}

	void set_left()
	{
		state.store(-1, std::memory_order_relaxed);
	}

	void drop()
	{
		state.store(0, std::memory_order_relaxed);
	}
};


// ### Chopsticks ##############################################################

/* 
 * implement a truly fair timed mutex 
 */
struct Stick : public std::timed_mutex
{
	// make using time points less painful
	template <class Clock, class Duration>
	using time_point_t = std::chrono::time_point<Clock,Duration>; 

	// spin waiting interval
	const unsigned int SPIN_WAIT_DELTA = 1; // ms

	// priority flags
	const int PRIORITY_NONE  = 0;
	const int PRIORITY_LEFT  = 1;
	const int PRIORITY_RIGHT = 2;

	// current thread with waiting priority
	std::atomic<int> priority;

	// a tracker for debugging sticks, may be removed / disabled on releases
	StickTracker * tracker;

	Stick() : priority(PRIORITY_NONE) { }

	/* attempts to lock in waiting priority */
	bool priority_lock(int given)
	{
		if ((int) priority == given) return true;
		int expected = PRIORITY_NONE;
		return priority.compare_exchange_strong(expected, given);
	}

	void priority_unlock()
	{
		priority = PRIORITY_NONE;
	}

	template <class Clock, class Duration>
	bool pickup(time_point_t<Clock, Duration> & timeout, int given_priority)
	{
		using namespace std::chrono;

		// spin wait (gcc will soon provide a better c++20 alternative using
		// hardware interrupts on atomic types)
		while (!priority_lock(given_priority))
		{
			// wait delta to prevent thread from hoarding cpu cycles
			std::this_thread::sleep_for(milliseconds(SPIN_WAIT_DELTA));

			// fail on timeout
			if (system_clock::now() >= timeout) return false;
		}

		// yay, we have waiting priority, i.e. when the current lock holder
		// is done, we are guaranteed the lock

		// try locking with the rest of the time we have left 
		bool succ = try_lock_until(timeout);

		// unlock priority, if failed to lock, thread should increase timeout.
		// If thread isn't waiting for this lock, then it's unfair to give it
		// a priority over threads that are waiting.
		priority_unlock();

		return succ;
	}

	template <class Clock, class Duration>
	bool pickup_right(time_point_t<Clock, Duration> & timeout)
	{
		bool succ = pickup(timeout, PRIORITY_RIGHT);
		if (succ) tracker->set_left();

		return succ;
	}

	template <class Clock, class Duration>
	bool pickup_left(time_point_t<Clock, Duration> & timeout)
	{
		bool succ = pickup(timeout, PRIORITY_LEFT);
		if (succ) tracker->set_right();

		return succ;
	}

	void drop()
	{
		unlock();
		tracker->drop();
	}


};

// ### Philosophers ############################################################

struct Person
{
	// unique id
	int id;

	// storage for individuals respective shared objects
	Stick * left = nullptr;
	Stick * right = nullptr;

	// hosting thread for the individual
	std::thread * life = nullptr;

	// flag for killing the individual on demand
	std::atomic<bool> running;
	
	/* start the individuals life */
	void simulate()
	{
		running = true;
		life = new std::thread(&Person::run, this);
	}
	
	void eat(int time, int starve)
	{
		using namespace std::chrono;

		// pickup sticks (key part of algorithm)
		auto starve_point = system_clock::now() + milliseconds(starve);

		bool have_left;
		bool have_right;
	
		if (id == 0)
		{
			// I'm the cycle breaking individual, pickup left then right
			have_left = left->pickup_left(starve_point);
			have_right = right->pickup_right(starve_point);
		} else 
		{
			// I'm just a normal individual, pickup right then left
			have_right = right->pickup_right(starve_point);
			have_left = left->pickup_left(starve_point);
		}

		if (have_left && have_right)
		{
			// I can eat for as long as a I need
			std::this_thread::sleep_for(milliseconds(time));
		} else
		{
			// I failed to pickup both sticks in time, I starve
			printf("Number %d has starved and died.\n", id);

			// and die
			running = false;
			deaths++;
		}

		// drop the sticks I'm holding
		if (have_left) left->drop();
		if (have_right) right->drop();
	}

	void run()
	{
		while (running)
		{
			// eat
			eat(EATING_TIME, STARVATION_TIME);

			// and think
			std::this_thread::sleep_for(
					std::chrono::milliseconds(THINKING_TIME));
		}
	}

	void kill()
	{
		running = false;
	}

	void join()
	{
		if (life != nullptr)
		{
			life->join();
		}
	}

	bool is_alive()
	{
		return (bool) running;
	}

	~Person()
	{
		delete life;
	}
};

// ### Function soley for pretty rendering #####################################

// for drawing the circle
const int radius = 20;
const int center_x = 22;
const int center_y = 24;


void plot_point(int x, int y, char c)
{
	// use linux terminal escape codes 
	printf("\033[%d;%df", y, 2 * x);
	printf("%c\n", c);
}

void plot_point_polar(double r, double theta, char c)
{
	int x = (int) round(center_x + r * cos(theta));
	int y = (int) round(center_x + r * sin(theta));
	plot_point(x, y, c);
}

/* draw table to the console using terminal escape code */
void draw_table(Person * people, StickTracker * sticks)
{
	// clear screen
	printf("\033[2J\033[0;0f");
	printf("Deaths: %d  Thinking: %dms  Starving: %dms  Eating: %dms  \n", 
			(int) deaths, THINKING_TIME, STARVATION_TIME, EATING_TIME);

	// draw people
	for (int i = 0; i < TABLE_SIZE; i++)
	{
		double theta = i * (2 * M_PI / TABLE_SIZE);
		plot_point_polar(radius, theta, people[i].is_alive() ? 'O' : 'X');
	}

	double phase = M_PI / TABLE_SIZE; 

	// draw sticks
	for (int i = 0; i < TABLE_SIZE; i++)
	{
		double theta = phase + i * (2 * M_PI / TABLE_SIZE);
		int state = sticks[i].state;
		if (state != 0)
		{
			double new_phase = theta + phase * state;
			double new_radius = radius - 2;

			if ((int) sticks[(i + state) % TABLE_SIZE].state * state < 0)
				plot_point_polar(new_radius, new_phase, ':');
			else
				plot_point_polar(new_radius, new_phase, '.');

		} 
		else 
		{
			plot_point_polar(radius, theta, '/');
		}
	}

	plot_point(center_x, center_y, '#');
}

/* handle control-c in the terminal */
Person * people_handles = nullptr;
void on_exit_signal(int signum)
{
	printf("Killing everyone...\n");

	// kill everyone
	for (int i = 0; i < TABLE_SIZE; i++)
		people_handles[i].kill();


	// join people
	for (int i = 0; i < TABLE_SIZE; i++)
		people_handles[i].join();


	printf("Total of %d people starved.\n", (int) deaths);

	exit(0);
}



int main(int argc, char ** argv)
{
	if (argc >= 2)
	{
		TABLE_SIZE = atoi(argv[1]);
	}

	// TODO: dynamically allocate this
	Stick sticks[TABLE_SIZE];
	StickTracker trackers[TABLE_SIZE];
	Person people[TABLE_SIZE];

	people_handles = people;
	signal(SIGINT, on_exit_signal);

	// distribute sticks
	for (int i = 0; i < TABLE_SIZE; i++)
	{
		people[i].id = i;
		people[i].left = &sticks[(i - 1 + TABLE_SIZE) % TABLE_SIZE];
		people[i].right = &sticks[i];

		sticks[i].tracker = &trackers[i];
	}

	// simulate people
	for (Person & p : people)
		p.simulate();

	for (;;)
	{
		draw_table(people, trackers);
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	
	return 0;
}
