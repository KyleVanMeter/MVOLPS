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

int branchAndBound(glp_prob *prob, MVOLP::ParameterObj params) {
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

      int pick = params.pickVar(vars);
      double bound = glp_get_col_prim(a, pick);

      std::string printMe = "Violated variables are: ";
      for (auto i : vars) {
        printMe += "x[ " + std::to_string(i) + "] ";
      }
      spdlog::debug(printMe);

      std::shared_ptr<MVOLP::NodeData> S2 =
          std::make_shared<MVOLP::NodeData>(a);

      std::shared_ptr<MVOLP::NodeData> S3 =
          std::make_shared<MVOLP::NodeData>(a);
      glp_set_col_bnds(S2->prob, pick, GLP_UP, 0, floor(bound));
      spdlog::info("Adding constraint " + std::to_string(floor(bound)) +
                   " >= " + "x[" + std::to_string(pick) +
                   "] to object " + std::to_string(S2->oid));
      glp_set_col_bnds(S3->prob, pick, GLP_LO, ceil(bound), 0);
      spdlog::info("Adding constraint " + std::to_string(ceil(bound)) +
                   " <= " + "x[" + std::to_string(pick) +
                   "] to object " + std::to_string(S3->oid));

      MVOLP::SPInfo sp = {S2->oid, MVOLP::NONE};
      subProblems.append_child(root, sp);
      sp = {S3->oid, MVOLP::NONE};
      subProblems.append_child(root, sp);
      treeIndex[S2->oid] = ++root;
      treeIndex[S3->oid] = ++root;

      leafContainer.push(S2);
      leafContainer.push(S3);
      if (count > 200000) {
        spdlog::error("Loop limit hit.  Returning early.");
        std::exit(-1);
      }
    }

    count++;
  }

  std::cout << "\nSolution is: " + solution + "\n";

  PrettyPrintTree(subProblems, subProblems.begin(), [](MVOLP::SPInfo &in) {
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
