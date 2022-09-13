all : GC MA
GC : main.cpp
	g++ -c main.cpp -o ./obj/main.o
MA :
	 make -C timer && make -C http && make -C WebServer  && make -C threadpool && make -C obj 