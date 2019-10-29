#include "InputParser.h"
#include "eigen3/Eigen/Core"
#include "glpk.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <typeinfo>

enum MVOLP_FileType { MVOLP_LP = 1, MVOLP_MPS = 2 };

void standard(glp_prob *prob) {
  int rows = glp_get_num_rows(prob);
  int cols = glp_get_num_cols(prob);

  /*
   * Ensure that the problem is maximization
   */
  if (glp_get_obj_dir(prob) == GLP_MIN) {
    int coef[cols];
    for (int i = 1; i <= cols; i++) {
      coef[i] = glp_get_obj_coef(prob, i) * -1;
      glp_set_obj_coef(prob, i, coef[i]);
    }

    glp_set_obj_dir(prob, GLP_MAX);
  }

  /*
   * Ensure that the RHS is in standard form, e.g. Ax<=b only
   */
  for (int i = 1; i <= rows; i++) {
    int condType = glp_get_row_type(prob, i);
    int ind[cols];
    double val[cols];

    if (condType != GLP_UP) {
      if (condType == GLP_LO) {
        // Ax >= b turns into -Ax <= -b
        double bound = glp_get_row_lb(prob, i);
        // The 0 is ignored in the GLP_UP case
        glp_set_row_bnds(prob, i, GLP_UP, 0, -1.0 * bound);

        int len = glp_get_mat_row(prob, i, ind, val);
        for (int j = 1; j <= len; j++) {
          val[j] *= -1.0;
        }

        glp_set_mat_row(prob, i, len, ind, val);
      }

      if (condType == GLP_FX) {
        int len = glp_get_mat_row(prob, i, ind, val);

        // Ax = b turns into Ax <= b and -Ax <= -b
        double bound = glp_get_row_lb(prob, i);
        glp_set_row_bnds(prob, i, GLP_UP, 0, bound);

        int newRow = glp_add_rows(prob, 1);
        for (int j = 1; j < len; j++) {
          val[j] *= -1.0;
        }

        glp_set_row_bnds(prob, newRow, GLP_UP, 0, -1.0 * bound);
        glp_set_mat_row(prob, newRow, len, ind, val);
      }

      // b1 <= Ax <= b2 turns into Ax <= b2 and -Ax <= b2
      if (condType == GLP_DB) {
        int len = glp_get_mat_row(prob, i, ind, val);
        double uBound = glp_get_row_ub(prob, i);
        double lBound = glp_get_row_lb(prob, i);

        glp_set_row_bnds(prob, i, GLP_UP, 0, uBound);

        int newRow = glp_add_rows(prob, 1);
        for (int j = 1; j < len; j++) {
          val[j] *= -1.0;
        }

        glp_set_row_bnds(prob, newRow, GLP_UP, 0, -1.0 * lBound);
        glp_set_mat_row(prob, newRow, len, ind, val);
      }
    }
  }
}

void initProblem(std::string filename, MVOLP_FileType ft) {
  printf("GLPK version: %s\n", glp_version());

  glp_prob *prob = glp_create_prob();

  if (ft == MVOLP_LP) {
    if (glp_read_lp(prob, NULL, filename.c_str())) {
      // No need to throw an exception as error msg passing is done by GLPK
      std::exit(-1);
    }
  }
  if (ft == MVOLP_MPS) {
    if (glp_read_mps(prob, GLP_MPS_FILE, NULL, filename.c_str())) {
      std::exit(-1);
    }
  }

  // Convert problem into standard Ax<=b form
  standard(prob);

  if (glp_get_obj_dir(prob) == GLP_MIN) {
    printf("Problem is minimization\n");
  } else {
    printf("Problem is maximization\n");
  }
  int rows = glp_get_num_rows(prob);
  int cols = glp_get_num_cols(prob);

  Eigen::VectorXd objectiveVector(cols);
  Eigen::MatrixXd constraintMatrix(rows, cols);
  Eigen::VectorXd constraintVector(rows);

  for (int j = 1; j <= cols; j++) {
    objectiveVector.row(j - 1) << glp_get_obj_coef(prob, j);
  }

  printf("Problem is %d x %d\n", rows, cols);
  std::cout << "c^T = " << objectiveVector.transpose() << std::endl;

  int num_non_zero[rows];
  for (int i = 1; i <= rows; i++) {
    int ind[cols];
    double val[cols];
    num_non_zero[i] = glp_get_mat_row(prob, i, ind, val);

    /*
     * At each step we search through the non-zero variables, and if they
     * match our index we store them in the constraint matrix, otherwise we
     * know that the variable is zero.
     */
    for (int j = 1; j <= cols; j++) {
      bool change = false;
      for (int k = 1; k <= num_non_zero[i]; k++) {
        if (j == ind[k]) {
          constraintMatrix.row(i - 1).col(j - 1) << val[k];
          // printf(" %f ", val[k]);
          change = true;
          break;
        }
      }
      if (!change) {
        constraintMatrix.row(i - 1).col(j - 1) << 0;
        // printf(" 0 ");
      }
    }

    /*
     * Most of this can be removed as we are already processing the problem to
     * account for different types of constraints.  That is, con_type should
     * only be GLP_UP
     */
    int con_type = glp_get_row_type(prob, i);
    double lb = glp_get_row_lb(prob, i);
    double ub = glp_get_row_ub(prob, i);
    if (con_type == GLP_LO) {
      // printf(" >= %f", lb);
      constraintVector.row(i - 1) << lb;
    } else if (con_type == GLP_UP) {
      // printf(" <= %f", ub);
      constraintVector.row(i - 1) << ub;
    } else if (con_type == GLP_DB) {
      // printf(" <= %f, and >= %f", lb, ub);
      constraintVector.row(i - 1) << lb;
    } else {
      // printf(" = %f", lb);
      constraintVector.row(i - 1) << lb;
      std::cout << "uh huh" << std::endl;
    }
    // printf("\n");
  }
  std::cout << "b = \n" << constraintVector << std::endl;
  std::cout << "A = \n" << constraintMatrix << std::endl;
}

int main(int argc, char **argv) {
  InputParser input(argc, argv);

  if (input.CMDOptionExists("-h")) {
    std::cout << "Usage: MVOLPS [OPTION]\n"
              << "File input options:\n"
              << "  -f [FILENAME.{mps|lp}]\n\n"
              << "Help:\n"
              << "  -h\n";

    return 0;
  }

  if (input.CMDOptionExists("-f")) {
    std::string fn = input.getCMDOption("-f");
    std::string temp = fn;
    std::reverse(temp.begin(), temp.end());
    temp = temp.substr(0, temp.find('.') + 1);
    std::reverse(temp.begin(), temp.end());

    if (temp == ".lp") {
      initProblem(fn, MVOLP_LP);
    } else if (temp == ".mps") {
      initProblem(fn, MVOLP_MPS);
    } else {
      std::cout << "Unrecognized filetype\n";

      return -1;
    }
  }

  return 0;
}
