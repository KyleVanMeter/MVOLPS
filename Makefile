CC=g++
CFLAGS=-lglpk -I. -Wall --std=c++17
DEPS=util.h tree.hh BranchAndBound.h
OBJ=2test.o util.o BranchAndBound.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

MVOLPS: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY: MVOLPS

clean:
	rm -rf *.o
