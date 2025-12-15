# BeatChess Makefile

CXX = g++
CXXFLAGS = -Wall -O2 `pkg-config --cflags gtk+-3.0`
LDFLAGS = `pkg-config --libs gtk+-3.0` -lm -lpthread

TARGET = beatchess
OBJECTS = chess_main.o beatchess.o help.o

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) -o $(TARGET) $(OBJECTS) $(LDFLAGS)

chess_main.o: chess_main.cpp
	$(CXX) $(CXXFLAGS) -c chess_main.cpp

beatchess.o: beatchess.cpp
	$(CXX) $(CXXFLAGS) -c beatchess.cpp

help.o: help.cpp
	$(CXX) $(CXXFLAGS) -c help.cpp

clean:
	rm -f $(OBJECTS) $(TARGET)

rebuild: clean all

.PHONY: all clean rebuild
