#ifndef BS_H
#define BS_H

#include "util.h"
#include "glpk.h"

int branchAndBound(glp_prob *prob, MVOLP::ParameterObj &params);
#endif
