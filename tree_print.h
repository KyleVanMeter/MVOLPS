#include "tree.hh"
#include <iostream>

template <typename T, typename CallBackFunc>
void PrettyPrintTree(const tree<T> &in, typename tree<T>::iterator root,
                     CallBackFunc &&cb);

/*
 * Template functor needed for printing trees made up of objects without the
 * overloaded << operator
 */
template <typename T, typename CallBackFunc>
void PrettyPrintTree(tree<T> &in, typename tree<T>::iterator root,
                     CallBackFunc &&cb) {
  while (root != in.end()) {
    for (int i = 0; i < in.depth(root); i++) {
      std::cout << " ";
    }
    std::cout << "-" << cb(root.node->data) << std::endl;
    root++;
  }
}

/*
 * What is the point of templates if you have to copy/paste code anyways? Needed
 * for printing of trees templated by objects that have the << operator
 */
template <typename T>
void PrettyPrintTree(tree<T> &in, typename tree<T>::iterator root) {
  while (root != in.end()) {
    for (int i = 0; i < in.depth(root); i++) {
      std::cout << " ";
    }
    std::cout << "-" << root.node->data << std::endl;
    root++;
  }
}
