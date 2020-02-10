#include <thread>
#include <future>
#include <vector>
#include <list>
#include <functional>

#include <cmath>
#include <cstdio> // I'm sorry, I really like printf()

typedef unsigned long prime_t;

int THREAD_COUNT = 8;
//const prime_t PRIME_RANGE = 100000000;  // 10^8
const prime_t PRIME_RANGE = 10000000;  // 10^8
const prime_t SQRT_PRIME_RANGE = 10000; // 10^4

struct Job
{
	// apparently thread-safe
	std::list<prime_t> prime_block;
	// shared ptr was necessary because atomics are 
	// non-copy-constructible
	std::shared_ptr<std::atomic<prime_t>> cur;

	prime_t start; 
	prime_t end;

	int id;

	Job() : cur(new std::atomic<prime_t>(0)) { }

};

struct Hive
{
	std::vector<Job> jobs;
};

bool is_prime_hive(prime_t test, Hive & hive)
{
	if (test % 2 == 0) return false;

	prime_t test_end = sqrt(test);

	auto job_it = hive.jobs.begin();
	for (prime_t p = 3; p <= test_end;)
	{
		Job & job = *job_it;
		std::list<prime_t> & block = job.prime_block;

		// use block
		prime_t block_cur = *job.cur;
		for (prime_t p : block)
		{
			if (p > block_cur || p >= test_end) break;
			if (test % p == 0) return false;
		}
		p = block_cur;

		if (p >= test_end) break;

		// fill in to next block (if there is one)
		auto old_it = job_it;
		prime_t stop = job_it != hive.jobs.end() ? (*job_it).start : test_end;
		job_it = old_it;

		if (p % 2 == 0) ++p;
		for (; p < stop; p += 2)
		{
			if (test % p == 0) return false;
		}
	}

	return true;
}

bool is_prime(prime_t test)
{
	if (test % 2 == 0) return false;

	prime_t max = sqrt(test);
	for (prime_t i = 3; i < max; i += 2)
	{
		if (test % i == 0) return false;
	}

	return true;
}

void find_primes(Hive & hive, Job & job) 
{ 

	auto & primes = job.prime_block;

	if (job.start <= 2)
	{
		primes.push_back(2);
		job.start = 3;
	}

	if (job.start % 2 == 0) job.start++;
	for (prime_t t = job.start; t < job.end; t += 2)
	{
		//bool succ = is_prime(t);
		bool succ = is_prime_hive(t, hive);
		if (succ)
		{
			//printf("Thread %d found %d.\n", job.id, t);
			primes.push_back(t);
		}

		// TODO: can be weaker
		*job.cur = t;
	}
}

int main(int argc, char ** argv)
{
	if (argc > 1)
		THREAD_COUNT = atoi(argv[1]);

	std::vector<std::thread> threads;

	Hive hive;
	for (int i = 0; i < THREAD_COUNT; i++)
		hive.jobs.push_back(Job());

	printf("Spawning threads...\n");

	prime_t block_size = PRIME_RANGE / THREAD_COUNT;
	for (int i = 0; i < THREAD_COUNT; i++)
	{
		hive.jobs[i].id = i;
		hive.jobs[i].start = i * block_size;
		hive.jobs[i].end = (i + 1) * block_size;

		*hive.jobs[i].cur = hive.jobs[i].start;

		threads.push_back( std::thread(find_primes, 
					std::ref(hive), std::ref(hive.jobs[i]) ));
	}

	for (auto & t : threads) 
		t.join();

	printf("Program done\n");
	for (Job & j : hive.jobs)
	{
		printf("(%8d - %8d) contains %d primes\n", 
				j.start, j.end, j.prime_block.size());
	}

	return 0;
}

