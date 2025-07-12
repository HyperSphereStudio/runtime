#include <cstdint>
#include <cstddef>
#include <mutex>
#include <new>
#include <cassert>

template <typename T>
class ObjectPool {
public:
    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;

    ObjectPool(int minPoolSize = 0, int maxPoolSize = 0)
        : freelist(nullptr),
          allocatedCount(0),
          minPoolSize(minPoolSize),
          maxPoolSize(maxPoolSize)
    {
        for (int i = 0; i < minPoolSize; ++i) {
            Node* node = allocateNode();
            deallocate(castToObject(node));  // Put into freelist
        }
    }

    ~ObjectPool() {
        std::lock_guard<std::mutex> lock(mtx);
        Node* node = freelist;
        while (node) {
            Node* next = node->next;
            destroyNode(node);
            node = next;
        }
    }

    T* allocate() {
        std::lock_guard<std::mutex> lock(mtx);

        if (freelist) {
            Node* node = freelist;
            freelist = node->next;
            return castToObject(node);
        }

        if (allocatedCount < maxPoolSize) {
            Node* newNode = allocateNode();
            ++allocatedCount;
            T* obj = castToObject(newNode);
            obj->reset();
            return obj;
        }

        return nullptr;
    }

    void deallocate(T* obj) {
        std::lock_guard<std::mutex> lock(mtx);
        obj->reset();

        Node* node = castToNode(obj);
        node->next = freelist;
        freelist = node;
    }

private:
    struct Node {
        alignas(T) uint8_t storage[sizeof(T)];
        Node* next = nullptr;
    };

    Node* freelist;
    int allocatedCount;
    const int minPoolSize;
    const int maxPoolSize;

    std::mutex mtx;

    Node* allocateNode() {
        void* mem = ::operator new(sizeof(Node));
        Node* node = static_cast<Node*>(mem);
        new (castToObject(node)) T();
        return node;
    }

    void destroyNode(Node* node) {
        T* obj = castToObject(node);
        obj->~T();
        ::operator delete(node);
    }

    T* castToObject(Node* node) {
        return reinterpret_cast<T*>(node);
    }

    Node* castToNode(T* obj) {
        return reinterpret_cast<Node*>(obj);
    }
};
