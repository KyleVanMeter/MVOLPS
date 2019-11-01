#include "glpk.h"

#include <tuple>
#include <vector>

namespace MVOLP {
enum Operation { ADD = 1, MULT = 2 };
enum RC { COL = 1, ROW = 2 };
enum FileType { LP = 1, MPS = 2 };

// This keeps track of which row, or column we make a change to when
// standardizing so we can reconstruct the solution to the original problem
static std::vector<std::tuple<RC, int, Operation, double>> stackMachine;
} // namespace MVOLP

void standard(glp_prob *prob);

void initProblem(std::string filename, MVOLP::FileType ft);
