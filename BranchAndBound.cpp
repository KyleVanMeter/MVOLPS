#include "BranchAndBound.h"
#include "tree.hh"
#include "tree_util.hh"
#include "util.h"

// std::trunc()
#include <cmath>
// std::accumulate()
#include <numeric>
#include <queue>

/*
 * Utility function to report on problem progress.
 * Returns -1 for infeasible
 *         0 for normal
 *         1 for optimal solution
 */
int printInfo(glp_prob *prob, bool initial=false) {
  int cols = glp_get_num_cols(prob);
  std::string printer("");
  std::vector<double> coef(cols);
  std::vector<int> violated(cols);

  // Print the non-zero values of the objective coefficients
  for (int i = 1; i <= cols; i++) {
    coef.at(i - 1) = glp_get_col_prim(prob, i);
    if (coef.at(i - 1) != 0 && glp_get_obj_coef(prob, i) != 0) {
      std::cout << glp_get_obj_coef(prob, i) << "*(x[" << i
                << "] = " << coef.at(i - 1) << ") + ";

      // If our coefficient is fractional and not continuous
      if (std::trunc(coef.at(i - 1)) != coef.at(i - 1) &&
          glp_get_col_kind(prob, i) != GLP_CV) {
        // Store the column index of the violated variable
        violated.at(i - 1) = i;
        printer +=
            "variable x[" + std::to_string(i) + "] integrality violated\n";
      }
    }
  }

  std::cout << glp_get_obj_coef(prob, 0) << " = " << glp_get_obj_val(prob)
            << "\n";

  if (initial) {
    // If the relaxation has no solution then the IP problem will not have one
    // too
    int status = glp_get_status(prob);
    if (status == GLP_NOFEAS || status == GLP_INFEAS) {
      std::cout
          << "Initial IP relaxation has no feasible solution.  Terminating\n";
      return -1;
    }

    // The vector is still 0 initialized iff no integrality constraints are
    // violated
    if (std::accumulate(violated.begin(), violated.end(), 0) == 0) {
      std::cout << "OPTIMAL IP FOUND: on initial relaxation\n";

      return 1;
    }
  }

  std::cout << printer;

  return 0;
}

int branchAndBound(glp_prob *prob) {
  std::queue<std::shared_ptr<MVOLP::NodeData>> leafContainer;
  std::shared_ptr<MVOLP::NodeData> b = std::make_shared<MVOLP::NodeData>(prob);
  b->inital=true;
  leafContainer.push(b);

  while (!leafContainer.empty()) {
    if (leafContainer.front().get()->inital) {
      glp_prob *a = glp_create_prob();
      glp_copy_prob(a, leafContainer.front().get()->prob, GLP_ON);
      glp_simplex(a, NULL);
      int status = printInfo(a, true);

      if (status != 0) {
        break;
      }
    }
    break;
  }

  return 0;
}
