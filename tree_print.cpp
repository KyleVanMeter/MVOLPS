#include "tree_print.h"

template<typename T>
void PrettyPrintTree(tree<T> &in, typename tree<T>::iterator root) {
  while (root != in.end()) {
    for (int i = 0; i < in.depth(root); i++) {
      std::cout << " ";
    }
    std::cout << "-" << root.node->data << std::endl;
    root++;
  }
}
