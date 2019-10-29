CC=g++
CFLAGS=-lglpk

MVOLPS:
	$(CC) 2test.cpp $(CFLAGS) -o MVOLPS

.PHONY: MVOLPS

clean:
	rm MVOLPS
