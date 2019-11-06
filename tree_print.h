#include "tree.hh"
#include <iostream>

template <typename T>
void PrettyPrintTree(const tree<T> &in, typename tree<T>::iterator root);

template <typename T>
void PrettyPrintTree(const tree<T> &in, typename tree<T>::iterator root) {
  while (root != in.end()) {
    for (int i = 0; i < in.depth(root); i++) {
      std::cout << " ";
    }
    std::cout << "-" << root.node->data << std::endl;
    root++;
  }
}
