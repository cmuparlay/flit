#ifndef LIST_DURABLE_AUTOMATIC_HPP_
#define LIST_DURABLE_AUTOMATIC_HPP_

#include <assert.h>
#include <atomic>
#include <bits/stdc++.h> 

#include <common/ssmem_wrapper.hpp>
#include <persist/persist_offset.hpp>
#include <persist/persist_interface.hpp>

namespace ListDurableAutomaticNode {
    template <typename T, template<typename, bool> typename PERSIST> 
    struct alignas(ALIGNMENT) Node {
        persist<int, flush_option::flush> key;
        persist<T, flush_option::flush> value;
        PERSIST<std::atomic<Node*>, flush_option::flush> next;

        Node(int k, T val, Node* n) : key(k), value(val), next(n) {
            assert((((uintptr_t) this) % (ALIGNMENT)) == 0); // check alignment
        }
    };

    template <typename T> 
    struct alignas(ALIGNMENT) Node<T, persist_interface> {
        persist<int, flush_option::no_flush> key;
        persist<T, flush_option::no_flush> value;
        persist_interface<std::atomic<Node*>, flush_option::flush> next;

        Node(int k, T val, Node* n) : key(k), value(val), next(n) {
            assert((((uintptr_t) this) % (ALIGNMENT)) == 0); // check alignment
        }
    };

    template <typename T> 
    struct alignas(ALIGNMENT) Node<T, persist_offset_spec> {
        persist<int, flush_option::flush> key;
        persist<T, flush_option::flush> value;
        persist_offset<std::atomic<Node*>, sizeof(Node*), flush_option::flush> next;    
        flush_counter_t counter;

        Node(int k, T val, Node* n) : key(k), value(val), next(n), counter(0) {
            assert((((uintptr_t) this) % (ALIGNMENT)) == 0); // check alignment
        }
    };
}

template <typename T, template<typename, bool> typename PERSIST>
class alignas(ALIGNMENT) ListDurableAutomatic {

private:
    using Node = ListDurableAutomaticNode::Node<T, PERSIST>;

    persist<Node*> head;

    Node* getAdd(Node* n) {
        long node = (long)n;
            return (Node*)(node & ~(0x1L)); // clear bit to get the real address
        }

        bool getMark(Node* n) {
            long node = (long)n;
            return (bool)(node & 0x1L);
        }

        Node* mark(Node* n) {
            long node = (long)n;
            node |= 0x1L;
            return (Node*)node;
        }

public:
    Node* getNext(Node* n) { 
        return n->next.load(std::memory_order_acquire); 
    }

    bool CAS_next(Node* node, Node* exp, Node* n) {
        return node->next.compare_exchange_strong(exp, n);
    }

//===========================================

    class Window {
    public:
        Node* pred;
        Node* curr;
        Window(Node* myPred, Node* myCurr) {
            pred = myPred;
            curr = myCurr;
        }
    };

//===========================================
    ListDurableAutomatic(int) : ListDurableAutomatic() {}

    ListDurableAutomatic() {
        head = static_cast<Node*>(ssmem.alloc(sizeof(Node)));
        new (head) Node(INT_MIN, INT_MIN, NULL);
        assert((((uintptr_t) this) % (ALIGNMENT)) == 0); // check alignment
    }

    ~ListDurableAutomatic() {
        while(head != NULL) {
            Node* next = getAdd(getNext(head));
            // ssmem.free(head);
            head = next;
        }
    }

//===========================================

        Window seek(Node* head, int key) {
            Node* left = head;
            Node* leftNext = getNext(head);
            Node* right = NULL;
            
            Node* curr = NULL;
            Node* currAdd = NULL;
            Node* succ = NULL;
            bool marked = false;
            while (true) {
                curr = head;
                currAdd = curr;
                succ = getNext(currAdd);
                marked = getMark(succ);
                /* 1: Find left and right */
                while (marked || currAdd->key < key) {
                    if (!marked) {
                        left = currAdd;
                        leftNext = succ;
                    }
                    curr = succ;
                    currAdd = getAdd(curr);        // load_acq
                    if (currAdd == NULL) {
                        break;
                    }
                    succ = getNext(currAdd);
                    marked = getMark(succ);
                }
                right = currAdd;
                /* 2: Check nodes are adjacent */
                if (leftNext == right) {
                 if ((right != NULL) && getMark(getNext(right))) {
                    continue;
                } else {
                    return Window(left, right);
                }
            }
                /* 3: Remove one or more marked nodes */
            if (CAS_next(left, leftNext, right)) {
                Node* removedNode = getAdd(leftNext);
                while(removedNode != right) {
                    ssmem.free(removedNode);
                    removedNode = getAdd(getNext(removedNode));
                }
                if ((right != NULL) && getMark(getNext(right))) {
                    continue;
                } else {
                    return Window(left, right);
                }
            }
        }
    }

//=========================================

    bool add(int k, T item) {
        if constexpr (!std::is_same<PERSIST<T,false>, persist_interface<T,false>>::value) 
            OperationLifetime op;
        //bool add(T item, int k, int threadID) {
        while (true) {
            Window window = seek(head, k);
            Node* pred = window.pred;
            Node* curr = window.curr;
            if (curr && curr->key == k)
                return false;
            Node* node = static_cast<Node*>(ssmem.alloc(sizeof(Node)));
            new (node) Node(k, item, curr);
            bool res = CAS_next(pred, curr, node);
            if (res) return true;
            else {
                ssmem.free(node);
                continue;
            }
        }
    }

//========================================

    bool remove(int key) {
        if constexpr (!std::is_same<PERSIST<T,false>, persist_interface<T,false>>::value) 
            OperationLifetime op;
        bool snip = false;
        while (true) {
            Window window = seek(head, key);
            Node* pred = window.pred;
            Node* curr = window.curr;
            if (!curr || curr->key != key) {
                return false;
            } else {
                Node* succ = getNext(curr);
                Node* succAndMark = mark(succ);
                if (succ == succAndMark) {
                    continue;
                }
                snip = CAS_next(curr, succ, succAndMark);
                if (!snip)
                    continue;
                if(CAS_next(pred, curr, succ))           //succ is not marked
                    ssmem.free(curr);
                return true;
            }
        }
    }

//========================================

        bool contains(int k) {
            if constexpr (!std::is_same<PERSIST<T,false>, persist_interface<T,false>>::value) 
                OperationLifetime op;
            int key = k;
            Node* curr = head;
            bool marked = getMark(getNext(curr));
            while (curr->key < key) {
                curr = getAdd(getNext(curr));
                if (!curr) {
                    return false;
                }
                marked = getMark(getNext(curr));
            }
            if(curr->key == key && !marked){
                return true;
            } else {
                return false;
            }
        }

        static std::string get_name() {
            return "List Auto, " + PERSIST<std::atomic<int>, flush_option::flush>::get_name();
        }

        /* Functions for debugging and validation.
           Must be run in a quiescent state.
         */

        long long size() {
            long long s = 0;
            Node* n = getAdd(getNext(head));
            while(n != nullptr) {
                bool marked = getMark(getNext(n));
                if(!marked) s++;
                n = getAdd(getNext(n));
            }
            return s;
        }
 
        long long keySum() {
            long long s = 0;
            Node* n = getAdd(getNext(head));
            while(n != nullptr) {
                bool marked = getMark(getNext(n));
                if(!marked) s+=n->key;
                n = getAdd(getNext(n));
            }
            return s;
        }  
};

#endif /* LIST_DURABLE_AUTOMATIC_HPP_ */