#include "BranchAndCut.h"
#include "util.h"

#include <limits>
#include <map>
#include <math.h>
#include <memory>
#include <queue>

#define MAXCUTS 2

void addGomoryCut(glp_prob *in) {
  int m = glp_get_num_rows(in);
  int n = glp_get_num_cols(in);

  double *val;
  int *ind;

  val = (double *)calloc(n, sizeof(double));
  ind = (int *)calloc(n, sizeof(int));

  for (int i = 1; i <= n; i++) {
    double colPrim = glp_get_col_prim(in, i);
    if ((glp_get_col_kind(in, i) != GLP_CV) &&
        (std::trunc(colPrim) != colPrim)) {
      double fractPart, intPart;
      double sumOfB = 0.0;

      std::map<int, double> sumCoefs;
      for (int a = 1; a <= n; a++) {
        sumCoefs[a] = 0.0;
      }

      int ret = glp_eval_tab_row(in, i + m, ind, val);
      for (int j = 1; j <= ret; j++) {
        fractPart = modf(val[j], &intPart);
        if (fractPart < 0.0) {
          // Our fractional component for a negative coefficient should be 1-f,
          // but since modf returns a negative number for negative input we just
          // do 1+f
          fractPart = 1 + fractPart;
        }

        double *sVal = (double *)calloc(n, sizeof(double));
        int *sInd = (int *)calloc(n, sizeof(int));

        int sRet = glp_get_mat_row(in, j, sInd, sVal);
        std::string printMe(" ");

        sumOfB += glp_get_row_ub(in, j) * fractPart;
        for (int k = 1; k <= sRet; k++) {
          sumCoefs[sInd[k]] += (-1 * sVal[k] * fractPart);
          printMe += std::to_string(-1 * sVal[k] * fractPart) + "*x[" +
                     std::to_string(sInd[k]) + "] +";
          // std::cout << "->" << sVal[k] << ", x[" << sInd[k] << "]<-";
        }

        // std::cout << printMe;

        free(sInd);
        free(sVal);
      }

      for (auto &a : sumCoefs) {
        std::cout << a.second << "*x[" << a.first << "] +";
      }

      fractPart = modf(colPrim, &intPart);
      if (fractPart < 0.0) {
        fractPart = 1 + fractPart;
      }

      std::cout << " >= " << fractPart - sumOfB << "\n";
    }
  }

  free(val);
  free(ind);
}

int branchAndCut(glp_prob *prob) {
  std::queue<std::shared_ptr<MVOLP::NodeData>> leafContainer;
  std::shared_ptr<MVOLP::NodeData> S1 = std::make_shared<MVOLP::NodeData>(prob);
  S1->inital = true;
  leafContainer.push(S1);

  glp_prob *a = glp_create_prob();
  double bestLower = -std::numeric_limits<double>::infinity();
  double bestUpper = std::numeric_limits<double>::infinity();

  bool branch = true;
  std::vector<int> vars;

  while (!leafContainer.empty()) {
    glp_copy_prob(a, leafContainer.front().get()->prob, GLP_OFF);

    branch = true;

    for (int i = 0; i < MAXCUTS; i++) {
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
      vars = ret.second;

      leafContainer.front().get()->upperBound = glp_get_obj_val(a);

      if (status == 1) {
        // Prune by integrality

        leafContainer.front().get()->upperBound = glp_get_obj_val(a);
        if (leafContainer.front().get()->upperBound > bestLower) {
          bestLower = leafContainer.front().get()->upperBound;
        }

        branch = false;

        leafContainer.front().~shared_ptr();
        leafContainer.pop();
      } else if (status == -1) {
        // Prune by non-initial infeasibility

        branch = false;

        leafContainer.front().~shared_ptr();
        leafContainer.pop();
      } else if (glp_get_obj_val(a) <= bestLower) {
        // Prune by worse lower bound

        branch = false;

        leafContainer.front().~shared_ptr();
        leafContainer.pop();
      } else {
        leafContainer.front().~shared_ptr();
        leafContainer.pop();

        // Add cutting-plane?
        addGomoryCut(a);
      }
    }

    if (branch) {
      double bound = glp_get_col_prim(a, vars[0]);

      std::shared_ptr<MVOLP::NodeData> S2 =
          std::make_shared<MVOLP::NodeData>(a);

      std::shared_ptr<MVOLP::NodeData> S3 =
          std::make_shared<MVOLP::NodeData>(a);
      glp_set_col_bnds(S2->prob, vars.front(), GLP_UP, 0, floor(bound));
      glp_set_col_bnds(S3->prob, vars.front(), GLP_LO, ceil(bound), 0);

      leafContainer.push(S2);
      leafContainer.push(S3);
    }
  }
}
