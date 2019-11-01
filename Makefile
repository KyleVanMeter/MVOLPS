CC=g++
CFLAGS=-lglpk -I.
DEPS=tree.h

MVOLPS: 2test.cpp tree.cpp
	$(CC) 2test.cpp $(CFLAGS) -o MVOLPS

.PHONY: MVOLPS

clean:
	rm MVOLPS
