#include "BranchAndBound.h"
#include "tree.hh"
#include "tree_util.hh"

// std::trunc()
#include <cmath>
#include <memory>

int branchAndBound(glp_prob *prob) {
  int cols = glp_get_num_cols(prob);
  std::string printer("");
  std::vector<double> coef(cols);
  std::vector<int> violated(cols);

  glp_simplex(prob, NULL);

  // Print the non-zero values of the objective coefficients
  for (int i = 1; i <= cols; i++) {
    coef.at(i-1) = glp_get_col_prim(prob, i);
    if (coef.at(i-1) != 0 && glp_get_obj_coef(prob, i) != 0) {
      std::cout << coef.at(i-1) << "*x[" << i << "]"
                << " + ";

      // If our coefficient is fractional and not continuous
      if (std::trunc(coef.at(i-1)) != coef.at(i-1) &&
          glp_get_col_kind(prob, i) != GLP_CV) {
        violated.at(i-1) = 1;
        printer += "variable x[" + std::to_string(i) + "] integrality violated.\n";
      }
    }
  }
  std::cout << glp_get_obj_coef(prob, 0) << "\n";
  std::cout << printer;

  return 0;
}
