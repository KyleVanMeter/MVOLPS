#ifndef UTIL_H
#define UTIL_H
#include "glpk.h"

#include <memory>
#include <queue>
#include <tuple>
#include <vector>

// std::ostringstream
#include <sstream>
// std::dec
#include <iostream>

static int id = 1;

namespace MVOLP {
enum Operation { ADD = 1, MULT = 2 };
enum RC { COL = 1, ROW = 2 };
enum FileType { LP = 1, MPS = 2 };
// fixed, lower bound, upper bound, double bound, or free variable
enum CondType { FR = 0, LO = 1, UP = 2, DB = 3, FX = 4 };
// Prune by integrality, (in)feasibility, worse bound, or none
enum PruneType { INTG = 0, FEAS = 1, BNDS = 3, NONE };

namespace param {
enum VarStratType { VO = 0, VFP = 1, VGO = 2 };
enum NodeStratType { DFS = 0, BEST = 1 };
} // namespace param

class NodeData {
public:
  NodeData(glp_prob *parent);
  ~NodeData();
  double upperBound;
  double lowerBound;
  glp_prob *prob;

  // This field keeps track of whether or not the problem is the initial
  // relaxation
  bool inital;

  // Object ID is used for constructing tree representations
  int oid;

private:
  NodeData(const glp_prob &other);
  NodeData &operator=(const NodeData &other);
};

// Sub-Problem info
struct SPInfo {
  int oid;
  PruneType prune;
};

class ParameterObj {
public:
  ParameterObj() = delete;
  ParameterObj(glp_prob *prob)
      : _varStrat(param::VarStratType::VO),
        _nodeStrat(param::NodeStratType::DFS) {
    _prob = prob;
  }

  int pickVar(const std::vector<int> &vars);
  std::shared_ptr<MVOLP::NodeData>
  pickNode(const std::deque<std::shared_ptr<MVOLP::NodeData>> &problems,
           int &index);

  void setStrategy(const param::VarStratType a, const param::NodeStratType b);
  void setVarStrat(const param::VarStratType a);
  void setNodeStrat(const param::NodeStratType a);
  std::pair<param::VarStratType, param::NodeStratType> getStrategy();

private:
  param::VarStratType _varStrat;
  param::NodeStratType _nodeStrat;
  glp_prob *_prob;
};

// This keeps track of which row, or column we make a change to when
// standardizing so we can reconstruct the solution to the original problem
static std::vector<std::tuple<RC, int, Operation, double>> stackMachine;

// In the case where we are not standardizing our problem we need to keep track
// of the constraint type along with the constraint value
static std::vector<std::tuple<CondType, double, double>> constraintVector;
} // namespace MVOLP

double getFract(double x);

void standard(glp_prob *prob);

glp_prob *initProblem(std::string filename, MVOLP::FileType ft);

std::pair<int, std::vector<int>> printInfo(glp_prob *prob, bool initial);

double evalObj(std::vector<double> coef);

int getGlpTerm();

// Template to convert any number of things to std::string
// Use like: sstr("thing," 1, etc)
template <typename... Args> std::string sstr(Args &&... args) {
  std::ostringstream sstr;
  (sstr << std::dec << ... << args);
  return sstr.str();
}
#endif
