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

		Node()
		{
		}

		Node(T _val)
		{
			val = _val;
		}

		~Node()
		{
			//printf("Node destroyed.\n");
		}
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

	private:

	std::atomic<Node*> head;
	std::atomic<int> numOps;

	public:

	Stack()
	{
		head = nullptr;
		numOps = 0;
	}

	// pushes raw node (for preallocated nodes)
	bool push(Node * newNode)
	{
		if (newNode == nullptr) return false;

		bool succ;
		do
		{
			newNode->next = head;
			succ = head.compare_exchange_strong(newNode->next, newNode);
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
			popped = head;

			if (popped == nullptr) 
				return nullptr;

			Node * newHead = popped->next;

			succ = head.compare_exchange_strong(popped, newHead);
		} while(!succ);

		++numOps;
		return popped;
	}

	int getOpCount()
	{
		return (int) numOps;
	}

	~Stack()
	{
		while (pop() != nullptr);
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
			if (std::rand() % 2 == 0)
			{
				// push node
				stack->push(pool.get(std::rand()));
			}
			else
			{
				// pop node
				auto node = stack->pop();
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

	printf("Populating...\n");
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
