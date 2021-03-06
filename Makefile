CXX=g++
CXXFLAGS=-lglpk -lzmq -lzmqpp -Wall -g -std=c++2a -Wpedantic
HEADER=-I.
DEPS=util.h tree.hh tree_print.h bs.h cut.h gmi.h message.h
OBJ=2test.o util.o bs.o cut.o gmi.o message.o
RM=rm -f

%.o: %.c $(DEPS)
	$(CXX) $(HEADER) $(CXXFLAGS) -c -o $@ $<

MVOLPS: $(OBJ)
	$(CXX) $(HEADER) -o $@ $^ $(CXXFLAGS)

.PHONY: MVOLPS

clean:
	$(RM) $(OBJ)

distclean:
	$(RM) MVOLPS
