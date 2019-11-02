#include "tree.hh"
#include "tree_util.hh"
#include "BranchAndBound.h"

int branchAndBound(glp_prob *prob) {
  glp_simplex(prob, NULL);

  return 0;
}
