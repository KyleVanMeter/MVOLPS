#include "InputParser.h"
#include "BranchAndBound.h"
#include "util.h"
#include "glpk.h"

#include <algorithm>
#include <iostream>
#include <string>

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

    glp_prob *prob;
    if (temp == ".lp") {
      prob = initProblem(fn, MVOLP::LP);
    } else if (temp == ".mps") {
      prob = initProblem(fn, MVOLP::MPS);
    } else {
      std::cout << "Unrecognized filetype\n";

      return -1;
    }

    std::cout << branchAndBound(prob) << std::endl;

  } else {
    std::cout << "see ./MVOLPS -h for usage\n";
  }

  return 0;
}
