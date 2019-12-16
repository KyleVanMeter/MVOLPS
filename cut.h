#ifndef CUT__H
#define CUT__H

#include "glpk.h"
#include <vector>

struct CutContainer {
  std::vector<int> inds;
  std::vector<double> vals;
  double lb;
  int oid; // Replace this with something that calculates if the generated cut
           // is from a problem of a greater depth than the current subproblem
};

class CutPool {
public:
  int addToPool(CutContainer cut);

  int addCutConstraint(glp_prob *in, int cID = -1);

private:
  std::vector<CutContainer> _cuts;
};

#endif
