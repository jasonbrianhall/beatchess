CC = gcc
CXX = g++
CFLAGS = -Wall -O2 `pkg-config --cflags gtk+-3.0`
CXXFLAGS = -Wall -O2 `pkg-config --cflags gtk+-3.0`
LDFLAGS = `pkg-config --libs gtk+-3.0` -lpthread -lm

# Targets
all: chess_main

chess_main: chess_main.o beatchess.o
	$(CXX) -o beatchess chess_main.o beatchess.o $(LDFLAGS)

chess_main.o: chess_main.cpp beatchess.h
	$(CXX) $(CXXFLAGS) -c chess_main.cpp

beatchess.o: beatchess.cpp beatchess.h visualization.h
	$(CXX) $(CXXFLAGS) -c beatchess.cpp

clean:
	rm -f *.o beatchess

.PHONY: all clean
