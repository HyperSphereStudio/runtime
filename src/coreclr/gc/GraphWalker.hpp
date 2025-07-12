#include "ObjectPool.hpp"
#include <vector>

template<typename T>
struct FastGraphStack {
    std::vector<T> stack;

    void reset() {
        stack.clear();
    }
};

template<typename T>
class FastGraphWalker {
    public:

    FastGraphWalker() : isEmpty(true), sk(nullptr), idx(0){}

    T* popNode() {
        if (sk == nullptr || idx == 0) {
            _ASSERTE(!isEmpty);
            isEmpty = true;
            return &parent;
        }
        return &sk->stack[--idx];  //Pop without losing the address...
    }
  
    T* currentNode() {
        if(sk == nullptr || idx == 0)
            return isEmpty ? nullptr : &parent;
        return &sk->stack[idx - 1];
    }

    void parentPushed() {
        isEmpty = false;
    }

    //Assume that parent is manually set...
    void pushNode(const T& n) {
        if (sk == nullptr) {
            if(isEmpty){
                parent = n;
                parentPushed();
            }else{
                sk = g_graphWalkingPool.allocate();
            }
            idx = 0;
        }

        if(sk){
            idx++;
            sk->stack.emplace_back(n);
        }
    }

    T parent;

    static ObjectPool<FastGraphStack<T>> g_graphWalkingPool;

    private:
    bool isEmpty;
    FastGraphStack<T>* sk;
    int idx;
};
