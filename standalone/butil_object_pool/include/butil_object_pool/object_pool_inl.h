// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#ifndef BUTIL_OBJECT_POOL_STANDALONE_OBJECT_POOL_INL_H
#define BUTIL_OBJECT_POOL_STANDALONE_OBJECT_POOL_INL_H

#include <algorithm>  // std::min
#include <atomic>
#include <cstddef>  // size_t, offsetof
#include <cstdlib>  // std::free, std::malloc
#include <cstring>  // std::memcpy
#include <memory>   // std::unique_ptr
#include <mutex>
#include <new>      // placement new
#include <ostream>
#include <type_traits>
#include <utility>  // std::forward
#include <vector>

namespace butil_object_pool {

namespace detail {

constexpr std::size_t kCacheLineSize = 64;

#if defined(__GNUC__) || defined(__clang__)
inline bool Likely(bool value) { return __builtin_expect(value, 1); }
inline bool Unlikely(bool value) { return __builtin_expect(value, 0); }
#else
inline bool Likely(bool value) { return value; }
inline bool Unlikely(bool value) { return value; }
#endif

template <typename U>
U* MallocConstruct() noexcept {
    void* const mem = std::malloc(sizeof(U));
    if (mem == nullptr) {
        return nullptr;
    }
    return ::new (mem) U();
}

template <typename U, typename... Args>
U* MallocConstruct(Args&&... args) noexcept {
    void* const mem = std::malloc(sizeof(U));
    if (mem == nullptr) {
        return nullptr;
    }
    return ::new (mem) U(std::forward<Args>(args)...);
}

template <typename U>
void DestructFree(U* ptr) noexcept {
    if (ptr == nullptr) {
        return;
    }
    ptr->~U();
    std::free(ptr);
}

}  // namespace detail

template <typename T, std::size_t NItem>
struct ObjectPoolFreeChunk {
    std::size_t nfree;
    T* ptrs[NItem];
};

// Variable-length free chunk used for the global list.
// `ptrs[1]` is a standard C++ workaround for a flexible array member.
template <typename T>
struct ObjectPoolDynamicFreeChunk {
    std::size_t nfree;
    T* ptrs[1];
};

struct ObjectPoolInfo {
    std::size_t local_pool_num;
    std::size_t block_group_num;
    std::size_t block_num;
    std::size_t item_num;
    std::size_t block_item_num;
    std::size_t free_chunk_item_num;
    std::size_t total_size;
#ifdef BUTIL_OBJECT_POOL_STANDALONE_NEED_FREE_ITEM_NUM
    std::size_t free_item_num;
#endif
};

inline std::ostream& operator<<(std::ostream& os, const ObjectPoolInfo& info) {
    os << "local_pool_num: " << info.local_pool_num
       << "\nblock_group_num: " << info.block_group_num
       << "\nblock_num: " << info.block_num
       << "\nitem_num: " << info.item_num
       << "\nblock_item_num: " << info.block_item_num
       << "\nfree_chunk_item_num: " << info.free_chunk_item_num
       << "\ntotal_size: " << info.total_size;
#ifdef BUTIL_OBJECT_POOL_STANDALONE_NEED_FREE_ITEM_NUM
    os << "\nfree_num: " << info.free_item_num;
#endif
    return os;
}

static constexpr std::size_t kMaxBlockGroupCount = 65536;
static constexpr std::size_t kGroupBlockCount = (1ULL << 16);

template <typename T>
class ObjectPoolBlockItemNum {
private:
    static const std::size_t kN1 = ObjectPoolBlockMaxSize<T>::value / sizeof(T);
    static const std::size_t kN2 = (kN1 < 1 ? 1 : kN1);

public:
    static const std::size_t value =
        (kN2 > ObjectPoolBlockMaxItem<T>::value ? ObjectPoolBlockMaxItem<T>::value : kN2);
};

template <typename T>
class alignas(detail::kCacheLineSize) ObjectPool {
public:
    static const std::size_t BLOCK_NITEM = ObjectPoolBlockItemNum<T>::value;
    static const std::size_t FREE_CHUNK_NITEM = BLOCK_NITEM;

    using FreeChunk = ObjectPoolFreeChunk<T, FREE_CHUNK_NITEM>;
    using DynamicFreeChunk = ObjectPoolDynamicFreeChunk<T>;

    struct alignas(detail::kCacheLineSize) Block {
        alignas(T) unsigned char items[sizeof(T) * BLOCK_NITEM];
        std::size_t nitem;

        Block() : nitem(0) {}
    };

    struct BlockGroup {
        std::atomic<std::size_t> nblock;
        std::atomic<Block*> blocks[kGroupBlockCount];

        BlockGroup() : nblock(0) {
            for (std::size_t i = 0; i < kGroupBlockCount; ++i) {
                blocks[i].store(nullptr, std::memory_order_relaxed);
            }
        }
    };

    class alignas(detail::kCacheLineSize) LocalPool {
    public:
        explicit LocalPool(ObjectPool* pool)
            : m_pool(pool), m_curBlock(nullptr), m_curBlockIndex(0) {
            m_curFree.nfree = 0;
        }

        ~LocalPool() {
            if (m_curFree.nfree != 0) {
                m_pool->PushFreeChunk(m_curFree);
            }
            m_pool->OnLocalPoolDestroyed();
        }

        T* Get() { return GetImpl(); }

        template <typename A1>
        T* Get(const A1& a1) {
            return GetImpl(a1);
        }

        template <typename A1, typename A2>
        T* Get(const A1& a1, const A2& a2) {
            return GetImpl(a1, a2);
        }

        int ReturnObject(T* ptr) {
            if (m_curFree.nfree < ObjectPool::FreeChunkItemCount()) {
                m_curFree.ptrs[m_curFree.nfree++] = ptr;
#ifdef BUTIL_OBJECT_POOL_STANDALONE_NEED_FREE_ITEM_NUM
                m_pool->m_globalNfree.fetch_add(1, std::memory_order_relaxed);
#endif
                return 0;
            }
            if (m_pool->PushFreeChunk(m_curFree)) {
                m_curFree.nfree = 1;
                m_curFree.ptrs[0] = ptr;
#ifdef BUTIL_OBJECT_POOL_STANDALONE_NEED_FREE_ITEM_NUM
                m_pool->m_globalNfree.fetch_add(1, std::memory_order_relaxed);
#endif
                return 0;
            }
            return -1;
        }

    private:
        template <typename... Args>
        T* GetImpl(Args&&... args) {
            if (m_curFree.nfree != 0) {
#ifdef BUTIL_OBJECT_POOL_STANDALONE_NEED_FREE_ITEM_NUM
                m_pool->m_globalNfree.fetch_sub(1, std::memory_order_relaxed);
#endif
                return m_curFree.ptrs[--m_curFree.nfree];
            }
            if (m_pool->PopFreeChunk(m_curFree)) {
#ifdef BUTIL_OBJECT_POOL_STANDALONE_NEED_FREE_ITEM_NUM
                m_pool->m_globalNfree.fetch_sub(1, std::memory_order_relaxed);
#endif
                return m_curFree.ptrs[--m_curFree.nfree];
            }

            if (m_curBlock != nullptr && m_curBlock->nitem < BLOCK_NITEM) {
                T* obj = ConstructInBlock(std::forward<Args>(args)...);
                if (obj != nullptr) {
                    ++m_curBlock->nitem;
                }
                return obj;
            }

            m_curBlock = m_pool->AddBlock(&m_curBlockIndex);
            if (m_curBlock != nullptr) {
                T* obj = ConstructInBlock(std::forward<Args>(args)...);
                if (obj != nullptr) {
                    ++m_curBlock->nitem;
                }
                return obj;
            }
            return nullptr;
        }

        template <typename... Args>
        T* ConstructInBlock(Args&&... args) {
            T* const objs = reinterpret_cast<T*>(m_curBlock->items);
            void* const place = static_cast<void*>(objs + m_curBlock->nitem);
            T* obj = nullptr;
            if constexpr (sizeof...(Args) == 0) {
                obj = ::new (place) T;
            } else {
                obj = ::new (place) T(std::forward<Args>(args)...);
            }
            if (!ObjectPoolValidator<T>::validate(obj)) {
                obj->~T();
                return nullptr;
            }
            return obj;
        }

        ObjectPool* m_pool;
        Block* m_curBlock;
        std::size_t m_curBlockIndex;
        FreeChunk m_curFree;
    };

    struct LocalPoolDeleter {
        void operator()(LocalPool* ptr) const noexcept { detail::DestructFree(ptr); }
    };

    static ObjectPool* singleton() {
        static ObjectPool* const instance = []() noexcept -> ObjectPool* {
            void* const mem = std::malloc(sizeof(ObjectPool));
            if (mem == nullptr) {
                return nullptr;
            }
            return ::new (mem) ObjectPool();
        }();
        return instance;
    }

    T* get_object() {
        LocalPool* const lp = GetOrNewLocalPool();
        if (detail::Likely(lp != nullptr)) {
            return lp->Get();
        }
        return nullptr;
    }

    template <typename A1>
    T* get_object(const A1& arg1) {
        LocalPool* const lp = GetOrNewLocalPool();
        if (detail::Likely(lp != nullptr)) {
            return lp->Get(arg1);
        }
        return nullptr;
    }

    template <typename A1, typename A2>
    T* get_object(const A1& arg1, const A2& arg2) {
        LocalPool* const lp = GetOrNewLocalPool();
        if (detail::Likely(lp != nullptr)) {
            return lp->Get(arg1, arg2);
        }
        return nullptr;
    }

    int return_object(T* ptr) {
        LocalPool* const lp = GetOrNewLocalPool();
        if (detail::Likely(lp != nullptr)) {
            return lp->ReturnObject(ptr);
        }
        return -1;
    }

    void clear_objects() { m_tlsLocalPool.reset(); }

    static std::size_t FreeChunkItemCount() {
        const std::size_t n = ObjectPoolFreeChunkMaxItem<T>::value();
        return (n < FREE_CHUNK_NITEM ? n : FREE_CHUNK_NITEM);
    }

    [[nodiscard]] ObjectPoolInfo describe_objects() const {
        ObjectPoolInfo info{};
        info.local_pool_num = static_cast<std::size_t>(m_nlocal.load(std::memory_order_relaxed));
        info.block_group_num = m_ngroup.load(std::memory_order_acquire);
        info.block_num = 0;
        info.item_num = 0;
        info.free_chunk_item_num = FreeChunkItemCount();
        info.block_item_num = BLOCK_NITEM;
#ifdef BUTIL_OBJECT_POOL_STANDALONE_NEED_FREE_ITEM_NUM
        info.free_item_num = m_globalNfree.load(std::memory_order_relaxed);
#endif

        for (std::size_t i = 0; i < info.block_group_num; ++i) {
            BlockGroup* const bg = m_blockGroups[i].load(std::memory_order_acquire);
            if (bg == nullptr) {
                break;
            }
            const std::size_t nblock = std::min(bg->nblock.load(std::memory_order_relaxed), kGroupBlockCount);
            info.block_num += nblock;
            for (std::size_t j = 0; j < nblock; ++j) {
                Block* const b = bg->blocks[j].load(std::memory_order_acquire);
                if (b != nullptr) {
                    info.item_num += b->nitem;
                }
            }
        }

        info.total_size = info.block_num * info.block_item_num * sizeof(T);
        return info;
    }

private:
    ObjectPool() : m_ngroup(0), m_nlocal(0) {
        for (std::size_t i = 0; i < kMaxBlockGroupCount; ++i) {
            m_blockGroups[i].store(nullptr, std::memory_order_relaxed);
        }
    }

    ~ObjectPool() = default;

    Block* AddBlock(std::size_t* index) {
        Block* const new_block = detail::MallocConstruct<Block>();
        if (new_block == nullptr) {
            return nullptr;
        }

        std::size_t ngroup = 0;
        do {
            ngroup = m_ngroup.load(std::memory_order_acquire);
            if (ngroup >= 1) {
                BlockGroup* const g = m_blockGroups[ngroup - 1].load(std::memory_order_acquire);
                if (g == nullptr) {
                    continue;
                }
                const std::size_t block_index = g->nblock.fetch_add(1, std::memory_order_relaxed);
                if (block_index < kGroupBlockCount) {
                    g->blocks[block_index].store(new_block, std::memory_order_release);
                    *index = (ngroup - 1) * kGroupBlockCount + block_index;
                    return new_block;
                }
                g->nblock.fetch_sub(1, std::memory_order_relaxed);
            }
        } while (AddBlockGroup(ngroup));

        detail::DestructFree(new_block);
        return nullptr;
    }

    bool AddBlockGroup(std::size_t old_ngroup) {
        std::lock_guard<std::mutex> lock(m_blockGroupMutex);
        const std::size_t ngroup = m_ngroup.load(std::memory_order_acquire);
        if (ngroup != old_ngroup) {
            return true;
        }
        if (ngroup >= kMaxBlockGroupCount) {
            return false;
        }

        BlockGroup* const bg = detail::MallocConstruct<BlockGroup>();
        if (bg == nullptr) {
            return false;
        }

        m_blockGroups[ngroup].store(bg, std::memory_order_release);
        m_ngroup.store(ngroup + 1, std::memory_order_release);
        return true;
    }

    LocalPool* GetOrNewLocalPool() {
        LocalPool* const existing = m_tlsLocalPool.get();
        if (detail::Likely(existing != nullptr)) {
            return existing;
        }

        std::unique_ptr<LocalPool, LocalPoolDeleter> lp(detail::MallocConstruct<LocalPool>(this));
        if (!lp) {
            return nullptr;
        }

        std::lock_guard<std::mutex> lock(m_changeThreadMutex);
        m_tlsLocalPool = std::move(lp);
        m_nlocal.fetch_add(1, std::memory_order_relaxed);
        return m_tlsLocalPool.get();
    }

    void OnLocalPoolDestroyed() {
        const long previous = m_nlocal.fetch_sub(1, std::memory_order_relaxed);
        if (previous != 1) {
            return;
        }

#ifdef BUTIL_OBJECT_POOL_STANDALONE_CLEAR_AFTER_ALL_THREADS_QUIT
        std::lock_guard<std::mutex> lock(m_changeThreadMutex);
        if (m_nlocal.load(std::memory_order_relaxed) != 0) {
            return;
        }

        FreeChunk dummy{};
        while (PopFreeChunk(dummy)) {
        }

        const std::size_t ngroup = m_ngroup.exchange(0, std::memory_order_relaxed);
        for (std::size_t i = 0; i < ngroup; ++i) {
            BlockGroup* const bg = m_blockGroups[i].load(std::memory_order_relaxed);
            if (bg == nullptr) {
                break;
            }
            const std::size_t nblock = std::min(bg->nblock.load(std::memory_order_relaxed), kGroupBlockCount);
            for (std::size_t j = 0; j < nblock; ++j) {
                Block* const b = bg->blocks[j].load(std::memory_order_relaxed);
                if (b == nullptr) {
                    continue;
                }
                for (std::size_t k = 0; k < b->nitem; ++k) {
                    T* const objs = reinterpret_cast<T*>(b->items);
                    objs[k].~T();
                }
                detail::DestructFree(b);
            }
            detail::DestructFree(bg);
        }

        for (std::size_t i = 0; i < kMaxBlockGroupCount; ++i) {
            m_blockGroups[i].store(nullptr, std::memory_order_relaxed);
        }
#endif
    }

    bool PopFreeChunk(FreeChunk& c) {
        if (m_freeChunks.empty()) {
            return false;
        }
        std::lock_guard<std::mutex> lock(m_freeChunksMutex);
        if (m_freeChunks.empty()) {
            return false;
        }
        DynamicFreeChunk* const p = m_freeChunks.back();
        m_freeChunks.pop_back();
        c.nfree = p->nfree;
        std::memcpy(c.ptrs, p->ptrs, sizeof(*p->ptrs) * p->nfree);
        std::free(p);
        return true;
    }

    bool PushFreeChunk(const FreeChunk& c) {
        if (c.nfree == 0) {
            return true;
        }
        DynamicFreeChunk* const p = static_cast<DynamicFreeChunk*>(std::malloc(
            offsetof(DynamicFreeChunk, ptrs) + sizeof(*c.ptrs) * c.nfree));
        if (p == nullptr) {
            return false;
        }
        p->nfree = c.nfree;
        std::memcpy(p->ptrs, c.ptrs, sizeof(*c.ptrs) * c.nfree);
        std::lock_guard<std::mutex> lock(m_freeChunksMutex);
        try {
            m_freeChunks.push_back(p);
        } catch (const std::bad_alloc&) {
            std::free(p);
            return false;
        }
        return true;
    }

    std::atomic<std::size_t> m_ngroup;
    std::atomic<long> m_nlocal;
    std::atomic<BlockGroup*> m_blockGroups[kMaxBlockGroupCount];

    std::mutex m_blockGroupMutex;
    std::mutex m_changeThreadMutex;

    std::vector<DynamicFreeChunk*> m_freeChunks;
    std::mutex m_freeChunksMutex;

    inline static thread_local std::unique_ptr<LocalPool, LocalPoolDeleter> m_tlsLocalPool = nullptr;

#ifdef BUTIL_OBJECT_POOL_STANDALONE_NEED_FREE_ITEM_NUM
    std::atomic<std::size_t> m_globalNfree{0};
#endif
};

template <typename T>
const std::size_t ObjectPool<T>::BLOCK_NITEM;

template <typename T>
const std::size_t ObjectPool<T>::FREE_CHUNK_NITEM;

}  // namespace butil_object_pool

#endif  // BUTIL_OBJECT_POOL_STANDALONE_OBJECT_POOL_INL_H
