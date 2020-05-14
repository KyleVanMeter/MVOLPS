#include "bs.h"
#include "cut.h"
#include "gmi.h"
#include "message.h"
#include "tree.hh"
#include "tree_print.h"
#include "tree_util.hh"
#include "util.h"

#include <zmqpp/zmqpp.hpp>
// std::trunc()
#include <cmath>
// std::accumulate()
#include <numeric>
#include <queue>
// std::map
#include <map>
// info and debug
#include "spdlog/spdlog.h"

int branchAndBound(glp_prob *prob, MVOLP::ParameterObj &params) {
  MVOLP::BaseMessageDispatch::define<MVOLP::LogDispatch>("LogDispatch");
  std::shared_ptr<MVOLP::LogDispatch> logInfo =
      std::dynamic_pointer_cast<MVOLP::LogDispatch>(
          MVOLP::BaseMessageDispatch::create("LogDispatch"));

  MVOLP::BaseMessageDispatch::define<MVOLP::DebugDispatch>("DebugDispatch");
  std::shared_ptr<MVOLP::DebugDispatch> logDebug =
      std::dynamic_pointer_cast<MVOLP::DebugDispatch>(
          MVOLP::BaseMessageDispatch::create("DebugDispatch"));

  MVOLP::BaseMessageDispatch::define<MVOLP::IPCDispatch>("IPCDispatch");
  std::shared_ptr<MVOLP::IPCDispatch> mqDispatch =
      std::dynamic_pointer_cast<MVOLP::IPCDispatch>(
          MVOLP::BaseMessageDispatch::create("IPCDispatch"));

  CutPool pool;
  tree<MVOLP::SPInfo> subProblems;
  // tree<int> subProblems;
  // maps OID to iterator of that node
  std::map<int, tree<MVOLP::SPInfo>::iterator> treeIndex;

  std::deque<std::shared_ptr<MVOLP::NodeData>> leafContainer;
  std::shared_ptr<MVOLP::NodeData> S1 = std::make_shared<MVOLP::NodeData>(prob);
  S1->inital = true;
  leafContainer.push_back(S1);

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
    int index;
    std::shared_ptr<MVOLP::NodeData> node =
        params.pickNode(leafContainer, index);
    //mqDispatch->baseFields->oid = node->oid;
    root = treeIndex[node.get()->oid];
    //mqDispatch->baseFields->pid = subProblems.parent(root)->oid;

    logDebug
        ->message(sstr("Current OID: ", node->oid, " with z-value ",
                       node->upperBound))
        ->write();
    logDebug->message(sstr("Container size: ", leafContainer.size()))->write();
    glp_erase_prob(a);
    a = glp_create_prob();
    glp_copy_prob(a, node.get()->prob, GLP_OFF);
    glp_simplex(a, NULL);

    if (node.get()->inital) {
      std::pair<int, std::vector<int>> ret = printInfo(a, true);
      int status = ret.first;

      if (status != 0) {
        break;
      }
    }

    std::pair<int, std::vector<int>> ret = printInfo(a, false);
    int status = ret.first;
    std::vector<int> vars = ret.second;

    node->upperBound = glp_get_obj_val(a);

    if (status == 1) {
      // Prune by integrality
      node.get()->upperBound = glp_get_obj_val(a);

      root.node->data.prune = MVOLP::INTG;
      MVOLP::BaseMessagePOD why;
      why.oid = node->oid;
      why.pid = subProblems.parent(root)->oid;
      why.nodeType = MVOLP::EventType::integer;
      why.direction = MVOLP::BranchDirection::L;
      mqDispatch->baseFields = why;
      //mqDispatch->baseFields->nodeType = MVOLP::EventType::integer;
      //mqDispatch->baseFields->direction = MVOLP::BranchDirection::L;
      mqDispatch->field6 = 0.0;
      mqDispatch->field9 = 0;
      mqDispatch->field10 = 1;
      mqDispatch->write();

      logInfo
          ->message(sstr("OID: ", node.get()->oid, ".  Pruning integral node."))
          ->write();

      if (node.get()->upperBound > bestLower) {
        // Current best solution
        bestLower = node.get()->upperBound;
        logInfo
            ->message(sstr("OID: ", node.get()->oid,
                           ".  Updating best lower bound to ", bestLower))
            ->write();

        solution = "";
        for (int i = 1; i <= glp_get_num_cols(a); i++) {
          if (glp_get_col_prim(a, i) != 0 && glp_get_obj_coef(a, i) != 0) {
            solution += sstr((glp_get_obj_coef(a, i)), "*(x[", i,
                             "] = ", (glp_get_col_prim(a, i)), ") + ");
          }
        }

        // Constant (shift) term
        solution +=
            sstr(glp_get_obj_coef(a, 0), " = ", glp_get_obj_val(a), "\n");
      }

      leafContainer.erase(leafContainer.begin() + index);
    } else if (status == -1) {
      // Prune infeasible non-initial sub-problems

      root.node->data.prune = MVOLP::FEAS;
      logInfo
          ->message(sstr("OID: ", node.get()->oid,
                         ".  Pruning non-initial infeasible node"))
          ->write();
      leafContainer.erase(leafContainer.begin() + index);
    } else if (glp_get_obj_val(a) <= bestLower) {
      // Prune if node is worse then best lower bound

      root.node->data.prune = MVOLP::BNDS;
      logInfo
          ->message(sstr("OID: ", node.get()->oid,
                         ".  Pruning worse lower-bounded node"))
          ->write();
      leafContainer.erase(leafContainer.begin() + index);
    } else {
      logDebug->message(sstr("Queue size is ", leafContainer.size()))->write();
      leafContainer.erase(leafContainer.begin() + index);

      if (params.IsCutEnabled()) {
        for (int j = 1; j <= glp_get_num_cols(a); j++) {
          CutContainer result = generateCut3(a, j);
          if (result.oid != -1) {
            pool.addToPool(result);
          }
        }

        pool.addCutConstraint(a);
      }

      int pick = params.pickVar(vars);
      double bound = glp_get_col_prim(a, pick);

      std::string printMe = "Violated variables are: ";
      for (auto i : vars) {
        printMe += sstr("x[ ", i, "] ");
      }
      logDebug->message(printMe)->write();

      std::shared_ptr<MVOLP::NodeData> S2 =
          std::make_shared<MVOLP::NodeData>(a);

      std::shared_ptr<MVOLP::NodeData> S3 =
          std::make_shared<MVOLP::NodeData>(a);
      glp_set_col_bnds(S2->prob, pick, GLP_UP, 0, floor(bound));
      logInfo
          ->message(sstr("Adding constraint ", floor(bound), " >= x[", pick,
                         "] to object ", S2->oid))
          ->write();
      glp_simplex(S2->prob, NULL);
      S2->upperBound = glp_get_obj_val(S2->prob);

      glp_set_col_bnds(S3->prob, pick, GLP_LO, ceil(bound), 0);
      logInfo
          ->message(sstr("Adding constraint ", ceil(bound), " <= ", "x[", pick,
                         "] to object ", S3->oid))
          ->write();
      glp_simplex(S3->prob, NULL);
      S3->upperBound = glp_get_obj_val(S3->prob);

      MVOLP::SPInfo sp = {S2->oid, MVOLP::NONE};
      subProblems.append_child(root, sp);
      sp = {S3->oid, MVOLP::NONE};
      subProblems.append_child(root, sp);
      treeIndex[S2->oid] = ++root;
      treeIndex[S3->oid] = ++root;

      leafContainer.push_back(S2);
      leafContainer.push_back(S3);
      if (count > 200000) {
        spdlog::error("Loop limit hit.  Returning early.");
        std::exit(-1);
      }
    }

    count++;
  }

  std::cout
      << "[I = Integral node, F = Infeasible node, B = Worse bound node]\n";
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

  std::cout << "\nSolution is: " + solution + "\n";
  logInfo->message(sstr("Solution found after ", count, " iterations"))
      ->write();

  return 0;
}
