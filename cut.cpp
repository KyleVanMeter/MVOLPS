#include "cut.h"
#include <cassert>
#include "util.h"
#include "spdlog/spdlog.h"

int CutPool::addToPool(CutContainer cut) {
  _cuts.push_back(cut);
  return _cuts.size();
}

int CutPool::addCutConstraint(glp_prob *in, int cID) {
  if (_cuts.size() == 0) {
    return -1;
  }

  if (cID < 0) {
    // TODO
    // Do some heuristic to pick the best cut and use cID as its index in the
    // _cuts vector
    cID = _cuts.size()-1;
  }

  int index = glp_add_rows(in, 1);
  CutContainer selectedCut = _cuts.at(cID);
  assert(selectedCut.inds.size() == selectedCut.vals.size());

  spdlog::info(sstr("Adding cut of size ", selectedCut.inds.size(), " versus row length of: ", glp_get_num_cols(in)));
  std::string printMe = "val: [";
  for (auto &i : selectedCut.vals) {
    printMe += std::to_string(i) + " ";
  }
  printMe += "], ind: [";
  for (auto &i : selectedCut.inds) {
    printMe += std::to_string(i) + " ";
  }
  printMe += "]";

  spdlog::debug(sstr("Adding cut: ", printMe));

  glp_set_mat_row(in, index, selectedCut.inds.size()-1, selectedCut.inds.data(),
                  selectedCut.vals.data());
  //glp_set_row_bnds(in, index, GLP_UP, 0, selectedCut.lb);
  glp_set_row_bnds(in, index, GLP_LO, selectedCut.lb, 0);

  return cID;
}
