CC=g++
CFLAGS=-lglpk -I. -Wall
DEPS=tree.h util.h
OBJ=2test.o tree.o util.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

MVOLPS: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY: MVOLPS

clean:
	rm -rf *.o
