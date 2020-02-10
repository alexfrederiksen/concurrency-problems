all: prime phi stack

prime : prime.cpp
	g++ prime.cpp -lpthread -o prime

phi : phi.cpp
	g++ phi.cpp -lpthread -o phi

stack : stack.cpp
	g++ stack.cpp -lpthread -g -o stack
