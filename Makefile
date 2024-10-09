# Compiler and flags
CXX = g++
CXXFLAGS = -Wall

# PackCC tool for PEG parsing
PACKCC = packcc

# Directories
SRC_DIR = src
TEST_DIR = test
BUILD_DIR = build

# Automatically gather all source files
ALL_SRC_FILES = $(wildcard $(SRC_DIR)/*.cpp) $(SRC_DIR)/grammar.c
SRC_FILES = $(filter-out $(SRC_DIR)/proof_droid.cpp, $(ALL_SRC_FILES))
TEST_FILES = $(wildcard $(TEST_DIR)/t-*.cpp)
H_FILES = $(wildcard $(SRC_DIR)/*.h) $(SRC_DIR)/grammar.h

# Output targets
TARGET = proof_droid
TEST_TARGETS = $(patsubst $(TEST_DIR)/%.cpp, $(BUILD_DIR)/%, $(TEST_FILES))

# Default target: build the application
all: grammar $(TARGET)

# Build application target
$(TARGET): $(ALL_SRC_FILES) $(H_FILES)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(ALL_SRC_FILES)

# Build all tests
tests: $(TEST_TARGETS)

$(BUILD_DIR)/%: $(TEST_DIR)/%.cpp $(SRC_FILES) $(H_FILES)
	$(CXX) $(CXXFLAGS) -o $@ $< $(SRC_FILES)

# Run all tests
.PHONY: run_tests
run_tests: tests
	@for test in $(TEST_TARGETS); do \
		echo "Running $$test..."; \
		./$$test || exit 1; \
	done

# Run all tests with make check
.PHONY: check
check: grammar run_tests

# Generate grammar.c and grammar.h from grammar.peg
.PHONY: grammar
grammar:
	$(PACKCC) -o $(SRC_DIR)/grammar $(SRC_DIR)/grammar.peg

# Clean build directory
.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)/* 
	rm -f $(SRC_DIR)/grammar.c $(SRC_DIR)/grammar.h
	rm -f $(TARGET)