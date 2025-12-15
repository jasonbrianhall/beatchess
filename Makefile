# BeatChess Makefile
# Compiles chess_main, beatchess, and help modules

CC = gcc
CXX = g++
CFLAGS = -Wall -O2 `pkg-config --cflags gtk+-3.0`
CXXFLAGS = -Wall -O2 `pkg-config --cflags gtk+-3.0`
LDFLAGS = `pkg-config --libs gtk+-3.0` -lpthread -lm

# Define VERSION (can be overridden on command line)
CXXFLAGS += -DVERSION="1.0"

# Source and object files
SOURCES = chess_main.cpp beatchess.cpp help.cpp
OBJECTS = chess_main.o beatchess.o help.o
TARGET = beatchess

# Default target
all: $(TARGET)

# Link the final executable
$(TARGET): $(OBJECTS)
	$(CXX) -o $@ $^ $(LDFLAGS)

# Compile chess_main.cpp
chess_main.o: chess_main.cpp
	$(CXX) $(CXXFLAGS) -c chess_main.cpp -o $@

# Compile beatchess.cpp
beatchess.o: beatchess.cpp
	$(CXX) $(CXXFLAGS) -c beatchess.cpp -o $@

# Compile help.cpp
help.o: help.cpp
	$(CXX) $(CXXFLAGS) -c help.cpp -o $@

# Clean up object files and executable
clean:
	rm -f $(OBJECTS) $(TARGET)

# Clean and rebuild
rebuild: clean all

# Debug build (with symbols)
debug: CXXFLAGS += -g
debug: $(TARGET)

# Help target
help:
	@echo "BeatChess Makefile"
	@echo "=================="
	@echo "Available targets:"
	@echo "  make            - Build the chess engine"
	@echo "  make clean      - Remove compiled files"
	@echo "  make rebuild    - Clean and rebuild"
	@echo "  make debug      - Build with debug symbols"
	@echo "  make help       - Show this help message"
	@echo ""
	@echo "Options:"
	@echo "  make VERSION=2.0 - Build with custom version number"
	@echo ""
	@echo "Example:"
	@echo "  make VERSION=2.0.0"

.PHONY: all clean rebuild debug help
