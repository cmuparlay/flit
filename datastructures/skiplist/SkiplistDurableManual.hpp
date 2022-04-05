#ifndef SKIPLIST_DURABLE_MANUAL_HPP_
#define SKIPLIST_DURABLE_MANUAL_HPP_

#include <assert.h>
#include <atomic>
#include <bits/stdc++.h>

#include <common/ssmem_wrapper.hpp>
#include <persist/persist_offset.hpp>

template <typename T, template<typename, bool> typename PERSIST>
class alignas(ALIGNMENT) SkiplistDurableManual {
private:
    static const int MAX_LEVEL = 64;
    static unsigned int random_seed;

    struct alignas(ALIGNMENT) Node {
        persist<int, flush_option::no_flush> key;
        persist <T, flush_option::no_flush> val;
        persist<int, flush_option::no_flush> toplevel;
        PERSIST<std::atomic<Node*>, flush_option::no_flush> next[MAX_LEVEL];

        Node(int k, T v, int topl) : key(k), val(v), toplevel(topl) {
            for (int i = 0; i < MAX_LEVEL; i++) {
                next[i].store_non_atomic(nullptr);
            }
            FLUSH_STRUCT(this);
            assert((((uintptr_t) this) % (ALIGNMENT)) == 0); // check alignment
        }

        Node(int k, T v, Node* n, int topl) : key(k), val(v), toplevel(topl) {
            for (int i = 0; i < MAX_LEVEL; i++) {
                next[i].store_non_atomic(n);
            }
            FLUSH_STRUCT(this);
            assert((((uintptr_t) this) % (ALIGNMENT)) == 0); // check alignment
        }
    };
    
    static thread_local Node* nodes[1024];
    persist<Node *> head;

    Node *getAdd(Node *n) {
        long node = (long) n;
        return (Node *) (node & ~(0x1L)); // clear bit to get the real address
    }

    bool getMark(Node *n) {
        long node = (long) n;
        return (bool) (node & 0x1L);
    }

    Node *mark(Node *n) {
        long node = (long) n;
        node |= 0x1L;
        return (Node *) node;
    }

    bool CAS_next_flushed(Node *node, Node *exp, Node *n, int i) {
        return node->next[i].compare_exchange_strong(exp, n, std::memory_order_seq_cst,
                                                     flush_option::flush);
    }

    bool CAS_next_unflushed(Node *node, Node *exp, Node *n, int i) {
        return node->next[i].compare_exchange_strong(exp, n, std::memory_order_seq_cst,
                                                     flush_option::no_flush);
    }

    Node *getNext(Node *n, int i) {
        return n->next[i].load(std::memory_order_acquire);
    }

public:

    SkiplistDurableManual(int) : SkiplistDurableManual() {}
    SkiplistDurableManual() {
        Node *min, *max;
        max = static_cast<Node*>(ssmem.alloc(sizeof(Node))); 
        new(max) Node(INT_MAX, 0, nullptr, MAX_LEVEL);
        FLUSH_STRUCT(max);
        min = static_cast<Node*>(ssmem.alloc(sizeof(Node))); 
        new(min) Node(INT_MIN, 0, max, MAX_LEVEL);
        this->head = min;
        FLUSH_STRUCT(&this->head);
        assert((((uintptr_t) this) % (ALIGNMENT)) == 0); // check alignment
    }

    ~SkiplistDurableManual() {
        Node *node, *next;
        node = this->head;
        while (node != nullptr) {
            next = getNext(node, 0);
            ssmem.free(node);
            node = next;
        }
    }

    // only safe to run queiscently
    int size() {
      int size = 0;
      Node *node;
      node = getAdd(getNext(head, 0));
      while (getNext(node, 0) != nullptr) {
        if (!getMark(getNext(node, 0))) {
          size++;
        }
        node = getAdd(getNext(node, 0));
      }
      return size;
    }

    // only safe to run queiscently
    long long keySum() {
      long long sum = 0;
      Node *node;
      node = getAdd(getNext(head, 0));
      while (getNext(node, 0) != nullptr) {
        if (!getMark(getNext(node, 0))) {
          sum+=node->key;
        }
        node = getAdd(getNext(node, 0));
      }
      return sum;
    }

    T get(int key) {
        OperationLifetime op;
        Node *succs[MAX_LEVEL], *preds[MAX_LEVEL];
        bool exists = search_no_cleanup(key, preds, succs);
        preds[0]->next[0].flush_if_needed();
        succs[0]->next[0].flush_if_needed();
        if (exists) {
            return succs[0]->val;
        } else {
            return 0;
        }
    }

    bool contains(int key) {
        return get(key) != 0; // TODO: what if the value of the key is 0?
    }

    bool remove(int key) {
        OperationLifetime op;

        Node *succs[MAX_LEVEL], *preds[MAX_LEVEL];
        bool found = search(key, preds, succs);
        preds[0]->next[0].flush_if_needed();
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
        preds[0]->next[0].flush_if_needed();

        if (found) {
            return false;
        }

        newNode = static_cast<Node *>(ssmem.alloc(sizeof(Node)));
        new(newNode) Node(key, val, nullptr, get_rand_level());

        for (int i = 0; i < newNode->toplevel; i++) {
            if (i == 0)
                newNode->next[i].store_non_atomic(succs[i], flush_option::flush);
            else
                newNode->next[i].store_non_atomic(succs[i], flush_option::no_flush);
        }

        /* Node is visible once inserted at lowest level */
        Node *before = getAdd(succs[0]);
        if (!CAS_next_flushed(preds[0], before, newNode, 0)) {
            ssmem.free(newNode);
            goto retry;
        }
        for (int i = 1; i < newNode->toplevel; i++) {
            while (true) {
                pred = preds[i];
                succ = succs[i];
                //someone has already removed that node
                if (getMark(getNext(newNode, i))) {
                    return true;
                }
                if (CAS_next_unflushed(pred, succ, newNode, i))
                    break;
                search(key, preds, succs);
            }
        }

        return true;
    }

    static std::string get_name() {
        return "Skiplist Manual, " + PERSIST<std::atomic<int>, flush_option::flush>::get_name();
    }





private:

    bool search(int key, Node **left_list, Node **right_list) {
        Node *left_parent, *left, *left_next, *right = nullptr, *right_next;
        retry:
        left_parent = this->head;
        left = this->head;
        int num_nodes = 0;

        for (int i = MAX_LEVEL - 1; i >= 0; i--) {
            left_next = getNext(left, i);

            if (getMark(left_next)) {
                goto retry;
            }
            /* Find unmarked node pair at this level - left and right */
            for (right = left_next;; right = right_next) {
                /* Skip a sequence of marked nodes */
                right_next = getNext(right, i);
                while (getMark(right_next)) {
                    right = getAdd(right_next);
                    right_next = getNext(right, i);
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
                for (int j = 0; j < num_nodes; j++) { //flush if nodes are not adjacent
                    nodes[j]->next[0].flush_if_needed();
                }
	    }
            bool cas = false;
            /* Ensure left and right nodes are adjacent */
            if (left_next != right) {
                if (i == 0) {
                    cas = CAS_next_flushed(left, left_next, right, 0);
                } else {
                    cas = CAS_next_unflushed(left, left_next, right, i);
                }
                if (!cas) {
                    goto retry;
                }
            }

            if (i == 0 && cas) {
                for (int j = 0; j < num_nodes - 2; j++) {
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
        int i = MAX_LEVEL - 1;
        for (; i >= 0; i--) {
            left_next = getAdd(getNext(left, i));
            right = left_next;
            while (true) {
                if (!getMark(getNext(right, i))) {
                    if (right->key >= key) {
                        break;
                    }
                    left = right;
                } else { //right is marked but we want adjascent nodes - critical only in level 0
                    if (i == 0) {
                        if (right->key >= key) {
                            break;
                        }
                        left = right;
                    }
                }
                right = getAdd(getNext(right, i));
            }

            if (left_list != nullptr) {
                left_list[i] = left;
            }
            if (right_list != nullptr) {
                right_list[i] = right;
            }
        }
        return (right->key == key && !getMark(getNext(right, 0)));
    }


    inline bool mark_node_ptrs(Node *n) {
        bool cas = false;
        Node *n_next;
        for (int i = n->toplevel - 1; i >= 0; i--) {
            do {
                n_next = getNext(n, i);
                if (getMark(n_next)) {
                    n->next[0].flush_if_needed();
                    cas = false;
                    break;
                }
                Node *before = getAdd(n_next);
                Node *after = mark(n_next);
                cas = CAS_next_unflushed(n, before, after, i); //no need to persist because search iafter already does
            } while (!cas);
        }
        return (cas); /* if I was the one that marked lvl 0 */
    }

    int get_rand_level() {
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

};

template<typename T, template<typename, bool> typename PERSIST> unsigned int SkiplistDurableManual<T, PERSIST>::random_seed = 323423;
template<typename T, template<typename, bool> typename PERSIST> thread_local typename SkiplistDurableManual<T, PERSIST>::Node* SkiplistDurableManual<T, PERSIST>::nodes[1024];


#endif
