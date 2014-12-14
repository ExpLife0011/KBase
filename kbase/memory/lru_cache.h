/*
 @ Kingsley Chen
*/

#if defined(_MSC_VER)
#pragma once
#endif

#ifndef KBASE_MEMORY_LRU_CACHE_H_
#define KBASE_MEMORY_LRU_CACHE_H_

#include <map>
#include <unordered_map>

namespace kbase {

// Standardize type argument signatures of std::map and std::unordered_map.

template<typename Key, typename Value>
struct TreeMap {
    using MapType = std::map<Key, Value>;
};

template<typename Key, typename Value>
struct HashMap {
    using MapType = std::unordered_map<Key, Value>;
};

template<typename Key, typename Entry, template<typename, typename> class Map>
class LRUCache {
public:
    using key_type = Key;
    using value_type = std::pair<const Key, Entry>;

private:
    using CachedEntryList = std::list<value_type>;
    using KeyTable = typename Map<Key, typename CachedEntryList::iterator>::MapType;

public:
    using size_type = size_t;
    using iterator = typename CachedEntryList::iterator;
    using const_iterator = typename CachedEntryList::const_iterator;
    using reverse_iterator = typename CachedEntryList::reverse_iterator;
    using const_reverse_iterator = typename CachedEntryList::const_reverse_iterator;

    enum : size_type {
        NO_AUTO_EVICT = 0
    };

    explicit LRUCache(size_type max_size)
        : max_size_(max_size)
    {}

    LRUCache(const LRUCache&) = delete;

    // TODO: LRUCache(LRUCache&&);

    LRUCache& operator=(const LRUCache&) = delete;

    // TODO: LRUCache& operator=(LRUCache&&);

    ~LRUCache() = default;

    iterator Put(const Key& key, const Entry& entry)
    {
        PutInternal(key, entry);    
    }

    iterator Put(const Key& key, Entry&& entry)
    {
        PutInternal(key, std::move(entry));
    }

    iterator erase(const_iterator pos)
    {
        key_table_.erase(pos->first);
        return entry_ordering_list_.erase(pos);
    }

    void Evict()
    {
        erase(entry_ordering_list_.begin());
    }

    size_type size() const
    {
        return entry_ordering_list_.size();
    }

    bool empty() const
    {
        return entry_ordering_list_.empty();
    }

    size_type max_size() const
    {
        return max_size_;
    }

    bool auto_evict() const
    {
        return max_size_ != 0;
    }

    iterator begin() { return entry_ordering_list_.begin(); }
    
    const_iterator begin() const { return entry_ordering_list_.begin(); }
    
    const_iterator cbegin() const { return entry_ordering_list_.cbegin(); }

    iterator end() { return entry_ordering_list_.end(); }

    const_iterator end() const { return entry_ordering_list_.end(); }

    const_iterator cend() const { return entry_ordering_list_.cend(); }

private:
    iterator PutInternal(const Key& key, Entry&& entry)
    {
        if ((auto key_it = key_table_.find(key)) != key_table_.end()) {
            erase(key_it->second);
        } else if (auto_evict() && (max_size() == size())) {
            Evict();
        }

        entry_ordering_list_.push_back(value_type(key, std::forward<Entry>(entry)));
        auto rv = key_table_.insert(
            std::make_pair(key, --entry_ordering_list_.end()));
        
        return rv.first;
    }

private:
    const size_type max_size_;
    CachedEntryList entry_ordering_list_;
    KeyTable key_table_;
};

template<typename Key, typename Entry>
using LRUTreeCache = LRUCache<Key, Entry, TreeMap>;

template<typename Key, typename Entry>
using LRUHashCache = LRUCache<Key, Entry, HashMap>;

}   // namespace kbase

#endif  // KBASE_MEMORY_LRU_CACHE_H_