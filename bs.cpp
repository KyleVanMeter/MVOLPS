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

/*
 * Calling the tree<T> parent function at the root node causes a segfault in
 * the library itself.  To get around that we create a wrapper that
 * (erroneously) says that the root of the tree is its own parent.
 */
int getParentOid(const tree<MVOLP::SPInfo> &probs,
                 const tree<MVOLP::SPInfo>::iterator &iter) {
  if (iter->oid <= 1) {
    return iter->oid;
  }

  return probs.parent(iter)->oid;
}

/*
 * in grUMPy there is an arbitrary convention for the BNB tree to branch in the
 * middle direction if its the initial subproblem, to the right for an added
 * upper-bound constraint, and left for an added lower-bound constraint.
 * Unfortunately this is not so easily determined, and to avoid refactoring we
 * must rely on the order in which sub-problems are created (see the end of
 * branchAndBound) which corresponds to an even oid (R), and an odd oid (L)
 */
MVOLP::BranchDirection getBranchDirection(const int &oid) {
  if (oid <= 1) {
    return MVOLP::BranchDirection::M;
  }
  if (oid % 2 == 0) {
    return MVOLP::BranchDirection::R;
  } else {
    return MVOLP::BranchDirection::L;
  }
}

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
  if (params.isServerEnabled()) {
    mqDispatch->createServer(params.getServerPort());
  }

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
    mqDispatch->clearAll();
    int index;
    MVOLP::BaseMessagePOD baseMsg;

    std::shared_ptr<MVOLP::NodeData> node =
        params.pickNode(leafContainer, index);
    root = treeIndex[node.get()->oid];

    baseMsg.oid = node->oid;
    baseMsg.pid = getParentOid(subProblems, treeIndex[baseMsg.oid]);
    baseMsg.direction = getBranchDirection(baseMsg.oid);

    logDebug
        ->message(sstr("Current OID: ", node->oid, " with z-value ",
                       node->upperBound))
        ->write();
    logDebug->message(sstr("Container size: ", leafContainer.size()))->write();
    glp_erase_prob(a);
    a = glp_create_prob();
    glp_copy_prob(a, node.get()->prob, GLP_OFF);
    glp_simplex(a, NULL);

    MVOLP::BaseMessagePOD pregenantData;
    pregenantData.nodeType = MVOLP::EventType::pregnant;
    pregenantData.oid = node->oid;
    pregenantData.pid = getParentOid(subProblems, treeIndex[node->oid]);
    pregenantData.direction = getBranchDirection(node->oid);
    mqDispatch->baseFields = pregenantData;
    mqDispatch->field6 = glp_get_obj_val(a);
    mqDispatch->field9 = 0;
    mqDispatch->field10 = 0;
    mqDispatch->write();
    mqDispatch->clearAll();

    std::pair<int, std::vector<int>> ret;
    std::vector<int> vars;
    int status;
    if (node.get()->inital) {
      ret = printInfo(a, true);
      status = ret.first;
      vars = ret.second;

      if (status == -1) {
        root.node->data.prune = MVOLP::FEAS;

        break;
      }
      if (status == 1) {
        node.get()->upperBound = glp_get_obj_val(a);
        root.node->data.prune = MVOLP::INTG;

        break;
      }
    } else {
      ret = printInfo(a, false);
      status = ret.first;
      vars = ret.second;
    }

    node->upperBound = glp_get_obj_val(a);

    if (status == 1) {
      // Prune by integrality
      node.get()->upperBound = glp_get_obj_val(a);
      root.node->data.prune = MVOLP::INTG;

      baseMsg.nodeType = MVOLP::EventType::integer;
      mqDispatch->baseFields = baseMsg;
      mqDispatch->field6 = node->upperBound;
      mqDispatch->field9 = 0;
      mqDispatch->field10 = 0;
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

        solution = "[" + std::to_string(node->oid) + "] Solution is: ";
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

      baseMsg.nodeType = MVOLP::EventType::infeasible;
      mqDispatch->baseFields = baseMsg;
      mqDispatch->field9 = 0;
      mqDispatch->field10 = 0;
      mqDispatch->write();

      logInfo
          ->message(sstr("OID: ", node.get()->oid,
                         ".  Pruning non-initial infeasible node"))
          ->write();
      leafContainer.erase(leafContainer.begin() + index);
    } else if (glp_get_obj_val(a) <= bestLower) {
      // Prune if node is worse then best lower bound

      root.node->data.prune = MVOLP::BNDS;

      baseMsg.nodeType = MVOLP::EventType::fathomed;
      mqDispatch->baseFields = baseMsg;
      mqDispatch->write();

      logInfo
          ->message(sstr("OID: ", node.get()->oid,
                         ".  Pruning worse lower-bounded node"))
          ->write();
      leafContainer.erase(leafContainer.begin() + index);
    } else {
      baseMsg.nodeType = MVOLP::EventType::branched;
      mqDispatch->baseFields = baseMsg;
      mqDispatch->field6 = node->upperBound;
      mqDispatch->field7 = [&]() -> double {
        double acc = 0;
        for (auto &i : vars) {
          if (i != 0) {
            acc += getFract(glp_get_col_prim(node->prob, i));
          }
        }

        return acc;
      }();
      mqDispatch->field8 = vars.size();
      mqDispatch->field9 = 0;
      mqDispatch->field10 = 0;
      mqDispatch->write();

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

      MVOLP::BaseMessagePOD candidateData;
      mqDispatch->clearAll();
      candidateData.nodeType = MVOLP::EventType::candidate;
      candidateData.oid = S2->oid;
      candidateData.pid = getParentOid(subProblems, treeIndex[S2->oid]);
      candidateData.direction = getBranchDirection(S2->oid);
      mqDispatch->baseFields = candidateData;
      mqDispatch->field6 = S2->upperBound;
      mqDispatch->write();

      MVOLP::BaseMessagePOD candidateData2;
      mqDispatch->clearAll();
      candidateData2.nodeType = MVOLP::EventType::candidate;
      candidateData2.oid = S3->oid;
      candidateData2.pid = getParentOid(subProblems, treeIndex[S3->oid]);
      candidateData2.direction = getBranchDirection(S3->oid);
      mqDispatch->baseFields = candidateData2;
      mqDispatch->field6 = S3->upperBound;
      mqDispatch->write();

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

  std::cout << "\n" + solution + "\n";
  logInfo->message(sstr("Solution found after ", count, " iterations"))
      ->write();

  return 0;
}
