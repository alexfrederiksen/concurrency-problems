#include <atomic>
#include <chrono>
#include <list>
#include <thread>
#include <functional>
#include <future>
#include <vector>

#include <cstdio>
#include <cstdlib>

int THREAD_COUNT = 8;
const unsigned long int PRIME_RANGE = 100000000;  // 10^8
const unsigned long int SQRT_PRIME_RANGE = 10000; // 10^4

// for convenience
using std::shared_ptr;
using std::shared_future;
using std::promise;
using std::atomic;

struct iteration_t
{
	long int prime;
	bool halt;

	shared_ptr<atomic<int>> done_count;
	shared_ptr<promise<iteration_t>> next;

	shared_future<iteration_t> next_fut;

	iteration_t() : 
		done_count(new atomic<int>(0)), next(new promise<iteration_t>()),
		next_fut((*next).get_future())
	{
	}
};

void print_rest(bool * is_composite, int start)
{
	if (start % 2 == 0) start++;
	for (int i = start; i < PRIME_RANGE; i += 2)
	{
		if (is_composite[i]) printf("Found new prime: %d\n", i);
	}
}

void sieve(int id, bool * is_composite, iteration_t iteration)
{
	while (!iteration.halt)
	{

		// --- Compute composites indexed by [id] (mod 8) classes --------------
		
		long prime = iteration.prime;
		for (long i = prime + id * prime; i < PRIME_RANGE; i += THREAD_COUNT * prime)
		{
			is_composite[i] = true;
			for (int j = 0; j < 100; j++);
		}

		// --- Synchronize with other threads ----------------------------------

		int done = (*iteration.done_count).fetch_add(1, std::memory_order_relaxed) + 1;

		if (done == THREAD_COUNT)
		{
			// only one thread should reach this state at a time 
			iteration_t new_iter;

			// find next prime
			long prime = iteration.prime == 2 ? 1 : iteration.prime;
			do
			{ 
				prime += 2;
			} while (is_composite[prime]);

			//printf("Found new prime: %d\n", prime);

			new_iter.prime = prime;

			new_iter.halt = prime > SQRT_PRIME_RANGE;

			//if (new_iter.halt) print_rest(is_composite, prime);

			// provide new iteration to all threads
			iteration.next->set_value(new_iter);
			iteration = new_iter;
		} else
		{
			// wait for next iteration
			iteration = iteration.next_fut.get();
		}
	}

	//printf("Thread %d exiting...\n", id);
}

int main(int argc, char ** argv)
{
	if (argc > 1)
		THREAD_COUNT = atoi(argv[1]);

	bool * buffer = new bool[PRIME_RANGE];

	std::fill(buffer, buffer + PRIME_RANGE, false);

	iteration_t first;
	// load in the first prime
	first.prime = 2; 
	first.halt = false;


	std::vector<std::thread> threads;

	printf("Spawning threads...\n");

	for (int i = 0; i < THREAD_COUNT; i++)
	{
		threads.push_back(std::thread(sieve, i, buffer, first));
	}

	for (auto & t : threads) t.join();

	printf("Program done");

	delete [] buffer;

	return 0;
}


