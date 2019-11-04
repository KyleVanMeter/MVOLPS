#include "BranchAndBound.h"
#include "tree.hh"
#include "tree_util.hh"
#include "util.h"

// std::trunc()
#include <cmath>
// std::accumulate()
#include <numeric>
#include <queue>

#include "spdlog/spdlog.h"

/*
 * Utility function to report on problem progress.
 * Returns -1 for infeasible
 *         0 for normal
 *         1 for integer solution
 */
std::pair<int, std::vector<int>> printInfo(glp_prob *prob,
                                           bool initial = false) {
  int cols = glp_get_num_cols(prob);
  std::string printer("");
  std::vector<double> coef(cols);
  std::vector<int> violated(cols);
  spdlog::info("test");

  // If the initial relaxation has no solution then the IP problem will not
  // have one too
  int status = glp_get_status(prob);
  if (status == GLP_NOFEAS || status == GLP_INFEAS || status == GLP_UNBND) {
    if (initial) {
      std::cout
          << "Initial LP relaxation has no feasible solution.  Terminating\n";
    } else {
      std::cout << "LP relaxation has no feasible solution.  Pruning\n";
    }
    return std::make_pair(-1, violated);
  }

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

  // Constant (shift) term
  std::cout << glp_get_obj_coef(prob, 0) << " = " << glp_get_obj_val(prob)
            << "\n";

  // The vector is still 0 initialized iff no integrality constraints are
  // violated
  if (std::accumulate(violated.begin(), violated.end(), 0) == 0) {
    if (initial) {
      std::cout << "OPTIMAL IP SOLUTION FOUND: on initial relaxation\n";
    } else {
      std::cout << "IP SOLUTION FOUND\n";
    }

    return std::make_pair(1, violated);
  }

  std::cout << printer;

  return std::make_pair(0, violated);
}

int branchAndBound(glp_prob *prob) {
  std::queue<std::shared_ptr<MVOLP::NodeData>> leafContainer;
  std::shared_ptr<MVOLP::NodeData> S1 = std::make_shared<MVOLP::NodeData>(prob);
  S1->inital = true;
  leafContainer.push(S1);

  glp_prob *a = glp_create_prob();
  double bestLower = -std::numeric_limits<double>::infinity();
  double bestUpper = std::numeric_limits<double>::infinity();

  int count = 0;

  while (!leafContainer.empty()) {
    std::cout << "OID: " << leafContainer.front().get()->oid
              << ", Container size: " << leafContainer.size() << "\n";
    glp_copy_prob(a, leafContainer.front().get()->prob, GLP_OFF);
    glp_simplex(a, NULL);
    if (leafContainer.front().get()->inital) {
      std::pair<int, std::vector<int>> ret = printInfo(a, true);
      int status = ret.first;

      if (status != 0) {
        break;
      }
    }

    std::pair<int, std::vector<int>> ret = printInfo(a, false);
    int status = ret.first;
    std::vector<int> vars = ret.second;

    leafContainer.front().get()->upperBound = glp_get_obj_val(a);

    if (status == 1) {
      // Prune by integrality
      leafContainer.front().get()->upperBound = glp_get_obj_val(a);
      if (leafContainer.front().get()->upperBound > bestLower) {
        bestLower = leafContainer.front().get()->upperBound;
      }

      leafContainer.front().~shared_ptr();
      leafContainer.pop();
    } else if (status == -1) {
      // Prune infeasible non-initial sub-problems
      leafContainer.front().~shared_ptr();
      leafContainer.pop();
    } else if (glp_get_obj_val(a) <= bestLower) {
      // Prune if node is worse then best lower bound
      leafContainer.front().~shared_ptr();
      leafContainer.pop();
    } else {
      std::cout << leafContainer.size() << std::endl;
      leafContainer.front().~shared_ptr();
      leafContainer.pop();
      std::cout << leafContainer.size() << std::endl;

      double bound = glp_get_obj_coef(a, vars[1]);
      for (auto i : vars) {
        std::cout << i << " ";
      }
      std::shared_ptr<MVOLP::NodeData> S2 =
          std::make_shared<MVOLP::NodeData>(a);

      std::shared_ptr<MVOLP::NodeData> S3 =
          std::make_shared<MVOLP::NodeData>(a);
      glp_set_col_bnds(S2->prob, vars.front() + 1, GLP_UP, 0, floor(bound));
      std::cout << "Adding " << floor(bound) << " >= " << "x[" << vars.front()+1 << "]\n";
      glp_set_col_bnds(S3->prob, vars.front() + 1, GLP_LO, 0, ceil(bound));
      std::cout << "Adding " << ceil(bound) << " <= "
                << "x[" << vars.front() + 1 << "]\n";

      leafContainer.push(S2);
      leafContainer.push(S3);
      if (count > 20) {
        break;
      }

    }

    count++;
  }

  return 0;
}
