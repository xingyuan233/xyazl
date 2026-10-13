#pragma once

#include "./type.h"
#include <string>

namespace xyu
{
    uint16 msb(const uint8* ctrl) noexcept;

    uint16 cmpeq(const uint8* ctrl, uint8 val) noexcept;
}

namespace xyu
{
    template <typename Key, typename Val>
    struct HashNode { Key key; Val val; };

    template <typename Key, typename Val>
    class HashTable
    {
        enum Tag : uint8 { EMPTY = 0x80, USED = 0x00, };
        struct ControlGroup { uint8 metas[16]; };
        using Node = HashNode<Key, Val>;

    private:
        uint8* data;        // 内存块：前半部分为 ControlGroup，后半部分为 Node 数组
        uint n;             // 元素数量
        uint capa;          // 扩容阈值
        uint groups;        // 控制块数量

    public:
        explicit HashTable() : data{}, n(0), capa(0), groups(0) {}

        HashTable(const HashTable&) = delete;
        HashTable& operator=(const HashTable&) = delete;

        HashTable(HashTable&& other) noexcept : data(other.data), n(other.n), capa(other.capa), groups(other.groups)
        {
            other.data = nullptr;
            other.n = other.capa = other.groups = 0;
        }

        HashTable& operator=(HashTable&& other) noexcept
        {
            if (this != &other)
            {
                this->~HashTable();

                data   = other.data;
                n      = other.n;
                capa   = other.capa;
                groups = other.groups;

                other.data   = nullptr;
                other.n = other.capa = other.groups = 0;
            }
            return *this;
        }

        ~HashTable() { release(); }

        uint count() const noexcept { return n; }

        Val& get(const Key& key)
        {
            reserve(n + 1);
            size_t hash = std::hash<Key>{}(key);
            uint ci = (hash >> 7) & (groups - 1);

            for (;;)
            {
                uint16 mask = cmpeq(get_ctrl(ci).metas, hash & 0x7f);
                while (mask)
                {
                    uint offset = __builtin_ctz(mask);
                    Node& kv = get_kv((ci << 4) + offset);
                    if (kv.key == key) return kv.val;
                    mask &= mask - 1;
                }

                mask = msb(get_ctrl(ci).metas);
                if (mask)
                {
                    size_t target_idx = (ci << 4) + __builtin_ctz(mask);
                    Node& kv = get_kv(target_idx);
                    ::new (&kv) Node{key, Val{}};
                    get_ctrl(target_idx >> 4).metas[target_idx & 15] = hash & 0x7f;
                    ++n;
                    return kv.val;
                }

                ci = (ci + 1) & (groups - 1);
            }
        }

        Val* find(const Key& key)
        {
            if (!data) return nullptr;
            size_t hash = std::hash<Key>{}(key);
            uint h2 = hash & 0x7f;
            uint ci = (hash >> 7) & (groups - 1);
            for (;;)
            {
                ControlGroup& ctrl = get_ctrl(ci);
                uint16 match = cmpeq(ctrl.metas, h2);
                while (match)
                {
                    Node& kv = get_kv((ci << 4) + __builtin_ctz(match));
                    if (kv.key == key) return &kv.val;
                    match &= match - 1;
                }
                uint16 empty = msb(ctrl.metas);
                if (empty) return nullptr;
                ci = (ci + 1) & (groups - 1);
            }
        }

        void reserve(uint mincapa)
        {
            if (mincapa <= capa) return;
            uint new_groups = groups == 0 ? 1 : groups;
            while ((new_groups * 16) * 0.8 < mincapa) new_groups <<= 1;

            uint old_groups = groups;
            uint8* old_data = data;
            groups = new_groups;
            capa = static_cast<uint>(groups * 16 * 0.8);

            uint ctrl_size = groups << 4;
            data = new uint8[ctrl_size + groups * 16 * sizeof(Node)];
            memset(data, EMPTY, groups * 16);

            if (old_data)
            {
                n = 0;
                for (size_t ci = 0; ci < old_groups; ci++)
                {
                    ControlGroup& ctrl = ((ControlGroup*)(old_data))[ci];
                    uint16 mask = ~msb(ctrl.metas);
                    while (mask)
                    {
                        uint offset = __builtin_ctz(mask);
                        size_t old_ctrl_size = old_groups << 4;
                        Node& old_kv = *reinterpret_cast<Node*>(old_data + old_ctrl_size + ((ci << 4) + offset) * sizeof(Node));

                        get(old_kv.key) = std::move(old_kv.val);
                        old_kv.~HashNode();
                        mask &= mask - 1;
                    }
                }
                delete[] old_data;
            }
        }

        void clear() noexcept
        {
            if (!data) return;
            for (uint ci = 0; ci < groups; ++ci)
            {
                uint16 mask = ~msb(get_ctrl(ci).metas);
                while (mask)
                {
                    get_kv((ci << 4) + __builtin_ctz(mask)).~Node();
                    mask &= mask - 1;
                }
            }
            memset(data, EMPTY, groups * 16);
            n = 0;
        }

        void release() noexcept
        {
            if (!data) return;
            for (uint ci = 0; ci < groups; ++ci)
            {
                uint16 mask = ~msb(get_ctrl(ci).metas);
                while (mask)
                {
                    get_kv((ci << 4) + __builtin_ctz(mask)).~Node();
                    mask &= mask - 1;
                }
            }
            delete[] data;
            data = nullptr;
            n = capa = groups = 0;
        }

    private:
        class iterator
        {
            HashTable* table;
            uint cindex;        // 当前控制组索引，end 时为 table->groups
            uint16 mask;        // 当前控制组中尚未访问的有效槽位掩码

        public:
            // 私有构造：从某个有效位置开始 (由 begin 调用)
            iterator(HashTable* t, uint ci, uint16 m) noexcept : table(t), cindex(ci), mask(m) {}

            // 前进到下一个有效元素，更新 cindex/mask
            void advance()
            {
                // 清除当前最低有效位
                mask &= mask - 1;
                // 若当前组已无有效元素，则移到下一个有有效元素的组
                while (mask == 0 && cindex < table->groups)
                {
                    ++cindex;
                    if (cindex >= table->groups) break;
                    mask = ~msb(table->get_ctrl(cindex).metas);
                }
            }

            Node& operator*() const
            {
                uint offset = __builtin_ctz(mask);
                return table->get_kv((cindex << 4) + offset);
            }

            iterator& operator++() { advance(); return *this; }
            iterator operator++(int) { iterator tmp = *this; advance(); return tmp; }

            bool operator==(const iterator& other) const
            {
                return table == other.table && cindex == other.cindex && mask == other.mask;
            }
            bool operator!=(const iterator& other) const { return !(*this == other); }
        };

    public:
        // 返回第一个有效元素的迭代器，若表空则返回 end()
        iterator begin()
        {
            for (uint ci = 0; ci < groups; ++ci)
            {
                uint16 m = ~msb(get_ctrl(ci).metas);
                if (m) return iterator(this, ci, m);
            }
            return end();
        }

        // 哨兵迭代器：cindex == groups, mask == 0
        iterator end() { return iterator(this, groups, 0); }

    private:
        // 获取控制块
        ControlGroup& get_ctrl(size_t cindex)
        {
            return reinterpret_cast<ControlGroup*>(data)[cindex];
        }

        // 获取键值对
        Node& get_kv(size_t index)
        {
            size_t ctrl_size = groups << 4;
            return *reinterpret_cast<Node*>(data + ctrl_size + index * sizeof(Node));
        }
    };
}
