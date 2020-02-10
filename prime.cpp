#include <atomic>
#include <condition_variable>
#include <mutex>
#include <chrono>
#include <list>
#include <thread>
#include <functional>
#include <future>
#include <vector>

#include <cstdio>
#include <cstdlib>

typedef unsigned long int prime_t;

int THREAD_COUNT = 8;

const prime_t PRIME_RANGE = 100000000;  // 10^8
const prime_t SQRT_PRIME_RANGE = 10000; // 10^4

std::atomic<prime_t> last_prime_found(0);

void sieve(int id, bool * is_composite)
{
	bool running = true;
	while (running)
	{
		// --- Find prime to work on -------------------------------------------
		prime_t prime = last_prime_found;
		bool got_it = false;
		do
		{
			if (prime == 0) prime = 2;
			else if (prime == 2) prime = 3;
			else prime += 2;

			if (prime > SQRT_PRIME_RANGE)
			{
				running = false;
				break;
			}

			if (!is_composite[prime])
			{ 
				// we found one, attempt to claim it. Otherwise catchup
				prime_t last = last_prime_found;
				if (last < prime)
					got_it = last_prime_found
						.compare_exchange_strong(last, prime);
				else
					prime = last;
			}
		} while (!got_it);

		if (!running) break;

		// Note: it is possible the prime we have is actually composite (though 
		// unlikely because it would require one of threads to be sufficiently behind)
		// But if this were the case, no harm is done, just extra work.

		// --- Remove all multiples of this prime ------------------------------
		
		for (prime_t i = 2 * prime; i < PRIME_RANGE; i += prime)
		{
			is_composite[i] = true;
		}
	}
}

int main(int argc, char ** argv)
{
	using namespace std::chrono;

	if (argc > 1)
		THREAD_COUNT = atoi(argv[1]);

	bool * buffer = new bool[PRIME_RANGE];

	std::fill(buffer, buffer + PRIME_RANGE, false);

	// threads start at the first prime, which skips 0 and 1, so hardcode
	// them as composites
	buffer[0] = true; 
	buffer[1] = true;

	std::vector<std::thread> threads;

	printf("Spawning threads...\n");

	// mark time
	auto start_time = system_clock::now();

	// --- Run algorithm -------------------------------------------------------

	for (int i = 0; i < THREAD_COUNT; i++)
		threads.push_back(std::thread(sieve, i, buffer));

	for (auto & t : threads) t.join();

	// mark time
	auto stop_time = system_clock::now();

	int time = duration_cast<milliseconds>(stop_time - start_time).count();

	// --- Compute statistics --------------------------------------------------

	long total = 0;
	prime_t sum = 0;

	prime_t top_primes[10];

	for (int i = PRIME_RANGE - 1; i >= 0; i--)
	{
		if (!buffer[i])
		{
			// i is a prime
			sum += i;
			total++;

			if (total <= 10)
			{
				top_primes[10 - total] = i;
			}
		}
	}

	printf("Execution time: %dms\n", time);
	printf("Prime count: %d\n", total);
	printf("Sum of primes: %lu\n", sum);
	printf("Top 10 primes (least to greatest): \n");
	for (int i = 0; i < 10; i++)
		printf("[%d] : %lu\n", i + 1, top_primes[i]);

	delete [] buffer;

	return 0;
}


