#ifndef MINI_REDIS_VALUE_HPP
#define MINI_REDIS_VALUE_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <variant>
#include <chrono>
#include <optional>

using namespace std;

namespace miniredis {

// Supported Redis value types
enum class ValueType {
    String,
    Hash,
    List,
    Set
};

// Variant holding actual data based on ValueType
using DataVariant = variant<string, unordered_map<string, string>, deque<string>, unordered_set<string>>;

// Value structure represents a stored Redis object with data type and TTL support.
struct Value {
    ValueType type{ValueType::String};
    DataVariant data{string("")};
    
    // Absolute time point when key expires (if set)
    optional<chrono::steady_clock::time_point> expire_at;

    // Checks if the key has expired
    bool isExpired() const {
        if (!expire_at.has_value()) return false;
        return chrono::steady_clock::now() >= expire_at.value();
    }

    // Remaining TTL in seconds (-1 if no TTL, -2 if expired)
    int64_t getTtlSeconds() const {
        if (!expire_at.has_value()) return -1;
        auto now = chrono::steady_clock::now();
        if (now >= expire_at.value()) return -2;
        return chrono::duration_cast<chrono::seconds>(expire_at.value() - now).count();
    }
};

} // namespace miniredis

#endif // MINI_REDIS_VALUE_HPP
