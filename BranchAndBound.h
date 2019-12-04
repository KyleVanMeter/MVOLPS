#ifndef BNB_H
#define BNB_H

#include "glpk.h"
#include "util.h"

int branchAndBound(glp_prob *prob, MVOLP::ParameterObj &params);
#endif
