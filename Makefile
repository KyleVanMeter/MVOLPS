CXX=g++
CXXFLAGS=-lglpk -lspdlog -Wall -std=c++14 -Wpedantic
HEADER=-I.
DEPS=util.h tree.hh tree_print.h BranchAndBound.h
OBJ=2test.o util.o BranchAndBound.o 
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
