#include "util.h"
#include "eigen3/Eigen/Core"
// debug
#include "spdlog/spdlog.h"
#include <iostream>
// numeric_limits
#include <limits>
// static_assert
#include <cassert>

MVOLP::NodeData::NodeData(glp_prob *parent) {
  static_assert(std::numeric_limits<double>::is_iec559,
                "Platform does not support IEE 754 floating-point");

  this->oid = id;
  id += 1;
  this->lowerBound = -std::numeric_limits<double>::infinity();
  this->upperBound = std::numeric_limits<double>::infinity();
  this->prob = glp_create_prob();
  glp_copy_prob(this->prob, parent, GLP_ON);

  this->inital = false;
}

MVOLP::NodeData::~NodeData() {
  spdlog::debug("Destructor called on objectID: " + std::to_string(this->oid));
  glp_delete_prob(this->prob);
}

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

        MVOLP::stackMachine.push_back(
            std::make_tuple(MVOLP::ROW, i, MVOLP::MULT, -1.0));

        int len = glp_get_mat_row(prob, i, ind, val);
        for (int j = 1; j <= len; j++) {
          val[j] *= -1.0;

          MVOLP::stackMachine.push_back(
              std::make_tuple(MVOLP::COL, ind[j], MVOLP::MULT, -1.0));
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
        glp_set_row_bnds(prob, newRow, GLP_UP, 0, -1.0 * lBound);

        MVOLP::stackMachine.push_back(
            std::make_tuple(MVOLP::ROW, i, MVOLP::MULT, -1.0));

        for (int j = 1; j < len; j++) {
          val[j] *= -1.0;

          MVOLP::stackMachine.push_back(
              std::make_tuple(MVOLP::COL, ind[j], MVOLP::MULT, -1.0));
        }

        glp_set_mat_row(prob, newRow, len, ind, val);
      }

      if (condType == GLP_FR) {
      }
    }
  }

  /*
   * ensure that the variable bounds are in standard form (x_n >= 0)
   */
  for (int i = 1; i <= cols; i++) {
    int cond_type = glp_get_col_type(prob, i);

    if (cond_type != GLP_UP) {
      if (cond_type == GLP_LO) {
      }

      if (cond_type == GLP_FX) {
      }

      if (cond_type == GLP_DB) {
      }
    }
  }
}

glp_prob *initProblem(std::string filename, MVOLP::FileType ft) {
  spdlog::info("GLPK version is " + std::string(glp_version()));
  spdlog::info("Opening file: " + filename);

  glp_prob *prob = glp_create_prob();

  if (ft == MVOLP::LP) {
    if (glp_read_lp(prob, NULL, filename.c_str())) {
      // No need to throw an exception as error msg passing is done by GLPK
      std::exit(-1);
    }
  }
  if (ft == MVOLP::MPS) {
    if (glp_read_mps(prob, GLP_MPS_FILE, NULL, filename.c_str())) {
      std::exit(-1);
    }
  }

  // Convert problem into standard Ax<=b form
  // standard(prob);

  spdlog::info(
      sstr("Problem contains ", glp_get_num_int(prob), " integer variables"));
  // if (glp_get_obj_dir(prob) == GLP_MIN) {
  //   printf("Problem is minimization\n");
  // } else {
  //   printf("Problem is maximization\n");
  // }
  int rows = glp_get_num_rows(prob);
  int cols = glp_get_num_cols(prob);

  Eigen::VectorXd objectiveVector(cols);
  Eigen::MatrixXd constraintMatrix(rows, cols);
  Eigen::VectorXd constraintVector(rows);
  Eigen::VectorXd boundVector(cols);

  if (getGlpTerm() == GLP_ON) {
    std::cout << "Bounds: \n";
    for (int j = 1; j <= cols; j++) {
      objectiveVector.row(j - 1) << glp_get_obj_coef(prob, j);

      // boundVector.row(j - 1) <<
      int con_type = glp_get_col_type(prob, j);
      double lb = glp_get_col_lb(prob, j);
      double ub = glp_get_col_ub(prob, j);
      if (con_type == GLP_LO) {
        printf("x[%d] >= %f", j, lb);
        // std::cout << "uh oh" << std::endl;
      } else if (con_type == GLP_UP) {
        printf("x[%d] <= %f", j, ub);
      } else if (con_type == GLP_DB) {
        printf("x[%d] <= %f, and x[%d] >= %f", j, lb, j, ub);
        // std::cout << "uh oh" << std::endl;
      } else if (con_type == GLP_FR) {
        printf("x[%d]", j);
      } else {
        printf("x[%d] = %f", j, lb);
        // std::cout << "uh oh" << std::endl;
      }
      printf("\n");
    }
  }

  spdlog::info(sstr("Problem is ", rows, " x ", cols));

  if (getGlpTerm() == GLP_ON) {
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
        // std::cout << "uh oh" << std::endl;
      } else if (con_type == GLP_UP) {
        // printf(" <= %f", ub);
        constraintVector.row(i - 1) << ub;
      } else if (con_type == GLP_DB) {
        // printf(" <= %f, and >= %f", lb, ub);
        constraintVector.row(i - 1) << lb;
        // std::cout << "uh oh" << std::endl;
      } else {
        // printf(" = %f", lb);
        constraintVector.row(i - 1) << lb;
        // std::cout << "uh oh" << std::endl;
      }
      // printf("\n");
    }

    std::cout << "b = \n" << constraintVector << std::endl;
    std::cout << "A = \n" << constraintMatrix << std::endl;
  }

  return prob;
}

/*
 * Utility function to report on problem progress.
 * Returns pair int and vector<int> the int status is:
 *        -1 for infeasible
 *         0 for normal
 *         1 for integer solution
 * The vector contains the column index of the violated variables
 */
std::pair<int, std::vector<int>> printInfo(glp_prob *prob,
                                           bool initial = false) {
  int cols = glp_get_num_cols(prob);
  std::string printer("");
  std::vector<double> coef(cols);
  std::vector<int> violated;

  // If the initial relaxation has no solution then the IP problem will not
  // have one too
  int status = glp_get_status(prob);
  if (status == GLP_NOFEAS || status == GLP_INFEAS || status == GLP_UNBND) {
    if (initial) {
      spdlog::info(
          "Initial LP relaxation has no feasible solution.  Terminating");
    }

    return std::make_pair(-1, violated);
  }

  // Print the non-zero values of the objective coefficients
  std::string printMe = "";
  for (int i = 1; i <= cols; i++) {
    coef.at(i - 1) = glp_get_col_prim(prob, i);
    if (coef.at(i - 1) != 0 && glp_get_obj_coef(prob, i) != 0) {
      printMe += std::to_string(glp_get_obj_coef(prob, i)) + "*(x[" +
                 std::to_string(i) + "] = " + std::to_string(coef.at(i - 1)) +
                 ") + ";

      // If our coefficient is fractional and not continuous
      if (std::trunc(coef.at(i - 1)) != coef.at(i - 1) &&
          glp_get_col_kind(prob, i) != GLP_CV) {
        // Store the column index of the violated variable
        // violated.at(i - 1) = i;
        violated.push_back(i);
        printer += "variable x[" + std::to_string(i) + "] integrality violated";
      }
    }
  }

  // Constant (shift) term
  printMe += std::to_string(glp_get_obj_coef(prob, 0)) + " = " +
             std::to_string(glp_get_obj_val(prob));
  spdlog::info(printMe);

  // The variables vector is 0
  if (violated.empty()) {
    if (initial) {
      spdlog::info("OPTIMAL IP SOLUTION FOUND: on initial relaxation");
    } else {
      spdlog::info("IP SOLUTION FOUND");
    }

    return std::make_pair(1, violated);
  }

  spdlog::info(printer);

  return std::make_pair(0, violated);
}

/*
 * GLPK does not have a way to get the terminal output flag, but it does return
 * it when set so toggle the state, set it back to the original state, and
 * return the flag
 */
int getGlpTerm() {
  int flag = glp_term_out(GLP_ON);
  glp_term_out(flag);

  return flag;
}
