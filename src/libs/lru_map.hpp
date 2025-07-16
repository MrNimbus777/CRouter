#pragma once
#include <functional>
#include <stdexcept>
#include <mutex>

template <typename K, typename V>
class lru_map {
   private:
    struct Node {
        K key;
        V val;
        Node* next = nullptr;
        Node* prev = nullptr;
    };
    Node* head;
    Node* tail;
    std::function<long(V)> func;
    std::mutex mtx;

    void move_to_head(Node* n) {
        if (n == head) {
            return;
        }
        
        if (n->prev) {
            n->prev->next = n->next;
        }

        if (n->next) {
            n->next->prev = n->prev;
        } else {
            tail = n->prev;
        }

        n->next = head;
        n->prev = nullptr;
        if (head) {
            head->prev = n;
        }
        head = n;

        if (!tail) {
            tail = n;
        }
    }
    void remove_tail() {
        if (!tail) return;
        Node* node_to_delete = tail;
        if (head == tail) {
            head = nullptr;
            tail = nullptr;
        } else {
            tail = tail->prev;
            tail->next = nullptr;
        }
        delete node_to_delete;
    }
    long byte_size_mtx() {
        long size = 0;
        Node* current = head;
        while (current) {
            size += func(current->val);
            current = current->next;
        }
        return size;
    }

   public:
    long max_bytes;
    lru_map(std::function<long(V)> one_element_size) : head(nullptr), tail(nullptr), func(std::move(one_element_size)), max_bytes(16777216) {}
    lru_map(std::function<long(V)> one_element_size, long max_bytes) : head(nullptr), tail(nullptr), func(std::move(one_element_size)), max_bytes(max_bytes) {}
    void put(const K& key, const V& val) {
        std::lock_guard<std::mutex> lock(mtx);
        if (func(val) > max_bytes) throw std::runtime_error("lru_map::put(const K& key, const V& val): val bigger than max_bytes");
        if (!head) {
            head = new Node;
            tail = head;
            head->key = key;
            head->val = val;
        } else {
            Node* find = head;
            while (find) {
                if (find->key == key) break;
                find = find->next;
            }
            if (find) {
                while (byte_size_mtx() - func(find->val) + func(val) > max_bytes) remove_tail();
                find->val = val;
            } else {
                while (byte_size_mtx() + func(val) > max_bytes) remove_tail();
                find = new Node;
                find->key = key;
                find->val = val;
                find->next = head;
                head->prev = find;
                head = find;
            }
        }
    }
    bool exists(const K& key) {
        std::lock_guard<std::mutex> lock(mtx);
        Node* find = head;
        while (find) {
            if (find->key == key) break;
            find = find->next;
        }
        return find != nullptr;
    }
    void remove(const K& key) {
        std::lock_guard<std::mutex> lock(mtx);
        Node* find = head;
        while (find) {
            if (find->key == key) break;
            find = find->next;
        }
        if (find) {
            if (find->prev)
                find->prev->next = find->next;
            else
                head = find->next;
            if (find->next)
                find->next->prev = find->prev;
            else
                tail = find->prev;
            delete find;
        }
    }
    const V& get(const K& key) {
        std::lock_guard<std::mutex> lock(mtx);
        Node* find = head;
        while (find) {
            if (find->key == key) break;
            find = find->next;
        }
        if (find) {
            move_to_head(find);
            return find->val;
        }
        throw std::runtime_error("lru_map::get(const K& key): key not found");
    }
    int size() {
        std::lock_guard<std::mutex> lock(mtx);
        int counter = 0;
        Node* current = head;
        while (current) {
            counter++;
            current = current->next;
        }
        return counter;
    }
    long byte_size() {
        std::lock_guard<std::mutex> lock(mtx);
        return byte_size_mtx();
    }
    ~lru_map() {
        std::lock_guard<std::mutex> lock(mtx);
        Node* current = head;
        while (current) {
            Node* next = current->next;
            delete current;
            current = next;
        }
    }
    lru_map() = delete;
    lru_map(const lru_map&) = delete;
    lru_map& operator=(const lru_map&) = delete;
};