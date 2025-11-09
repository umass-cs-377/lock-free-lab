# Makefile for Concurrency Labs: Lock vs CAS and Lock vs Spin

CC      = gcc
CFLAGS  = -std=c11 -O2 -Wall -Wextra -pthread

# Targets
CAS_TARGET  = lock_vs_cas
SPIN_TARGET = lock_vs_spin

# Sources
CAS_SRC  = lock_vs_cas.c
SPIN_SRC = lock_vs_spin.c

# Default target builds both
all: $(CAS_TARGET) $(SPIN_TARGET)

$(CAS_TARGET): $(CAS_SRC)
	$(CC) $(CFLAGS) $(CAS_SRC) -o $(CAS_TARGET)

$(SPIN_TARGET): $(SPIN_SRC)
	$(CC) $(CFLAGS) $(SPIN_SRC) -o $(SPIN_TARGET)

# Run the CAS version with default arguments
run-cas: $(CAS_TARGET)
	./$(CAS_TARGET) 8 2000000

# Run the Spin version with default arguments
run-spin: $(SPIN_TARGET)
	./$(SPIN_TARGET) 8 2000000

# Build both in debug mode
debug: CFLAGS += -g -O0
debug: clean all
	@echo "Both labs compiled with debug symbols."

# Clean up all compiled files
clean:
	rm -f $(CAS_TARGET) $(SPIN_TARGET)
