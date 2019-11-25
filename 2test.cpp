#include "InputParser.h"
#include "BranchAndBound.h"
#include "util.h"
#include "glpk.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <iostream>
#include <string>

int main(int argc, char **argv) {
  InputParser input(argc, argv);

  if (input.CMDOptionExists("-h") || input.CMDOptionExists("--help")) {
    std::cout << "Usage: MVOLPS [OPTION]\n"
              << "File input options:\n"
              << "  -f/--file [FILENAME.{mps|lp}]\n\n"
              << "Output verbosity options:\n"
              << "  -s/--silent\n"
              << "  -v/--verbose\n"
              << "  -d/--debug\n\n"
              << "Help:\n"
              << "  -h/--help\n";

    return 0;
  }

  if (input.CMDOptionExists("-v") || input.CMDOptionExists("--verbose")) {
    spdlog::set_level(spdlog::level::info);
  }

  if (input.CMDOptionExists("-d") || input.CMDOptionExists("--debug")) {
    spdlog::set_level(spdlog::level::debug);
  }

  if (input.CMDOptionExists("-s") || input.CMDOptionExists("--silent")) {
    spdlog::set_level(spdlog::level::off);
    glp_term_out(GLP_OFF);
  }

  if (input.CMDOptionExists("-f") || input.CMDOptionExists("--file")) {
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

    branchAndBound(prob);

  } else {
    std::cout << "see ./MVOLPS -h for usage\n";
  }

  return 0;
}
