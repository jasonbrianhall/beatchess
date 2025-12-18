# BeatChess Makefile - Fixed and Improved Version with Debug Target
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

# Debug build with debugging symbols and AI debug output
debug: CXXFLAGS = -Wall -g -O0 -std=c++11 -DCHESS_AI_DEBUG `pkg-config --cflags gtk+-3.0`
debug: LDFLAGS = `pkg-config --libs gtk+-3.0` -lm -lpthread
debug: TARGET = beatchess_debug
debug: clean $(TARGET)
	@echo "✅ Debug build successful: $(TARGET)"
	@echo "   Run with: gdb ./$(TARGET)"
	@echo "   Or: valgrind ./$(TARGET)"
	@echo "   AI debug output is ENABLED (-DCHESS_AI_DEBUG)"

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	rm -f $(OBJECTS) $(TARGET) beatchess_debug
	@echo "✅ Clean complete"

# Clean and rebuild
rebuild: clean all

# Show help
help:
	@echo "BeatChess Makefile - Available targets:"
	@echo "  make              - Build BeatChess (default, optimized)"
	@echo "  make debug        - Build with debug symbols and AI debug output"
	@echo "  make rebuild      - Clean and rebuild (optimized)"
	@echo "  make clean        - Remove object files and executable"
	@echo "  make help         - Show this help message"
	@echo ""
	@echo "Build configuration (Release):"
	@echo "  CXX:              $(CXX)"
	@echo "  CXXFLAGS:         $(CXXFLAGS)"
	@echo "  LDFLAGS:          $(LDFLAGS)"
	@echo "  TARGET:           $(TARGET)"
	@echo ""
	@echo "Debug configuration (make debug):"
	@echo "  CXXFLAGS:         -Wall -g -O0 -std=c++11 -DCHESS_AI_DEBUG (gtk flags)"
	@echo "  TARGET:           beatchess_debug"
	@echo "  Includes:         GDB support + AI debug printf output"
	@echo ""
	@echo "Source files:"
	@echo "  $(SOURCES)"

# Phony targets (not files)
.PHONY: all clean rebuild help debug

# Avoid issues with files named like targets
.SECONDARY: $(OBJECTS)
