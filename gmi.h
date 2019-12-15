#ifndef GMI_H
#define GMI_H
#include "cut.h"
#include "util.h"
#include "glpk.h"

CutContainer generateCut3(glp_prob *in, int j);

#endif
