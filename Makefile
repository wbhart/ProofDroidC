# Compiler and flags
CXX = g++
CXXFLAGS = -Wall

# PackCC tool for PEG parsing
PACKCC = packcc

# Directories
SRC_DIR = src
TEST_DIR = test
BUILD_DIR = build

# Source files and output
PEG_FILE = $(SRC_DIR)/grammar.peg
GENERATED_C = $(SRC_DIR)/grammar.c
GENERATED_H = $(SRC_DIR)/grammar.h
H_FILES = $(SRC_DIR)/symbol_enum.h $(SRC_DIR)/node.h $(SRC_DIR)/precedence.h
TARGET = proof_droid

# Test files and targets
TEST_TARGETS = $(BUILD_DIR)/t-parser

# Default target: build the application
all: $(TARGET)

# Rule to create grammar.c and grammar.h from grammar.peg
$(GENERATED_C) $(GENERATED_H): $(PEG_FILE)
	$(PACKCC) $(PEG_FILE)

# Rule to compile the application
$(TARGET): $(SRC_DIR)/proof_droid.cpp $(GENERATED_C) $(H_FILES)
	$(CXX) $(CXXFLAGS) $(SRC_DIR)/proof_droid.cpp $(GENERATED_C) -o $(TARGET) -I$(SRC_DIR)

# Ensure the build directory exists before compiling test files
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Rule to compile test files into the build directory
$(TEST_TARGETS): $(BUILD_DIR) $(TEST_DIR)/t-parser.cpp $(GENERATED_C) $(H_FILES)
	$(CXX) $(CXXFLAGS) $(TEST_DIR)/t-parser.cpp $(GENERATED_C) -o $(BUILD_DIR)/t-parser -I$(SRC_DIR)

# Clean up generated files and binaries
clean:
	rm -f $(GENERATED_C) $(GENERATED_H) $(TARGET) $(BUILD_DIR)/*

# Run tests with "make check"
check: $(TEST_TARGETS)
	@$(BUILD_DIR)/t-parser

.PHONY: all clean check
