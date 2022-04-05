#ifndef LIST_ORIGINAL_HPP_
#define LIST_ORIGINAL_HPP_

#include <assert.h>
#include <atomic>
#include <bits/stdc++.h> 
#include <common/ssmem_wrapper.hpp>

template <class T> 
class alignas(ALIGNMENT) ListOriginal{
private:
    class alignas(ALIGNMENT) Node{
    public:
        int key;
        T value;
        std::atomic<Node*> next;

        Node(int k, T val, Node* n) : key(k), value(val), next(n) {
            // std::cout << "Node alignment: " << (((uintptr_t) this) & -((uintptr_t) this)) << ", expected alignment " << ALIGNMENT << std::endl;
            assert((((uintptr_t) this) % (ALIGNMENT)) == 0); // check alignment
        }
        Node* getNext() {
            return next.load(std::memory_order_acquire);
        }
        bool CAS_next(Node* exp, Node* n) {
            return next.compare_exchange_strong(exp, n);
        }
    };

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

    Node* head;

//===========================================

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

    ListOriginal(int) : ListOriginal() {}

    ListOriginal() {
        head = static_cast<Node*>(ssmem.alloc(sizeof(Node), false));
        new (head) Node(INT_MIN, INT_MIN, NULL);
        assert((((uintptr_t) this) % (ALIGNMENT)) == 0); // check alignment
    }

    ~ListOriginal() {
        while(head != NULL) {
            Node* next = getAdd(head->getNext());
            //ssmem.free(head); // figure out why this doesn't work with hashtable
            head = next;
        }
    }
    
//===========================================

        Window seek(Node* head, int key) {
            Node* left = head;
            Node* leftNext = head->getNext();
            Node* right = NULL;
            
            Node* curr = NULL;
            Node* currAdd = NULL;
            Node* succ = NULL;
            bool marked = false;
            while (true) {
                curr = head;
                currAdd = curr;
                succ = currAdd->getNext();
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
                    succ = currAdd->getNext();
                    marked = getMark(succ);
                }
                right = currAdd;
                /* 2: Check nodes are adjacent */
                if (leftNext == right) {
                   if ((right != NULL) && getMark(right->getNext())) {
                    continue;
                } else {
                    return Window(left, right);
                }
            }
                /* 3: Remove one or more marked nodes */
            if (left->CAS_next(leftNext, right)) {
                Node* removedNode = getAdd(leftNext);
                while(removedNode != right) {
                    ssmem.free(removedNode, false);
                    removedNode = getAdd(removedNode->getNext());
                }
                if ((right != NULL) && getMark(right->getNext())) {
                    continue;
                } else {
                    return Window(left, right);
                }
            }
        }
    }

//=========================================

    bool add(int k, T item) {
        //bool add(T item, int k, int threadID) {
        while (true) {
            Window window = seek(head, k);
            Node* pred = window.pred;
            Node* curr = window.curr;
            if (curr && curr->key == k) {
                return false;
            }
            Node* node = static_cast<Node*>(ssmem.alloc(sizeof(Node), false));
            new (node) Node(k, item, curr);
            bool res = pred->CAS_next(curr, node);
            if (res) {
                return true;
            } else {
                ssmem.free(node, false);
                continue;
            }
        }
    }

//========================================

    bool remove(int key) {
        bool snip = false;
        while (true) {
            Window window = seek(head, key);
            Node* pred = window.pred;
            Node* curr = window.curr;
            if (!curr || curr->key != key) {
                return false;
            } else {
                Node* succ = curr->getNext();
                Node* succAndMark = mark(succ);
                if (succ == succAndMark) {
                    continue;
                }
                snip = curr->CAS_next(succ, succAndMark);
                if (!snip)
                    continue;
                if(pred->CAS_next(curr, succ))           //succ is not marked
                    ssmem.free(curr, false);
                return true;
            }
        }
    }

//========================================

        bool contains(int k) {
            int key = k;
            Node* curr = head;
            bool marked = getMark(curr->getNext());
            while (curr->key < key) {
                curr = getAdd(curr->getNext());
                if (!curr) {
                    return false;
                }
                marked = getMark(curr->getNext());
            }
            if(curr->key == key && !marked){
                return true;
            } else {
                return false;
            }
        }

        static std::string get_name() {
            return "List Original";
        }

        /* Functions for debugging and validation.
           Must be run in a quiescent state.
         */

        long long size() {
            long long s = 0;
            Node* n = getAdd(head->getNext());
            while(n != nullptr) {
                bool marked = getMark(n->getNext());
                if(!marked) s++;
                n = getAdd(n->getNext());
            }
            return s;
        }

        long long keySum() {
            long long s = 0;
            Node* n = getAdd(head->getNext());
            while(n != nullptr) {
                bool marked = getMark(n->getNext());
                if(!marked) s+=n->key;
                n = getAdd(n->getNext());
            }
            return s;
        }
};

#endif /* LIST_ORIGINAL_HPP_ */

