#include "glpk.h"

#include <tuple>
#include <vector>

namespace MVOLP {
enum Operation { ADD = 1, MULT = 2 };
enum RC { COL = 1, ROW = 2 };
enum FileType { LP = 1, MPS = 2 };
enum CondType { FR = 0, LO = 1, UP = 2, DB = 3, FX = 4 };
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

private:
  NodeData(const glp_prob &other);
  NodeData &operator=(const NodeData &other);
};

// This keeps track of which row, or column we make a change to when
// standardizing so we can reconstruct the solution to the original problem
static std::vector<std::tuple<RC, int, Operation, double>> stackMachine;

// In the case where we are not standardizing our problem we need to keep track
// of the constraint type along with the constraint value
static std::vector<std::tuple<CondType, double, double>> constraintVector;
} // namespace MVOLP

void standard(glp_prob *prob);

glp_prob *initProblem(std::string filename, MVOLP::FileType ft);

double evalObj(std::vector<double> coef);
