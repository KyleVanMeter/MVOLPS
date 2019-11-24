#include "BranchAndBound.h"
#include "tree.hh"
#include "tree_print.h"
#include "tree_util.hh"
#include "util.h"

// std::trunc()
#include <cmath>
// std::accumulate()
#include <numeric>
#include <queue>
// std::map
#include <map>
// info and debug
#include "spdlog/spdlog.h"

/*
 * Utility function to report on problem progress.
 * Returns pair int and vector<int> the int status is:
 *        -1 for infeasible
 *         0 for normal
 *         1 for integer solution
 * The vector contains the column index of the violated variables
 */
std::pair<int, std::vector<int>> printInfo(glp_prob *prob,
                                           bool initial = false) {
  int cols = glp_get_num_cols(prob);
  std::string printer("");
  std::vector<double> coef(cols);
  std::vector<int> violated;

  // If the initial relaxation has no solution then the IP problem will not
  // have one too
  int status = glp_get_status(prob);
  if (status == GLP_NOFEAS || status == GLP_INFEAS || status == GLP_UNBND) {
    if (initial) {
      spdlog::info(
          "Initial LP relaxation has no feasible solution.  Terminating\n");
    }

    return std::make_pair(-1, violated);
  }

  // Print the non-zero values of the objective coefficients
  std::string printMe = "";
  for (int i = 1; i <= cols; i++) {
    coef.at(i - 1) = glp_get_col_prim(prob, i);
    if (coef.at(i - 1) != 0 && glp_get_obj_coef(prob, i) != 0) {
      printMe += std::to_string(glp_get_obj_coef(prob, i)) + "*(x[" +
                 std::to_string(i) + "] = " + std::to_string(coef.at(i - 1)) +
                 ") + ";

      // If our coefficient is fractional and not continuous
      if (std::trunc(coef.at(i - 1)) != coef.at(i - 1) &&
          glp_get_col_kind(prob, i) != GLP_CV) {
        // Store the column index of the violated variable
        // violated.at(i - 1) = i;
        violated.push_back(i);
        printer +=
            "variable x[" + std::to_string(i) + "] integrality violated\n";
      }
    }
  }

  // Constant (shift) term
  printMe += std::to_string(glp_get_obj_coef(prob, 0)) + " = " +
             std::to_string(glp_get_obj_val(prob)) + "\n";
  spdlog::info(printMe);

  // The variables vector is 0
  if (violated.empty()) {
    if (initial) {
      spdlog::info("OPTIMAL IP SOLUTION FOUND: on initial relaxation\n");
    } else {
      spdlog::info("IP SOLUTION FOUND\n");
    }

    return std::make_pair(1, violated);
  }

  spdlog::info(printer);

  return std::make_pair(0, violated);
}

int branchAndBound(glp_prob *prob) {
  tree<MVOLP::SPInfo> subProblems;
  // tree<int> subProblems;
  // maps OID to iterator of that node
  std::map<int, tree<MVOLP::SPInfo>::iterator> treeIndex;

  std::queue<std::shared_ptr<MVOLP::NodeData>> leafContainer;
  std::shared_ptr<MVOLP::NodeData> S1 = std::make_shared<MVOLP::NodeData>(prob);
  S1->inital = true;
  leafContainer.push(S1);

  MVOLP::SPInfo in = {S1->oid, MVOLP::NONE};
  tree<MVOLP::SPInfo>::iterator root =
      subProblems.insert(subProblems.begin(), in);
  subProblems.append_child(root, in);
  treeIndex[S1->oid] = subProblems.begin();

  glp_prob *a = glp_create_prob();
  double bestLower = -std::numeric_limits<double>::infinity();
  double bestUpper = std::numeric_limits<double>::infinity();

  std::string solution = "";
  int count = 0;

  while (!leafContainer.empty()) {

    root = treeIndex[leafContainer.front().get()->oid];
    spdlog::debug("Current OID: " +
                  std::to_string(leafContainer.front().get()->oid));
    spdlog::debug("Container size: " + std::to_string(leafContainer.size()));
    glp_erase_prob(a);
    a = glp_create_prob();
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

      root.node->data.prune = MVOLP::INTG;
      spdlog::info("OID: " + std::to_string(leafContainer.front().get()->oid) +
                   ".  Pruning integral node.");
      leafContainer.front().get()->upperBound = glp_get_obj_val(a);
      if (leafContainer.front().get()->upperBound > bestLower) {
        bestLower = leafContainer.front().get()->upperBound;
        spdlog::info(
            "OID: " + std::to_string(leafContainer.front().get()->oid) +
            ".  Updating best lower bound to " + std::to_string(bestLower));

        solution = "";
        for (int i = 1; i <= glp_get_num_cols(a); i++) {
          if (glp_get_col_prim(a, i) != 0 && glp_get_obj_coef(a, i) != 0) {
            solution += std::to_string(glp_get_obj_coef(a, i)) + "*(x[" +
                        std::to_string(i) +
                        "] = " + std::to_string(glp_get_col_prim(a, i)) +
                        ") + ";
          }
        }

        // Constant (shift) term
        solution += std::to_string(glp_get_obj_coef(a, 0)) + " = " +
                    std::to_string(glp_get_obj_val(a)) + "\n";
      }

      leafContainer.front().~shared_ptr();
      leafContainer.pop();
    } else if (status == -1) {
      // Prune infeasible non-initial sub-problems

      root.node->data.prune = MVOLP::FEAS;
      spdlog::info("OID: " + std::to_string(leafContainer.front().get()->oid) +
                   ".  Pruning non-initial infeasible node");
      leafContainer.front().~shared_ptr();
      leafContainer.pop();
    } else if (glp_get_obj_val(a) <= bestLower) {
      // Prune if node is worse then best lower bound

      root.node->data.prune = MVOLP::BNDS;
      spdlog::info("OID: " + std::to_string(leafContainer.front().get()->oid) +
                   ".  Pruning worse lower-bounded node");
      leafContainer.front().~shared_ptr();
      leafContainer.pop();
    } else {
      spdlog::debug("Queue size is " + std::to_string(leafContainer.size()));
      leafContainer.front().~shared_ptr();
      leafContainer.pop();
      spdlog::debug("Queue size is " + std::to_string(leafContainer.size()));

      double bound = glp_get_col_prim(a, vars[0]);

      std::string printMe = "Violated variables are: ";
      for (auto i : vars) {
        printMe += "x[ " + std::to_string(i) + "] ";
      }
      spdlog::debug(printMe);

      std::shared_ptr<MVOLP::NodeData> S2 =
          std::make_shared<MVOLP::NodeData>(a);

      std::shared_ptr<MVOLP::NodeData> S3 =
          std::make_shared<MVOLP::NodeData>(a);
      glp_set_col_bnds(S2->prob, vars.front(), GLP_UP, 0, floor(bound));
      spdlog::info("Adding constraint " + std::to_string(floor(bound)) +
                   " >= " + "x[" + std::to_string(vars.front()) +
                   "] to object " + std::to_string(S2->oid) + "\n");
      glp_set_col_bnds(S3->prob, vars.front(), GLP_LO, ceil(bound), 0);
      spdlog::info("Adding constraint " + std::to_string(ceil(bound)) +
                   " <= " + "x[" + std::to_string(vars.front()) +
                   "] to object " + std::to_string(S3->oid) + "\n");

      MVOLP::SPInfo sp = {S2->oid, MVOLP::NONE};
      subProblems.append_child(root, sp);
      sp = {S3->oid, MVOLP::NONE};
      subProblems.append_child(root, sp);
      treeIndex[S2->oid] = ++root;
      treeIndex[S3->oid] = ++root;

      leafContainer.push(S2);
      leafContainer.push(S3);
      if (count > 100) {
        spdlog::debug("Loop limit hit.  Returning early.");
        break;
      }
    }

    count++;
  }

  std::cout << "\nSolution is: " + solution + "\n";

  PrettyPrintTree(subProblems, subProblems.begin(), [](MVOLP::SPInfo in) {
    if (in.prune == MVOLP::INTG) {
      return std::to_string(in.oid) + " I";
    } else if (in.prune == MVOLP::FEAS) {
      return std::to_string(in.oid) + " F";
    } else if (in.prune == MVOLP::BNDS) {
      return std::to_string(in.oid) + " B";
    } else {
      return std::to_string(in.oid);
    }
  });

  return 0;
}
