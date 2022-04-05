
#ifndef SKIPLIST_ORIGINAL_HPP_
#define SKIPLIST_ORIGINAL_HPP_

#include<bits/stdc++.h> 
#include<atomic>

#include<common/rand_r_32.h>
#include<common/ssmem_wrapper.hpp>

template <class T> class alignas(ALIGNMENT) SkiplistOriginal {
private:

  class alignas(ALIGNMENT) Node;
  static unsigned int random_seed;
  static const int MAX_LEVEL = 64;
  static thread_local Node* nodes[1024];

  class alignas(ALIGNMENT) Node {
  public:
    int key;
    T val;
    unsigned char toplevel;
    std::atomic<Node*> next[MAX_LEVEL];
    
    Node() {
      assert((((uintptr_t) this) % (ALIGNMENT)) == 0); // check alignment
    }

    Node(int k, T v, int topl) {
      key = k;
      val = v;
      toplevel = topl;
      assert((((uintptr_t) this) % (ALIGNMENT)) == 0); // check alignment
    }
    Node(int k, T v, Node* n, int topl) {
      key = k;
      val = v;
      toplevel = topl;
      for (int i = 0; i < MAX_LEVEL; i++)
      {
        next[i].store(n);
      }
      assert((((uintptr_t) this) % (ALIGNMENT)) == 0); // check alignment
    }
    
    bool CASNext(Node* exp, Node* n, int i) {
      Node* old = next[i].load();
      if (exp != old) { 
        return false;
      }
      bool ret = next[i].compare_exchange_strong(exp, n);
      return ret;
    }
    Node* getNext(int i) {
      Node* n = next[i].load();
      return n;
    }
  };

  int get_rand_level()
  {
    int level = 1;
    for (int i = 0; i < MAX_LEVEL - 1; i++)
    {
      if ((rand_r_32(&random_seed) % 101) < 50)
        level++;
      else
        break;
    }
          /* 1 <= level <= MAX_LEVEL */

    return level;
  }

  static inline bool isMarked(Node* ptr)
  {
    auto ptrLong = (long long)(ptr);
    return ((ptrLong & 1) == 1);
  }

  static inline Node* getCleanReference(Node* ptr)
  {
    auto ptrLong = (long long)(ptr);
    ptrLong &= ~1;
    return (Node *)(ptrLong);
  }

  static inline Node* getMarkedReference(Node* ptr)
  {
    auto ptrLong = (long long)(ptr);
    ptrLong |= 1;
    return (Node *)(ptrLong);
  }

public:

  SkiplistOriginal(int) : SkiplistOriginal() {}

  SkiplistOriginal() {
    Node *min, *max;
    max = static_cast<Node*>(ssmem.alloc(sizeof(Node), false));
    //max = static_cast<Node*>(malloc(sizeof(Node)));
    new (max) Node(INT_MAX, 0, nullptr, MAX_LEVEL);
    min = static_cast<Node*>(ssmem.alloc(sizeof(Node), false));
    //min = static_cast<Node*>(malloc(sizeof(Node)));
    new (min) Node(INT_MIN, 0, max, MAX_LEVEL);
    this->head = min;
    assert((((uintptr_t) this) % (ALIGNMENT)) == 0); // check alignment
  }
  ~SkiplistOriginal() {       
    Node *node, *next;
    node = this->head;
    while (node != NULL)
    {
      next = node->getNext(0);
      ssmem.free(node, false);
      node = next;
    }
            // ssfree(set);
  }
  long long size() {
    int size = 0;
    Node *node;
    node = getCleanReference(this->head->getNext(0));
    while (node->getNext(0) != nullptr) {
      if (!isMarked(node->getNext(0))) {
        size++;
      }
      node = getCleanReference(node->getNext(0));
    }
    return size;
  }
  long long keySum() {
    long long sum = 0;
    Node *node;
    node = getCleanReference(this->head->getNext(0));
    while (node->getNext(0) != nullptr) {
      if (!isMarked(node->getNext(0))) {
        sum+=node->key;
      }
      node = getCleanReference(node->getNext(0));
    }
    return sum;
  }
  T get(int key) {
    Node *left = left_search(key);
    return left->val;
  }
  bool contains(int key) {
    Node *left = left_search(key);
    if(left->key == key)
      return true;
    else
      return false;
  }
  bool remove(int key) {
    Node *succs[MAX_LEVEL];
    bool found = search_no_cleanup_succs(key, succs);
    if (!found) {
      return false;
    }
    Node *node_del = succs[0];
    bool my_delete = mark_node_ptrs(node_del);
    if (my_delete) {
      search(key, nullptr, nullptr);
      return true;
    }
    return false;
  }

  bool add(int key, T val) {
    Node *newNode, *pred, *succ;
    Node *succs[MAX_LEVEL], *preds[MAX_LEVEL];
    bool found;

    retry:
    found = search(key, preds, succs);
    if (found) {
      return false;
    }
    newNode = static_cast<Node*>(ssmem.alloc(sizeof(Node), false)); 
    //newNode = static_cast<Node*>(malloc(sizeof(Node))); 
    new (newNode) Node(key, val, nullptr, get_rand_level());

    for (int i = 0; i < newNode->toplevel; i++) {
      newNode->next[i] = succs[i];
    }

          /* Node is visible once inserted at lowest level */
    Node *before = getCleanReference(succs[0]);
    if (!preds[0]->CASNext(before, newNode, 0)) {
      ssmem.free(newNode, false);
      goto retry;
    }
    for (int i = 1; i < newNode->toplevel; i++) {
      while (true) {
        pred = preds[i];
        succ = succs[i];
                        //someone has already removed that node
        if (isMarked(newNode->getNext(i))) {
          return true;
        }
        if (pred->CASNext(succ, newNode, i))
          break;
        search(key, preds, succs);
      }
    }
    return true;
  }

  static std::string get_name() {
      return "Skiplist Original";
  }

private:
  Node *head;

  bool search(int key, Node **left_list, Node **right_list) {
    Node *left, *left_next, *right = nullptr, *right_next;
    retry:
    left = this->head;
    int num_nodes = 0;
    for (int i = MAX_LEVEL - 1; i >= 0; i--) {
      left_next = left->getNext(i);

      if (isMarked(left_next)) {
        goto retry;
      }
                    /* Find unmarked node pair at this level - left and right */
      for (right = left_next;; right = right_next) {
                        /* Skip a sequence of marked nodes */
        right_next = right->getNext(i);
        while (isMarked(right_next)) {
          right = getCleanReference(right_next);
          right_next = right->getNext(i);
          if (i == 0){
            nodes[num_nodes++] = right;
          }
        }
        if (right->key >= key) {
          break;
        }
        left = right;
        left_next = right_next;
        num_nodes = 0;
      }
      bool cas = false;
                    /* Ensure left and right nodes are adjacent */
      if (left_next != right) {
        bool cas = left->CASNext(left_next, right, i);
        if (!cas) {
          goto retry;
        }
      }
      if (i == 0 && cas) {
        for(int j = 0; j < num_nodes-1; j++) {
          ssmem.free(nodes[j], false);
        }
      }
      if (left_list != nullptr) {
        left_list[i] = left;
      }
      if (right_list != nullptr) {
        right_list[i] = right;
      }
    }
    return (right->key == key);
  }
  bool search_no_cleanup(int key, Node **left_list, Node **right_list) {
    Node *left, *left_next, *right = nullptr;
    left = this->head;
    for (int i = MAX_LEVEL - 1; i >= 0; i--) {
      left_next = getCleanReference(left->getNext(i));
      right = left_next;
      while (true) {
        if (!isMarked(right->getNext(i))) {
          if (right->key >= key) {
            break;
          }
          left = right;
        }
        right = getCleanReference(right->getNext(i));
      }

      if (left_list != nullptr) {
        left_list[i] = left;
      } 
      if (right_list != nullptr) {
        right_list[i] = right;
      }
    }  
    return (right->key == key);
  }
  bool search_no_cleanup_succs(int key, Node **right_list) {
    Node *left, *left_next, *right = nullptr;
    left = this->head;
    for (int i = MAX_LEVEL - 1; i >= 0; i--) {
      left_next = getCleanReference(left->getNext(i));
      right = left_next;
      while (true) {
        if (!isMarked(right->getNext(i))) {
          if (right->key >= key) {
            break;
          }
          left = right;
        }
        right = getCleanReference(right->getNext(i));
      }
      right_list[i] = right;
    }
    return (right->key == key);
  }
  Node* left_search(int key) {
    Node *left = nullptr, *left_prev;
    left_prev = this->head;
    for (int lvl = MAX_LEVEL - 1; lvl >= 0; lvl--) {
      left = getCleanReference(left_prev->getNext(lvl));
      while (left->key < key || isMarked(left->getNext(lvl))) {
        if (!isMarked(left->getNext(lvl))) {
          left_prev = left;
        }
        left = getCleanReference(left->getNext(lvl));
      }
      if (left->key == key) {
        return left;
      }
    }
    return left_prev;
  }
  inline bool mark_node_ptrs(Node *n) {
    bool cas = false;
    Node *n_next;
    for (int i = n->toplevel - 1; i >= 0; i--) {
      do {
        n_next = n->getNext(i);
        if (isMarked(n_next)) {
          cas = false;
          break;
        }
        Node *before = getCleanReference(n_next);
        Node *after = getMarkedReference(n_next);
        cas = n->CASNext(before, after, i);
      } while (!cas);
    }
            return (cas); /* if I was the one that marked lvl 0 */
  }
} __attribute__((aligned((64))));

template<typename T> unsigned int SkiplistOriginal<T>::random_seed = 323423;
template<typename T> thread_local typename SkiplistOriginal<T>::Node* SkiplistOriginal<T>::nodes[1024];

#endif /* SKIPLIST_ORIGINAL_HPP_ */