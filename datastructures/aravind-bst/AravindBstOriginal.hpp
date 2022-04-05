
#ifndef ARAVIND_BST_ORIGINAL_
#define ARAVIND_BST_ORIGINAL_

#include <assert.h>
#include <atomic>
#include <bits/stdc++.h> 
#include <algorithm>

#include <common/ssmem_wrapper.hpp>

#define GC 1

// write up a easy traverse method, one for automatic and one for NVtraverse
// measure number of lines of code changed.

template <class T> 
class alignas(ALIGNMENT) AravindBstOriginal {
private:
  struct alignas(ALIGNMENT) Node{
      int key;
      T value;
      std::atomic<Node*> right;
      std::atomic<Node*> left;

      Node(int k, T v, Node* r, Node* l) : key(k), value(v), right(r), left(l) {
        assert((((uintptr_t) this) % (ALIGNMENT)) == 0); // check alignment
      }
  };

  struct seek_record_t{
      Node* ancestor;
      Node* successor;
      Node* parent;
      Node* leaf;
  };

  Node* root;

  static thread_local seek_record_t seek_record;

  static const int INF2 = INT_MAX;
  static const int INF1 = INT_MAX - 1;
  static const int INF0 = INT_MAX - 2;

  static inline bool GETFLAG(Node* ptr) {
    return (bool) (((uint64_t)ptr) & 1);
  }

  static inline bool GETTAG(Node* ptr) {
      return (bool) (((uint64_t)ptr) & 2);
  }

  static inline Node* FLAG(Node* ptr) {
      return (Node*) (((uint64_t)ptr) | 1);
  }

  static inline Node* TAG(Node* ptr) {
      return (Node*) (((uint64_t)ptr) | 2) ;
  }

  static inline Node* UNTAG(Node* ptr) {
      return (Node*) (((uint64_t)ptr) & 0xfffffffffffffffd);
  }

  static inline Node* ADDRESS(Node* ptr) {
      return (Node*) (((uint64_t)ptr) & 0xfffffffffffffffc);
  }

  Node* create_node(int k, T value, int initializing) {
      Node* new_node;
  #if GC == 1
      new_node = (Node*) ssmem.alloc(sizeof(Node), false);
  #else 
      new_node = (Node*) malloc(sizeof(Node));
  #endif
      if (new_node == NULL) {
          perror("malloc in bst create node");
          exit(1);
      }
      new (new_node) Node(k, value, NULL, NULL);
      return (Node*) new_node;
  }

public:
  AravindBstOriginal(int) : AravindBstOriginal() {}

  AravindBstOriginal() {
    Node* s;
    Node* inf0;
    Node* inf1;
    Node* inf2;
    root = create_node(INF2,0,1);
    s = create_node(INF1,0,1);
    inf0 = create_node(INF0,0,1);
    inf1 = create_node(INF1,0,1);
    inf2 = create_node(INF2,0,1);
    
    root->left = s;
    root->right = inf2;
    s->right = inf1;
    s->left= inf0;
    assert((((uintptr_t) this) % (ALIGNMENT)) == 0); // check alignment
  }

  void bst_seek(int key){
    seek_record_t seek_record_l;
    Node* node_s = ADDRESS(root->left);
    seek_record_l.ancestor = root;
    seek_record_l.successor = node_s; 
    seek_record_l.parent = node_s;
    seek_record_l.leaf = ADDRESS(node_s->left);

    Node* parent_field = (Node*) seek_record_l.parent->left;
    Node* current_field = (Node*) seek_record_l.leaf->left;
    Node* current = ADDRESS(current_field);


    while (current != NULL) {
        if (!GETTAG(parent_field)) {
            seek_record_l.ancestor = seek_record_l.parent;
            seek_record_l.successor = seek_record_l.leaf;
        }
        seek_record_l.parent = seek_record_l.leaf;
        seek_record_l.leaf = current;

        parent_field = current_field;
        if (key < current->key) {
            current_field= (Node*) current->left;
        } else {
            current_field= (Node*) current->right;
        }
        current=ADDRESS(current_field);
    }
    seek_record.ancestor=seek_record_l.ancestor;
    seek_record.successor=seek_record_l.successor;
    seek_record.parent=seek_record_l.parent;
    seek_record.leaf=seek_record_l.leaf;
  }

  bool contains(int key) {
     bst_seek(key);
     if (seek_record.leaf->key == key) {
          return true;
     } else {
          return false;
     }
  }


  bool add(int key, T val) {
      Node* new_internal = NULL;
      Node* new_node = NULL;
      uint created = 0;
      while (1) {
          bst_seek(key);
          if (seek_record.leaf->key == key) {
  #if GC == 1
              if (created) {
                  ssmem.free(new_internal, false);
                  ssmem.free(new_node, false);
              }
  #endif
              return false;
          }
          Node* parent = seek_record.parent;
          Node* leaf = seek_record.leaf;

          std::atomic<Node*>* child_addr;
          if (key < parent->key) {
            child_addr = &(parent->left); 
          } else {
              child_addr = &(parent->right); 
          }
          //if (likely(created==0)) {
          if (created==0) {
              new_internal=create_node(std::max(key,leaf->key),0,0);
              new_node = create_node(key,val,0);
              created=1;
          } else {
              new_internal->key=std::max(key,leaf->key);
          }
          if ( key < leaf->key) {
              new_internal->left = new_node;
              new_internal->right = leaf; 
          } else {
              new_internal->right = new_node;
              new_internal->left = leaf;
          }
   #ifdef __tile__
      MEM_BARRIER;
  #endif
          Node* old_val = ADDRESS(leaf);
          bool result = child_addr->compare_exchange_strong(old_val, ADDRESS(new_internal));
          if (result) {
              return true;
          }
          Node* chld = *child_addr; 
          if ( (ADDRESS(chld)==leaf) && (GETFLAG(chld) || GETTAG(chld)) ) {
              bst_cleanup(key); 
          }
      }
  }

  bool remove(int key) {
      bool injecting = true; 
      Node* leaf;
      //T val;
      while (1) {
          //std::cout << "remove loop" << std::endl;
          bst_seek(key);
          //val = seek_record.leaf->value;
          Node* parent = seek_record.parent;

          std::atomic<Node*>* child_addr;
          if (key < parent->key) {
              child_addr = &(parent->left); 
          } else {
              child_addr = &(parent->right); 
          }

          if (injecting == true) {
              //std::cout << "injecting" << std::endl;
              leaf = seek_record.leaf;
              if (leaf->key != key) {
                  return false;
              }
              Node* lf = ADDRESS(leaf);
              bool result = child_addr->compare_exchange_strong(lf, FLAG(lf));
              if (result) {
                  injecting = false;
                  bool done = bst_cleanup(key);
                  if (done) {
                      return true;
                  }
              } else {
                  Node* chld = *child_addr;
                  if ( (ADDRESS(chld) == leaf) && (GETFLAG(chld) || GETTAG(chld)) ) {
                      bst_cleanup(key);
                  }
              }
          } else {
              if (seek_record.leaf != leaf) {
                  return true; 
              } else {
                  bool done = bst_cleanup(key);
                  //std::cout << "cleanup" << std::endl;
                  if (done) {
                      return true;
                  }
              }
          }
      }
  }

  bool bst_cleanup(int key) {
      Node* ancestor = seek_record.ancestor;
      Node* successor = seek_record.successor;
      Node* parent = seek_record.parent;
      //Node* leaf = seek_record.leaf;

      std::atomic<Node*>* succ_addr;
      if (key < ancestor->key) {
          succ_addr = &(ancestor->left); 
      } else {
          succ_addr = &(ancestor->right); 
      }

      std::atomic<Node*>* child_addr;
      std::atomic<Node*>* sibling_addr;
      if (key < parent->key) {
         child_addr = &(parent->left);
         sibling_addr = &(parent->right);
      } else {
         child_addr = &(parent->right);
         sibling_addr = &(parent->left);
      }

      Node* chld = *(child_addr);
      if (!GETFLAG(chld)) {
          chld = *(sibling_addr);
          sibling_addr = child_addr;
      }
  //#if defined(__tile__) || defined(__sparc__)
      while (1) {
          Node* untagged = *sibling_addr;
          Node* tagged = TAG(untagged);
          bool res = sibling_addr->compare_exchange_strong(untagged, tagged);
          if (res) {
              break;
           }
      }
  //#else
  //    set_bit(sibling_addr,1);
  //#endif

      Node* sibl = *sibling_addr;
      Node* old_val = ADDRESS(successor);
      //std::cout << *succ_addr << " " << old_val << " " << successor << std::endl;
      if (succ_addr->compare_exchange_strong(old_val, UNTAG(sibl))) {
  #if GC == 1
      while(successor != parent) {
        successor = ADDRESS(successor);
        parent = ADDRESS(parent);
        Node* left = successor->left;
        Node* right = successor->right;
        ssmem.free(successor);
        if(GETFLAG(left)) {
          ssmem.free(ADDRESS(left));
          successor = ADDRESS(right);
        } else {
          ssmem.free(ADDRESS(right));
          successor = ADDRESS(left);
        }
      }

      ssmem.free(ADDRESS(chld), false);
      ssmem.free(ADDRESS(successor), false);
  #endif
          return true;
      }
      return false;
  }

  static std::string get_name() {
      return "Bst Original";
  }

  /* Functions for debugging and validation.
     Must be run in a quiescent state.
   */

  long long sizeHelper(Node* n) {
    if(n == nullptr) return 0;
    if(ADDRESS(n->left) == nullptr) return 1;
    return sizeHelper(ADDRESS(n->left)) + sizeHelper(ADDRESS(n->right));
  }

  long long size() {
    return sizeHelper(ADDRESS(ADDRESS(ADDRESS(root->left)->left)->left));
  }

  long long keySumHelper(Node* n) {
    if(n == nullptr) return 0;
    if(ADDRESS(n->left) == nullptr) return n->key;
    return keySumHelper(ADDRESS(n->left)) + keySumHelper(ADDRESS(n->right));
  }

  long long keySum() {
    return keySumHelper(ADDRESS(ADDRESS(ADDRESS(root->left)->left)->left));
  }  

};

template <class T>
thread_local typename AravindBstOriginal<T>::seek_record_t AravindBstOriginal<T>::seek_record;

#endif /* ARAVIND_BST_ORIGINAL_*/
