CXX=g++
CXXFLAGS=-lglpk -lspdlog -Wall -g -std=c++17 -Wpedantic
HEADER=-I.
DEPS=util.h tree.hh tree_print.h bs.h cut.h gmi.h
OBJ=2test.o util.o bs.o cut.cpp gmi.cpp
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
