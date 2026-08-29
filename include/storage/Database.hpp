#ifndef MINI_REDIS_DATABASE_HPP
#define MINI_REDIS_DATABASE_HPP

#include "storage/Value.hpp"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <optional>

using namespace std;

namespace miniredis {

// Database class managing in-memory state and data structures
class Database {
public:
    // String operations
    void set(const string& key, const string& value, optional<int64_t> ttl_ms = nullopt);
    optional<string> get(const string& key);
    optional<int64_t> incr(const string& key, int64_t increment = 1);

    // Generic key & TTL operations
    bool del(const string& key);
    bool exists(const string& key);
    bool expire(const string& key, int64_t ttl_ms);
    int64_t ttl(const string& key);
    vector<string> keys(const string& pattern = "*");
    void flush();

    // Active background expiration sampling (deletes up to sample_limit expired keys)
    size_t purgeExpiredKeys(size_t sample_limit = 20);

    // Hash operations
    bool hset(const string& key, const string& field, const string& value);
    optional<string> hget(const string& key, const string& field);
    bool hdel(const string& key, const string& field);
    optional<unordered_map<string, string>> hgetall(const string& key);

    // List operations
    size_t lpush(const string& key, const vector<string>& values);
    size_t rpush(const string& key, const vector<string>& values);
    optional<string> lpop(const string& key);
    optional<string> rpop(const string& key);
    optional<vector<string>> lrange(const string& key, int start, int stop);

    // Set operations
    size_t sadd(const string& key, const vector<string>& members);
    size_t srem(const string& key, const vector<string>& members);
    optional<unordered_set<string>> smembers(const string& key);

private:
    unordered_map<string, Value> store_;
};

} // namespace miniredis

#endif // MINI_REDIS_DATABASE_HPP
