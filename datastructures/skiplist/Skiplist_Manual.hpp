#ifndef SKIPLIST_DURABLE_MANUAL_HPP_
#define SKIPLIST_DURABLE_MANUAL_HPP_

#include <assert.h>
#include <atomic>
#include <bits/stdc++.h>

#include <common/ssmem_wrapper.hpp>
#include <persist/persist_offset.hpp>

extern __thread unsigned long * seeds;
extern __thread void* nodes[1024];
extern int levelmax;

#define FRASER_MAX_MAX_LEVEL 64 /* covers up to 2^64 elements */


namespace SkiplistDurableManualNode {
    template<typename T, template<typename, bool> typename PERSIST>
    struct alignas(ALIGNMENT) Node {
        persist<int, flush_option::no_flush> key;
        persist <T, flush_option::no_flush> value;
        persist<int, flush_option::no_flush> toplevel;
        PERSIST<std::atomic<Node*>, flush_option::no_flush> next[FRASER_MAX_MAX_LEVEL];

        Node(int k, T v, int topl) : key(k), val(v), toplevel(topl) {
            for (int i = 0; i < levelmax; i++) {
                next[i].store_non_atomic(nullptr);
            }
            FLUSH_STRUCT(this);
            assert((((uintptr_t) this) % (ALIGNMENT)) == 0); // check alignment
        }

        Node(int k, T v, Node* n, int topl) : key(k), val(v), toplevel(topl) {
            for (int i = 0; i < levelmax; i++) {
                next[i].store_non_atomic(n);
            }
            FLUSH_STRUCT(this);
            assert((((uintptr_t) this) % (ALIGNMENT)) == 0); // check alignment
        }
    };

    template<typename T>
    struct alignas(ALIGNMENT) Node<T, persist_offset_spec> {
        persist<int, flush_option::no_flush> key;
        persist <T, flush_option::no_flush> value;
        persist<int, flush_option::no_flush> toplevel;
        persist_offset<std::atomic<Node *>, sizeof(Node *), flush_option::no_flush> next[FRASER_MAX_MAX_LEVEL];
        flush_counter_t counter;

        Node(int k, T v, int topl) : key(k), val(v), toplevel(topl), counter(0) {
            for (int i = 0; i < levelmax; i++) {
                next[i].store_non_atomic(nullptr);
            }
            FLUSH_STRUCT(this);
            assert((((uintptr_t) this) % (ALIGNMENT)) == 0); // check alignment
        }

        Node(int k, T v, Node* n, int topl) : key(k), val(v), toplevel(topl), counter(0) {
            for (int i = 0; i < levelmax; i++) {
                next[i].store_non_atomic(n);
            }
            FLUSH_STRUCT(this);
            assert((((uintptr_t) this) % (ALIGNMENT)) == 0); // check alignment
        }

    };
}

template <typename T, template<typename, bool> typename PERSIST>
class alignas(ALIGNMENT) SkiplistListDurableManual {

private:
    using Node = SkiplistListDurableManualNode::Node<T, PERSIST>;

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



    SkiplistDurableManual() {
        Node *min, *max;
        new(max) Node(INT_MAX, 0, nullptr, levelmax);
        BARRIER(max);
        new(min) Node(INT_MIN, 0, max, levelmax);
        BARRIER(min);
        this->head = min;
        BARRIER(&this->head);
        assert((((uintptr_t) this) % (ALIGNMENT)) == 0); // check alignment
    }

    ~SkiplistDurableManual() {
        Node *node, *next;
        node = this->head;
        while (node != nullptr) {
            next = getNext(node, 0);
            //ssfree(node);
            node = next;
        }
    }

    T get(int key) {
        OperationLifetime op;
        Node *succs[FRASER_MAX_MAX_LEVEL], *preds[FRASER_MAX_MAX_LEVEL];
        bool exists = search_no_cleanup(key, preds, succs);
        preds[0]->next[0].flush_if_needed();
        succs[0]->next[0].flush_if_needed();
        //fence is missing - is there any interface for that?
        if (exists) {
            return succs[0]->val;
        } else {
            return 0;
        }
    }

    bool contains(int key) {
        return get(key) != 0;
    }

    bool remove(int key) {
        OperationLifetime op;

        Node *succs[FRASER_MAX_MAX_LEVEL], *preds[FRASER_MAX_MAX_LEVEL];
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


    bool insert(int key, T val, int id) {
        OperationLifetime op;

        Node *newNode, *pred, *succ;
        Node *succs[FRASER_MAX_MAX_LEVEL], *preds[FRASER_MAX_MAX_LEVEL];
        int i;
        bool found;

        retry:
        found = search(key, preds, succs);
        preds[0]->next[0].flush_if_needed();
        if (found) {
            return false;
        }

        newNode = static_cast<Node *>(ssmem.alloc(sizeof(Node)));
        new(newNode) Node(k, val, nullptr, get_rand_level(id));

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





private:

    bool search(int key, Node **left_list, Node **right_list) {
        Node *left_parent, *left, *left_next, *right = nullptr, *right_next;
        retry:
        left_parent = this->head;
        left = this->head;
        int num_nodes = 0;
        for (int i = levelmax - 1; i >= 0; i--) {
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
                    nodes[j].next[0].flush_if_needed();
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
    }

    bool search_no_cleanup(int key, Node **left_list, Node **right_list) {
        Node *left, *left_next, *right = nullptr;
        left = this->head;
        for (int i = levelmax - 1; i >= 0; i--) {
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
        return (right->key == key && !getMark(getNext(right, i)));
    }


    inline bool mark_node_ptrs(Node *n) {
        bool cas = false;
        Node *n_next;
        for (int i = n->toplevel - 1; i <= 0; i--) {
            do {
                n_next = getNext(n, i);
                if (getMark(n_next) && i == 0) {
                    n->next[0].flush_if_needed();
                    cas = false;
                    break;
                }
                Node *before = getAdd(n_next);
                Node *after = mark(n_next);
                cas = CAS_next_unflushed(n, before, after, i); //no need to persist because search already does
            } while (!cas);
        }
        return (cas); /* if I was the one that marked lvl 0 */
    }

    int get_rand_level(int seed) {
        int level = 1;
        for (int i = 0; i < levelmax - 1; i++) {
            if ((rand_r_32((unsigned int *) &seed) % 101) < 50)
                level++;
            else
                break;
        }
        return level;
    }

};




#endif