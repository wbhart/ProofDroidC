# Compiler and flags
CXX = g++
CXXFLAGS = -Wall

# PackCC tool for PEG parsing
PACKCC = packcc

# Source files and output
PEG_FILE = grammar.peg
GENERATED_C = grammar.c
GENERATED_H = grammar.h
H_FILES = symbol_enum.h node.h
TARGET = proof_droid

# Default target: build the application
all: $(TARGET)

# Rule to create grammar.c and grammar.h from grammar.peg
$(GENERATED_C) $(GENERATED_H): $(PEG_FILE)
	$(PACKCC) $(PEG_FILE)

# Rule to compile the application
$(TARGET): $(GENERATED_C) $(H_FILES)
	$(CXX) $(CXXFLAGS) $(GENERATED_C) -o $(TARGET)

# Clean up generated files, but don't remove manually created headers
clean:
	rm -f $(GENERATED_C) $(GENERATED_H) $(TARGET)

.PHONY: all clean

