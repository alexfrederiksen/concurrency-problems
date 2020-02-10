#include <atomic>
#include <memory>
#include <thread>

#include <cstdio>  // printf()
#include <cstdlib> // rand();
#include <ctime>   // to seed rand();

template <class T>
class Stack
{
	public:

	struct Node
	{
		T val;
		Node * next;
	};


	class NodePool
	{
		int size;
		Node * pool;

		int used;

		public:

		NodePool(int _size)
		{
			used = 0;
			size = _size;
			pool = new Node[size];
		}

		Node * get(int val)
		{
			if (used >= size) return nullptr;

			Node * node = pool + used;
			used++;

			node->val = val;

			return node;
		}

		~NodePool()
		{
			delete [] pool;
		}
	};

	/* descriptor */
	struct Descriptor
	{
		Node * head;
		int size;
	};

	private:

	std::atomic<int> numOps;

	std::shared_ptr<Descriptor> desc;

	public:

	Stack()
	{
		std::shared_ptr<Descriptor> newDesc = std::make_shared<Descriptor>();
		newDesc->head = nullptr;
		newDesc->size = 0;

		std::atomic_store(&desc, newDesc);

		numOps = 0;
	}

	std::shared_ptr<Descriptor> buildDescriptor(Node * head, int size)
	{
		std::shared_ptr<Descriptor> newDesc = std::make_shared<Descriptor>();
		newDesc->head = nullptr;
		newDesc->size = 0;
		return newDesc;
	}

	// pushes raw node (for preallocated nodes)
	bool push(Node * newNode)
	{
		if (newNode == nullptr) return false;

		bool succ;
		do
		{
			std::shared_ptr<Descriptor> curDesc = std::atomic_load(&desc);

			// setup new node
			newNode->next = curDesc->head;

			// build new descriptor
			std::shared_ptr<Descriptor> newDesc = std::make_shared<Descriptor>();
			newDesc->head = newNode;
			newDesc->size = curDesc->size + 1;

			// attempt to swap in new descriptor
			succ = std::atomic_compare_exchange_strong(&desc, &curDesc, newDesc);
		} while (!succ);

		++numOps;
		return true;
	}

	// returns node (don't handle node destruction)
	Node * pop()
	{
		Node * popped;
		bool succ;
		do
		{
			std::shared_ptr<Descriptor> curDesc = std::atomic_load(&desc);

			popped = curDesc->head;

			if (popped == nullptr) 
				return nullptr;

			Node * newHead = popped->next;

			// build new descriptor
			std::shared_ptr<Descriptor> newDesc = std::make_shared<Descriptor>();
			newDesc->head = newHead;
			newDesc->size = curDesc->size - 1;

			succ = std::atomic_compare_exchange_strong(&desc, &curDesc, newDesc);
		} while(!succ);

		++numOps;

		return popped;
	}

	int size()
	{
		std::shared_ptr<Descriptor> curDesc = std::atomic_load(&desc);

		++numOps;
		return curDesc->size;
	}


	int getOpCount()
	{
		return (int) numOps;
	}

	~Stack()
	{
		// we don't manage the nodes
	}
};

const int TEST_OPS = 150'000;

struct Tester
{
	Stack<int> * stack;
	std::thread * life;

	Stack<int>::NodePool pool;

	public:

	Tester() : pool(TEST_OPS)
	{
	}

	void start(Stack<int> * _stack)
	{
		stack = _stack;
		life = new std::thread(&Tester::run, this);
	}

	void run()
	{
		for (int i = 0; i < TEST_OPS; i++)
		{
			int q = std::rand() % 3;
			if (q == 0)
			{
				// push node
				stack->push(pool.get(std::rand()));
			}
			else if (q == 2)
			{
				// pop node
				auto node = stack->pop();
			}
			else
			{
				int size = stack->size();
			}
		}
	}

	void join()
	{
		if (life != nullptr)
			life->join();
	}

	~Tester()
	{
		if (life != nullptr)
			delete life;
	}
};

void populate(Stack<int> & stack, Stack<int>::NodePool & pool)
{
	Stack<int>::Node * node; 
	while ((node = pool.get(std::rand())) != nullptr)
		stack.push(node);
}

void test()
{
	std::srand(std::time(nullptr));

	Stack<int> stack;
	Tester testers[4];

	printf("Pre-Populating...\n");
	Stack<int>::NodePool prepopPool(50'000);
	populate(stack, prepopPool);

	printf("Launching threads...\n");
	for (int i = 0; i < 4; i++)
		testers[i].start(&stack);

	for (int i = 0; i < 4; i++)
		testers[i].join();

	printf("%d operations completed\n", stack.getOpCount());
}

int main()
{
	test();
	return 0;
}
