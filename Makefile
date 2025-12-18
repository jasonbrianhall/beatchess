# BeatChess Makefile - Fixed and Improved Version
# GTK+ 3.0 Chess application with integrated shared AI module

CXX = g++
CXXFLAGS = -Wall -O2 -std=c++11 `pkg-config --cflags gtk+-3.0`
LDFLAGS = `pkg-config --libs gtk+-3.0` -lm -lpthread

# Target executable
TARGET = beatchess

# Source files
SOURCES = chess_main.cpp beatchess.cpp help.cpp beatchess_draw.cpp chess_ai_move.cpp

# Object files (automatically derived from sources)
OBJECTS = $(SOURCES:.cpp=.o)

# Default target
all: $(TARGET)

# Link target
$(TARGET): $(OBJECTS)
	@echo "Linking $(TARGET)..."
	$(CXX) -o $(TARGET) $(OBJECTS) $(LDFLAGS)
	@echo "✅ Build successful: $(TARGET)"

# Generic compilation rule for all .cpp files
%.o: %.cpp
	@echo "Compiling $<..."
	$(CXX) $(CXXFLAGS) -c $< -o $@



# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	rm -f $(OBJECTS) $(TARGET)
	@echo "✅ Clean complete"

# Clean and rebuild
rebuild: clean all

# Show help
help:
	@echo "BeatChess Makefile - Available targets:"
	@echo "  make              - Build BeatChess (default)"
	@echo "  make rebuild      - Clean and rebuild"
	@echo "  make clean        - Remove object files and executable"
	@echo "  make help         - Show this help message"
	@echo ""
	@echo "Build configuration:"
	@echo "  CXX:              $(CXX)"
	@echo "  CXXFLAGS:         $(CXXFLAGS)"
	@echo "  LDFLAGS:          $(LDFLAGS)"
	@echo "  TARGET:           $(TARGET)"
	@echo ""
	@echo "Source files:"
	@echo "  $(SOURCES)"

# Phony targets (not files)
.PHONY: all clean rebuild help

# Avoid issues with files named like targets
.SECONDARY: $(OBJECTS)
