// src/tools/tool_registry.cpp — hand-written hash table (spec §4.3).
//
// Separate chaining with a power-of-two bucket array. Each bucket is a
// std::vector<std::unique_ptr<Tool>>, so a rehash moves only the pointers;
// the Tool objects never move, and every Tool* from find() stays valid.

#include "tools/tool_registry.h"

#include <cstdint>
#include <string>
#include <utility>

ToolRegistry::ToolRegistry() : buckets_(kInitialBuckets) {}

// The unique_ptrs in the buckets destroy every tool exactly once.
ToolRegistry::~ToolRegistry() = default;

// 64-bit FNV-1a: uses every byte, order-sensitive, full-width result.
std::size_t ToolRegistry::hash(std::string_view key) noexcept {
    std::uint64_t h = 1469598103934665603ULL;   // offset basis
    for (unsigned char c : key) {
        h ^= c;
        h *= 1099511628211ULL;                  // FNV prime
    }
    return static_cast<std::size_t>(h);
}

std::size_t ToolRegistry::index_for(std::string_view key) const noexcept {
    return hash(key) & (buckets_.size() - 1);
}

void ToolRegistry::rehash(std::size_t new_bucket_count) {
    std::vector<Bucket> fresh(new_bucket_count);
    for (Bucket& old : buckets_) {
        for (std::unique_ptr<Tool>& t : old) {
            std::size_t i = hash(t->name()) & (new_bucket_count - 1);
            fresh[i].push_back(std::move(t));   // moves the pointer, not the Tool
        }
    }
    buckets_ = std::move(fresh);
}

bool ToolRegistry::add(std::unique_ptr<Tool> tool) {
    if (tool == nullptr) return false;

    const std::string key = tool->name();
    if (find(key) != nullptr) return false;     // `tool` is destroyed on return

    // Check the load factor BEFORE inserting, so it never exceeds the threshold.
    if (static_cast<double>(size_ + 1) >
        kMaxLoadFactor * static_cast<double>(buckets_.size())) {
        rehash(buckets_.size() * 2);
    }

    buckets_[index_for(key)].push_back(std::move(tool));
    ++size_;
    return true;
}

Tool* ToolRegistry::find(std::string_view name) const {
    const Bucket& b = buckets_[index_for(name)];
    for (const std::unique_ptr<Tool>& t : b) {
        if (t->name() == name) return t.get();
    }
    return nullptr;
}

std::size_t ToolRegistry::size() const noexcept {
    return size_;
}

void ToolRegistry::for_each(const std::function<void(const Tool&)>& fn) const {
    for (const Bucket& b : buckets_) {
        for (const std::unique_ptr<Tool>& t : b) fn(*t);
    }
}

std::size_t ToolRegistry::bucket_count() const noexcept {
    return buckets_.size();
}

double ToolRegistry::load_factor() const noexcept {
    return static_cast<double>(size_) / static_cast<double>(buckets_.size());
}

std::size_t ToolRegistry::max_probe_length() const {
    std::size_t longest = 0;
    for (const Bucket& b : buckets_) {
        if (b.size() > longest) longest = b.size();
    }
    return longest;
}
