#include "cut.h"
#include "glpk.h"
#include "util.h"

#include "stdlib.h"
#include <tuple>
#include <cmath>
#include <iostream>


CutContainer generateCut3(glp_prob *in, int j) {
  CutContainer result;
  double temp;
  int curRow, curCol;
  int m = glp_get_num_rows(in);
  int n = glp_get_num_cols(in);

  if (glp_get_col_kind(in, j) != GLP_IV) {
    //std::cout << "non-integer\n";
    result.oid = -1;
    return result;
  }
  if (glp_get_col_stat(in, j) != GLP_BS) {
    //std::cout << "non-basic\n";
    result.oid = -1;
    return result;
  }

  double *work = (double *)calloc(m + n + 1, sizeof(double));
  for (int i = 0; i <= m + n; i++) {
    work[i] = 0;
  }

  double *val2 = (double *)calloc(n + 1, sizeof(double));
  int *ind2 = (int *)calloc(n + 1, sizeof(int));
  int len = glp_eval_tab_row(in, m + j, ind2, val2);
  double rhs = glp_get_col_prim(in, j);
  int stat, kind;
  double ub;

  for (int i = 1; i <= len; i++) {
    double val = val2[i];
    if (ind2[i] <= m) {
      curRow = ind2[i];
      stat = glp_get_row_stat(in, curRow);
      kind = GLP_CV;
      ub = glp_get_row_ub(in, curRow);
    } else {
      curCol = ind2[i] - m;
      stat = glp_get_col_stat(in, curCol);
      kind = glp_get_col_kind(in, curCol);
      ub = glp_get_col_ub(in, curCol);
    }

    double fRhs = getFract(rhs);
    double fVal = getFract(val);
    if (kind == GLP_IV) {
      if (fRhs >= fVal) {
        temp = fVal;
      } else {
        temp = (fRhs / (1.0 - fRhs)) * (1.0 - fVal);
      }
    }
    if (kind == GLP_CV) {
      if (val >= 0.0) {
        temp = val;
      } else {
        temp = (fRhs / (1.0 - fRhs)) * (-1.0 * val);
      }
    }

    work[ind2[i]] = -1.0 * temp;
    rhs -= temp * ub;
  }

  for (int i = 0; i <= m + n; i++) {
    std::cout << work[i] << " ";
  }
  std::cout << "\n";

  for (int i = 1; i <= m; i++) {
    double *val2 = (double *)calloc(n + 1, sizeof(double));
    int *ind2 = (int *)calloc(n + 1, sizeof(int));
    int len2 = glp_get_mat_row(in, i, ind2, val2);

    for (int k = 1; k <= len2; k++) {
      work[m + k] += work[i] * val2[k];
    }
  }

  len = 0;
  double *val = (double *)calloc(n + 1, sizeof(double));
  int *ind = (int *)calloc(n + 1, sizeof(int));
  ind[0] = 0;
  val[0] = rhs;
  std::cout << "\n";
  for (int i = 1; i <= n; i++) {
    len++;
    ind[len] = i;
    val[len] = work[m + i];
    std::cout << val[len] << "*x[" << ind[len] << "] + ";
  }
  std::cout << "0 >= " << val[0] << "\n";

  for (int i = 0; i <= n; i++) {
    result.inds.push_back(ind[i]);
    result.vals.push_back(val[i]);
  }
  result.lb = val[0];

  free(ind);
  free(val);
  free(work);
  free(ind2);
  free(val2);
  return result;
}
