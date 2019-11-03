CC=g++
CFLAGS=-lglpk -I. -Wall -std=c++14 -Wpedantic
DEPS=util.h tree.hh BranchAndBound.h
OBJ=2test.o util.o BranchAndBound.o
RM=rm -f

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

MVOLPS: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY: MVOLPS

clean:
	$(RM) $(OBJ)

distclean:
	$(RM) MVOLPS
