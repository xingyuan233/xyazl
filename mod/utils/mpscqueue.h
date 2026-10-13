#pragma once

#include <atomic>

namespace xyu
{
    template <typename T>
    struct MPSCQueue
    {
        struct Node { std::atomic<Node*> next; T data; };
    private:
        alignas(64) std::atomic<Node*> tail;    // 队尾指针
        char _pad[64 - sizeof(tail)];
        Node* lead;                 // 哨兵节点
        Node* del = nullptr;        // 延迟删除节点

    public:
        // 构造
        MPSCQueue() noexcept : tail{}, lead{}
        {
            lead = new Node{ nullptr };
            tail.store(lead, std::memory_order_relaxed);
        }
        // 析构
        ~MPSCQueue() noexcept
        {
            if (del) delete del;
            for (Node* cur = lead; cur;)
            {
                Node* next = cur->next.load(std::memory_order_relaxed);
                delete cur;
                cur = next;
            }
        }

        void push(T val) noexcept
        {
            auto node = new Node{ nullptr, std::move(val) };
            Node* tn = tail.exchange(node, std::memory_order_acq_rel);
            tn->next.store(node, std::memory_order_release);
        }

        T pop(bool* empty) noexcept
        {
            Node* node = lead->next.load(std::memory_order_acquire);
            if (!node)
            {
                *empty = true;
                return T{};
            }
            if (del) delete del;
            del = lead;
            lead = node;
            *empty = false;
            return std::move(node->data);
        }

        bool empty() const noexcept
        {
            return lead->next.load(std::memory_order_acquire) == nullptr;
        }
    };
}
