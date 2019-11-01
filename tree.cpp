#include "tree.h"

Node *newNode(Node *parent) {
  Node *node = new Node;
  node->parent = parent;

  return parent;
}
