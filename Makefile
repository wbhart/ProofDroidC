# Compiler and flags
CXX = g++
CXXFLAGS = -Wall

# Source files and output
SRC_DIR = src
PEG_FILE = $(SRC_DIR)/grammar.peg
GENERATED_C = $(SRC_DIR)/grammar.c
GENERATED_H = $(SRC_DIR)/grammar.h
H_FILES = $(SRC_DIR)/symbol_enum.h $(SRC_DIR)/node.h $(SRC_DIR)/precedence.h
TARGET = proof_droid

# Default target: build the application
all: $(TARGET)

# Rule to create grammar.c and grammar.h from grammar.peg
$(GENERATED_C) $(GENERATED_H): $(PEG_FILE)
	packcc $(PEG_FILE)

# Rule to compile the application
$(TARGET): $(GENERATED_C) $(H_FILES) $(SRC_DIR)/proof_droid.cpp
	$(CXX) $(CXXFLAGS) $(GENERATED_C) $(SRC_DIR)/proof_droid.cpp -o $(TARGET) -I$(SRC_DIR)

# Clean up generated files, but don't remove manually created headers
clean:
	rm -f $(GENERATED_C) $(GENERATED_H) $(TARGET)

.PHONY: all clean
