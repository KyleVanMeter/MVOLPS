#include "util.h"

#include "bs.h"
//#include "BranchAndBound.h"
#include "InputParser.h"
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
              << "  --server [port] start server on given port\n"
              << "Output verbosity options:\n"
              << "  -s/--silent\n"
              << "  -v/--verbose\n"
              << "  -d/--debug\n"
              << "  -so/--solver-output\n\n"
              << "Algorithm strategy options:\n"
              << "  -vs [{0|1|2}]\n"
              << "    0. vars are picked on order\n"
              << "    1. vars are picked on fractional part closeness to 0.5\n"
              << "    2. vars are picked on greatest impact on obj. function\n"
              << "  -bs [{0|1}]\n"
              << "    0. nodes are picked for DFS (FIFO/queue)\n"
              << "    1. nodes are picked for best-FS (greatest z-value)\n"
              << "  -cm [{0|1}]\n"
              << "    0. disable cut generation\n"
              << "    1. generate Gomory mixed integer\n"
              << "    -cf [0...1]\n"
              << "      Percentage of generated cuts to be added per node"
              << "Help:\n"
              << "  -h/--help\n";

    return 0;
  }

  glp_term_out(GLP_OFF);

  if (input.CMDOptionExists("-v") || input.CMDOptionExists("--verbose")) {
    spdlog::set_level(spdlog::level::info);
  }

  if (input.CMDOptionExists("-d") || input.CMDOptionExists("--debug")) {
    spdlog::set_level(spdlog::level::debug);
    glp_term_out(GLP_ON);
  }

  if (input.CMDOptionExists("-s") || input.CMDOptionExists("--silent")) {
    spdlog::set_level(spdlog::level::off);
  }

  if (input.CMDOptionExists("-so") ||
      input.CMDOptionExists("--solver-output")) {
    glp_term_out(GLP_ON);
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

    MVOLP::ParameterObj params(prob);

    if (input.CMDOptionExists("--server")) {
      std::string option = input.getCMDOption("--server");
      int port = std::stoi(option);

      params.setServerPort(port);
    }

    if (input.CMDOptionExists("-bs")) {
      std::string option = input.getCMDOption("-bs");
      int opt = std::stoi(option);

      if (opt == MVOLP::param::NodeStratType::BEST) {
        params.setNodeStrat(MVOLP::param::NodeStratType::BEST);
      } else if (opt == MVOLP::param::NodeStratType::DFS) {
        params.setNodeStrat(MVOLP::param::NodeStratType::DFS);
      } else {
        spdlog::error("Unknown parameter value for -bs");
        return -1;
      }
    }

    if (input.CMDOptionExists("-vs")) {
      std::string option = input.getCMDOption("-vs");
      int opt = std::stoi(option);

      if (opt == MVOLP::param::VarStratType::VFP) {
        params.setVarStrat(MVOLP::param::VarStratType::VFP);
      } else if (opt == MVOLP::param::VarStratType::VGO) {
        params.setVarStrat(MVOLP::param::VarStratType::VGO);
      } else if (opt == MVOLP::param::VarStratType::VO) {
        params.setVarStrat(MVOLP::param::VarStratType::VO);
      } else {
        spdlog::error("Unknown parameter value for -vs");
        return -1;
      }
    }

    if (input.CMDOptionExists("-cm")) {
      std::string option = input.getCMDOption("-cm");
      int opt = std::stoi(option);

      if (opt == MVOLP::param::CutStratType::GMI) {
        params.setCutStrat(MVOLP::param::CutStratType::GMI);
      } else if (opt == MVOLP::param::CutStratType::NONE) {
        params.setCutStrat(MVOLP::param::CutStratType::NONE);
      } else {
        spdlog::error("Unknown parameter value for -cm");
        return -1;
      }

      if (input.CMDOptionExists("-cf")) {
        std::string option = input.getCMDOption("-cf");
        double chance = std::stod(option);
        if (!((chance <= 1.0) && (chance >= 0.0))) {
          spdlog::error("Cut Frequency parameter must be in range [0.0, 1.0]");
          return -1;
        }

        params.setCutChance(chance);
      } else {
        params.setCutChance(1.0);
      }
    } else {
      params.setCutStrat(MVOLP::param::CutStratType::NONE);
    }

    branchAndBound(prob, params);
    //nothing(prob, params);

  } else {
    std::cout << "see ./MVOLPS -h for usage\n";
  }

  return 0;
}
