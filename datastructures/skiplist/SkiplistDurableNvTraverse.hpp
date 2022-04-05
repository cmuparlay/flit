
#ifndef SKIPLIST_DURABLE_NVTRAVERSE_HPP_
#define SKIPLIST_DURABLE_NVTRAVERSE_HPP_

#include<bits/stdc++.h> 
#include<atomic>

#include<common/rand_r_32.h>
#include<common/ssmem_wrapper.hpp>
#include<persist/persist_offset.hpp>

template <class T, template<typename, bool> typename PERSIST> 
class alignas(ALIGNMENT) SkiplistDurableNvTraverse {
private:

  struct alignas(ALIGNMENT) Node;
  
  static const int MAX_LEVEL = 64;
  static unsigned int random_seed;
  static thread_local Node* nodes[1024];

  struct alignas(ALIGNMENT) Node {
    persist<int, flush_option::no_flush> key;
    persist<T, flush_option::no_flush> val;
    persist<unsigned char> toplevel;
    PERSIST<std::atomic<Node*>, flush_option::no_flush> next[MAX_LEVEL+1];

    Node(int k, T v, Node* n, int topl)  : key(k), val(v), toplevel(topl) {
      for (int i = 0; i < MAX_LEVEL+1; i++)
      {
        next[i].store_non_atomic(n);
      }
      FLUSH_STRUCT(this);
      assert((((uintptr_t) this) % (ALIGNMENT)) == 0); // check alignment
    }

    bool CASNext(Node* exp, Node* n, int i) {
      if (i == 0)
        return next[i].compare_exchange_strong(exp, n, std::memory_order_seq_cst,
                                                 flush_option::flush);
      else
        return next[i].compare_exchange_strong(exp, n);
    }
    Node* getNextF(int i) {
      Node* n = next[i].load(std::memory_order_acquire, flush_option::flush);
      return n;
    }
    Node* getNext(int i) {
      Node* n = next[i].load();
      return n;
    }

    void flush() {
      bool do_flush = false;
      for(int i = 0; i < toplevel; i++)
        if(next[i].is_flush_needed()) {
          do_flush = true;
          break;
        }
      if(do_flush) FLUSH_STRUCT(this);
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

  SkiplistDurableNvTraverse(int) : SkiplistDurableNvTraverse() {}
  
  SkiplistDurableNvTraverse() {
    Node *min, *max;
    max = static_cast<Node*>(ssmem.alloc(sizeof(Node)));
    new (max) Node(INT_MAX, 0, nullptr, MAX_LEVEL);
    min = static_cast<Node*>(ssmem.alloc(sizeof(Node)));
    new (min) Node(INT_MIN, 0, max, MAX_LEVEL);
    FLUSH_STRUCT(max);
    FLUSH_STRUCT(min);
    this->head = min;
    FENCE();
    assert((((uintptr_t) this) % (ALIGNMENT)) == 0); // check alignment
  }
  ~SkiplistDurableNvTraverse() {       
    Node *node, *next;
    node = this->head;
    while (node != nullptr)
    {
      next = node->getNext(0);
                //delete node;
      node = next;
    }
            // ssfree(set);
  }
  int size() {
    int size = 0;
    Node *node;
    node = static_cast<Node *>(getCleanReference(this->head->getNext(0)));
    while (node->getNext(0) != nullptr) {
      if (!isMarked(node->getNext(0))) {
        size++;
      }
      node = static_cast<Node *>(getCleanReference(node->getNext(0)));
    }
    return size;
  }
  long long keySum() {
    long long sum = 0;
    Node *node;
    node = static_cast<Node *>(getCleanReference(this->head->getNext(0)));
    while (node->getNext(0) != nullptr) {
      if (!isMarked(node->getNext(0))) {
        sum+=node->key;
      }
      node = static_cast<Node *>(getCleanReference(node->getNext(0)));
    }
    return sum;
  }
  T get(int key) {
    Node *succs[MAX_LEVEL], *preds[MAX_LEVEL];
    bool exists = search_no_cleanup(key, preds, succs);
    preds[0]->flush();
    succs[0]->flush();
    if (exists) {
      return succs[0]->val;
    } else {
      return 0;
    }
  }
  bool find(int key) {
    return get(key) != 0;
  }
  bool contains(int key) {
    OperationLifetime op;
    Node *succs[MAX_LEVEL], *preds[MAX_LEVEL];
    bool exists = search_no_cleanup(key, preds, succs);
    preds[0]->flush();
    succs[0]->flush();
    if (exists) {
      return true;
    } else {
      return false;
    }
  }
  bool remove(int key) {
    OperationLifetime op;
    Node *succs[MAX_LEVEL];
    bool found = search(key, nullptr, succs);
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
    OperationLifetime op;
    Node *newNode, *pred, *succ;
    Node *succs[MAX_LEVEL], *preds[MAX_LEVEL];
    bool found;

    retry:
    found = search(key, preds, succs);
    if (found) {
      return false;
    }
    newNode = static_cast<Node*>(ssmem.alloc(sizeof(Node))); 
    new (newNode) Node(key, val, nullptr, get_rand_level());

    for (int i = 0; i < newNode->toplevel; i++) {
      newNode->next[i].store_non_atomic(succs[i]);
    }
    FLUSH_STRUCT(newNode);

          /* Node is visible once inserted at lowest level */
    Node *before = static_cast<Node *>(getCleanReference(succs[0]));
    if (!preds[0]->CASNext(before, newNode, 0)) {
      ssmem.free(newNode);
      goto retry;
    }
    for (int i = 1; i < newNode->toplevel; i++) {
      while (true) {
        pred = preds[i];
        succ = succs[i];
                        //someone has already removed that node
        if (isMarked(newNode->getNextF(i))) {
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
      return "Skiplist NvTraverse, " + PERSIST<std::atomic<int>, flush_option::flush>::get_name();
  }

private:
  persist<Node*> head;

  bool search(int key, Node **left_list, Node **right_list) {
    Node *left_parent, *left, *left_next, *right = nullptr, *right_next;
    retry:
    left_parent = this->head;
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
          right = static_cast<Node *>(getCleanReference(right_next));
          right_next = right->getNext(i);
          if (i == 0) {
            nodes[num_nodes++] = right;
          }
        }
        if (right->key >= key) {
          break;
        }
        left_parent = left;
        left = right;
        left_next = right_next;
        num_nodes = 0;
      }
      if (i == 0) {
        nodes[num_nodes++] = left_parent;
        for (int j = 0; j < num_nodes; j++) {
         nodes[j]->flush();
       }
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
      for (int j = 0; j < num_nodes-2; j++) {
       ssmem.free(nodes[j]);
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
    left_next = static_cast<Node *>(getCleanReference(left->getNext(i)));
    right = left_next;
    while (true) {
      if (!isMarked(right->getNext(i))) {
        if (right->key >= key) {
          break;
        }
        left = right;
      }
      right = static_cast<Node *>(getCleanReference(right->getNext(i)));
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
    left_next = static_cast<Node *>(getCleanReference(left->getNext(i)));
    right = left_next;
    while (true) {
      if (!isMarked(right->getNext(i))) {
        if (right->key >= key) {
          break;
        }
        left = right;
      }
      right = static_cast<Node *>(getCleanReference(right->getNext(i)));
    }
    right_list[i] = right;
  }
  return (right->key == key);
}
Node* left_search(int key) {
  Node *left = nullptr, *left_prev;
  left_prev = this->head;
  for (int lvl = MAX_LEVEL - 1; lvl >= 0; lvl--) {
    left = static_cast<Node *>(getCleanReference(left_prev->getNext(lvl)));
    while (left->key < key || isMarked(left->getNext(lvl))) {
      if (!isMarked(left->getNext(lvl))) {
        left_prev = left;
      }
      left = static_cast<Node *>(getCleanReference(left->getNext(lvl)));
    }
    if ((left->key == key)) {
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
      n_next = n->getNextF(i);
      if (isMarked(n_next)) {
        cas = false;
        break;
      }
      Node *before = static_cast<Node *>(getCleanReference(n_next));
      Node *after = static_cast<Node *>(getMarkedReference(n_next));
      cas = n->CASNext(before, after, i);
    } while (!cas);
  }
            return (cas); /* if I was the one that marked lvl 0 */
}
} __attribute__((aligned((64))));

template<typename T, template<typename, bool> typename PERSIST> unsigned int SkiplistDurableNvTraverse<T, PERSIST>::random_seed = 323423;
template<typename T, template<typename, bool> typename PERSIST> thread_local typename SkiplistDurableNvTraverse<T, PERSIST>::Node* SkiplistDurableNvTraverse<T, PERSIST>::nodes[1024];

#endif /* SKIPLIST_DURABLE_NVTRAVERSE_HPP_ */