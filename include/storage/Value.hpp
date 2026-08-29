#ifndef MINI_REDIS_VALUE_HPP
#define MINI_REDIS_VALUE_HPP

#include <string>
#include <unordered_map>
#include <deque>
#include <unordered_set>
#include <variant>
#include <chrono>
#include <optional>

using namespace std;

namespace miniredis {

// Supported data structure types in MiniRedis
enum class ValueType {
    String,
    Hash,
    List,
    Set
};

// C++ variant holding the actual underlying data structure for a key
using DataVariant = variant<
    string,                         // ValueType::String
    unordered_map<string, string>,  // ValueType::Hash
    deque<string>,                  // ValueType::List
    unordered_set<string>           // ValueType::Set
>;

// The Value struct represents any value stored in the database map.
// It holds the data type, the data variant, and an optional expiration timestamp (TTL).
struct Value {
    ValueType type{ValueType::String};
    DataVariant data{string("")};
    optional<chrono::steady_clock::time_point> expire_at; // Absolute expiration deadline

    // Checks if this key has expired based on current time
    bool isExpired() const {
        if (!expire_at.has_value()) return false;
        return chrono::steady_clock::now() >= expire_at.value();
    }

    // Calculates remaining TTL in seconds (-1 if key has no expiration set)
    int64_t getTtlSeconds() const {
        if (!expire_at.has_value()) return -1;
        auto now = chrono::steady_clock::now();
        if (now >= expire_at.value()) return -2; // Expired
        auto diff = chrono::duration_cast<chrono::seconds>(expire_at.value() - now);
        return diff.count();
    }
};

} // namespace miniredis

#endif // MINI_REDIS_VALUE_HPP
