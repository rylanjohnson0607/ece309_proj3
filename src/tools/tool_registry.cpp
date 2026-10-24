// src/tools/tool_registry.cpp — YOU IMPLEMENT (spec §4.3).
//
// A hand-written hash table. No std::unordered_map / std::map / std::set.
// Separate chaining with owning pointers and growth at load factor 1.0.

#include "tools/tool_registry.h"
#include <cstdint>
#include <string>
#include <utility>

ToolRegistry::ToolRegistry() : buckets_(16) {}  // Start with 16 empty buckets.
ToolRegistry::~ToolRegistry() = default;   // unique_ptrs in your buckets free the tools

std::size_t ToolRegistry::hash(std::string_view key) noexcept {
    // Use the starting value and multiply/XOR algorithm from Appendix A.
    std::uint64_t h = 1469598103934665603ULL;
    for (unsigned char c : key) {
        h ^= c;
        h *= 1099511628211ULL;
    }
    // Bucket selection happens separately, using hash(name) % bucket_count().
    return static_cast<std::size_t>(h);
}

bool ToolRegistry::add(std::unique_ptr<Tool> tool) {
    if (!tool) {
        return false;
    }

    const std::string name = tool->name();
    if (find(name) != nullptr) {
        return false;  // The rejected new tool is destroyed automatically.
    }

    // Grow before insertion so the load factor never exceeds 1.0.
    if (size() >= bucket_count()) { //Faster than calling load_factor
        rehash(bucket_count() * 2);
    }
    const std::size_t index = hash(name) % bucket_count();
    buckets_[index].push_back(std::move(tool));
    ++tool_count_;
    return true;
}

void ToolRegistry::rehash(std::size_t new_bucket_count) {
    std::vector<Bucket> new_buckets(new_bucket_count);
    for (auto& bucket : buckets_) {
        for (auto& tool : bucket) {
            const std::size_t index = hash(tool->name()) % new_bucket_count;
            new_buckets[index].push_back(std::move(tool));
        }
    }
    // Swap in the new table. The old buckets now contain only empty pointers.
    buckets_.swap(new_buckets);
}

Tool* ToolRegistry::find(std::string_view name) const {
    const std::size_t index = hash(name) % bucket_count();
    // Different names can share a bucket, so compare each stored name.
    for (const auto& tool : buckets_[index]) {
        if (tool->name() == name) {
            return tool.get();  // Borrow the pointer; the registry keeps ownership.
        }
    }
    return nullptr;
}

std::size_t ToolRegistry::size() const noexcept {
    return tool_count_;
}

void ToolRegistry::for_each(const std::function<void(const Tool&)>& fn) const {
    for (const auto& bucket : buckets_) {
        for (const auto& tool : bucket) {
            fn(*tool);
        }
    }
}

std::size_t ToolRegistry::bucket_count() const noexcept {
    return buckets_.size();
}

double ToolRegistry::load_factor() const noexcept {
    return static_cast<double>(size()) / bucket_count();
}

std::size_t ToolRegistry::max_probe_length() const {
    std::size_t longest = 0;
    for (const auto& bucket : buckets_) {
        if (bucket.size() > longest) {
            longest = bucket.size();
        }
    }
    return longest;
}
