#include "storage/Database.hpp"
#include <random>
#include <algorithm>

using namespace std;

namespace miniredis {

// --- String Operations ---

void Database::set(const string& key, const string& value, optional<int64_t> ttl_ms) {
    Value val;
    val.type = ValueType::String;
    val.data = value;
    if (ttl_ms.has_value()) {
        val.expire_at = chrono::steady_clock::now() + chrono::milliseconds(ttl_ms.value());
    }
    store_[key] = val;
}

optional<string> Database::get(const string& key) {
    auto it = store_.find(key);
    if (it == store_.end()) return nullopt;

    if (it->second.isExpired()) {
        store_.erase(it);
        return nullopt;
    }

    if (it->second.type != ValueType::String) return nullopt;
    return std::get<string>(it->second.data);
}

optional<int64_t> Database::incr(const string& key, int64_t increment) {
    auto it = store_.find(key);
    int64_t current_val = 0;

    if (it != store_.end()) {
        if (it->second.isExpired()) {
            store_.erase(it);
        } else if (it->second.type != ValueType::String) {
            return nullopt; // WRONGTYPE Operation against a key holding the wrong kind of value
        } else {
            try {
                current_val = stoll(std::get<string>(it->second.data));
            } catch (...) {
                return nullopt; // Value is not an integer
            }
        }
    }

    current_val += increment;
    set(key, to_string(current_val));
    return current_val;
}

// --- Key Management & Expiration ---

bool Database::del(const string& key) {
    return store_.erase(key) > 0;
}

bool Database::exists(const string& key) {
    auto it = store_.find(key);
    if (it == store_.end()) return false;
    if (it->second.isExpired()) {
        store_.erase(it);
        return false;
    }
    return true;
}

bool Database::expire(const string& key, int64_t ttl_ms) {
    auto it = store_.find(key);
    if (it == store_.end() || it->second.isExpired()) {
        if (it != store_.end()) store_.erase(it);
        return false;
    }
    it->second.expire_at = chrono::steady_clock::now() + chrono::milliseconds(ttl_ms);
    return true;
}

int64_t Database::ttl(const string& key) {
    auto it = store_.find(key);
    if (it == store_.end()) return -2; // Key does not exist
    if (it->second.isExpired()) {
        store_.erase(it);
        return -2;
    }
    return it->second.getTtlSeconds();
}

vector<string> Database::keys(const string& pattern) {
    vector<string> result;
    for (auto it = store_.begin(); it != store_.end(); ) {
        if (it->second.isExpired()) {
            it = store_.erase(it);
        } else {
            if (pattern == "*" || pattern == it->first) {
                result.push_back(it->first);
            }
            ++it;
        }
    }
    return result;
}

void Database::flush() {
    store_.clear();
}

size_t Database::purgeExpiredKeys(size_t sample_limit) {
    size_t evicted = 0;
    if (store_.empty()) return 0;

    vector<string> sample_keys;
    sample_keys.reserve(min(store_.size(), sample_limit));

    for (const auto& pair : store_) {
        sample_keys.push_back(pair.first);
        if (sample_keys.size() >= sample_limit) break;
    }

    for (const auto& k : sample_keys) {
        auto it = store_.find(k);
        if (it != store_.end() && it->second.isExpired()) {
            store_.erase(it);
            evicted++;
        }
    }

    return evicted;
}

// --- Hash Operations ---

bool Database::hset(const string& key, const string& field, const string& value) {
    auto it = store_.find(key);
    if (it != store_.end() && it->second.isExpired()) {
        store_.erase(it);
        it = store_.end();
    }

    if (it == store_.end()) {
        Value val;
        val.type = ValueType::Hash;
        unordered_map<string, string> map;
        map[field] = value;
        val.data = map;
        store_[key] = val;
        return true;
    }

    if (it->second.type != ValueType::Hash) return false;

    auto& map = std::get<unordered_map<string, string>>(it->second.data);
    bool is_new = (map.find(field) == map.end());
    map[field] = value;
    return is_new;
}

optional<string> Database::hget(const string& key, const string& field) {
    auto it = store_.find(key);
    if (it == store_.end() || it->second.isExpired()) return nullopt;
    if (it->second.type != ValueType::Hash) return nullopt;

    const auto& map = std::get<unordered_map<string, string>>(it->second.data);
    auto fit = map.find(field);
    if (fit == map.end()) return nullopt;
    return fit->second;
}

bool Database::hdel(const string& key, const string& field) {
    auto it = store_.find(key);
    if (it == store_.end() || it->second.isExpired()) return false;
    if (it->second.type != ValueType::Hash) return false;

    auto& map = std::get<unordered_map<string, string>>(it->second.data);
    return map.erase(field) > 0;
}

optional<unordered_map<string, string>> Database::hgetall(const string& key) {
    auto it = store_.find(key);
    if (it == store_.end() || it->second.isExpired()) return nullopt;
    if (it->second.type != ValueType::Hash) return nullopt;

    return std::get<unordered_map<string, string>>(it->second.data);
}

// --- List Operations ---

size_t Database::lpush(const string& key, const vector<string>& values) {
    auto it = store_.find(key);
    if (it != store_.end() && it->second.isExpired()) {
        store_.erase(it);
        it = store_.end();
    }

    if (it == store_.end()) {
        Value val;
        val.type = ValueType::List;
        val.data = deque<string>();
        store_[key] = val;
        it = store_.find(key);
    }

    if (it->second.type != ValueType::List) return 0;
    auto& list = std::get<deque<string>>(it->second.data);
    for (const auto& v : values) {
        list.push_front(v);
    }
    return list.size();
}

size_t Database::rpush(const string& key, const vector<string>& values) {
    auto it = store_.find(key);
    if (it != store_.end() && it->second.isExpired()) {
        store_.erase(it);
        it = store_.end();
    }

    if (it == store_.end()) {
        Value val;
        val.type = ValueType::List;
        val.data = deque<string>();
        store_[key] = val;
        it = store_.find(key);
    }

    if (it->second.type != ValueType::List) return 0;
    auto& list = std::get<deque<string>>(it->second.data);
    for (const auto& v : values) {
        list.push_back(v);
    }
    return list.size();
}

optional<string> Database::lpop(const string& key) {
    auto it = store_.find(key);
    if (it == store_.end() || it->second.isExpired()) return nullopt;
    if (it->second.type != ValueType::List) return nullopt;

    auto& list = std::get<deque<string>>(it->second.data);
    if (list.empty()) return nullopt;

    string val = list.front();
    list.pop_front();
    return val;
}

optional<string> Database::rpop(const string& key) {
    auto it = store_.find(key);
    if (it == store_.end() || it->second.isExpired()) return nullopt;
    if (it->second.type != ValueType::List) return nullopt;

    auto& list = std::get<deque<string>>(it->second.data);
    if (list.empty()) return nullopt;

    string val = list.back();
    list.pop_back();
    return val;
}

optional<vector<string>> Database::lrange(const string& key, int start, int stop) {
    auto it = store_.find(key);
    if (it == store_.end() || it->second.isExpired()) return nullopt;
    if (it->second.type != ValueType::List) return nullopt;

    const auto& list = std::get<deque<string>>(it->second.data);
    int size = static_cast<int>(list.size());
    if (size == 0) return vector<string>{};

    if (start < 0) start = max(0, size + start);
    if (stop < 0) stop = size + stop;

    if (start >= size || start > stop) return vector<string>{};
    stop = min(stop, size - 1);

    vector<string> res;
    for (int i = start; i <= stop; ++i) {
        res.push_back(list[i]);
    }
    return res;
}

// --- Set Operations ---

size_t Database::sadd(const string& key, const vector<string>& members) {
    auto it = store_.find(key);
    if (it != store_.end() && it->second.isExpired()) {
        store_.erase(it);
        it = store_.end();
    }

    if (it == store_.end()) {
        Value val;
        val.type = ValueType::Set;
        val.data = unordered_set<string>();
        store_[key] = val;
        it = store_.find(key);
    }

    if (it->second.type != ValueType::Set) return 0;

    auto& set_data = std::get<unordered_set<string>>(it->second.data);
    size_t added = 0;
    for (const auto& m : members) {
        if (set_data.insert(m).second) added++;
    }
    return added;
}

size_t Database::srem(const string& key, const vector<string>& members) {
    auto it = store_.find(key);
    if (it == store_.end() || it->second.isExpired()) return 0;
    if (it->second.type != ValueType::Set) return 0;

    auto& set_data = std::get<unordered_set<string>>(it->second.data);
    size_t removed = 0;
    for (const auto& m : members) {
        if (set_data.erase(m) > 0) removed++;
    }
    return removed;
}

optional<unordered_set<string>> Database::smembers(const string& key) {
    auto it = store_.find(key);
    if (it == store_.end() || it->second.isExpired()) return nullopt;
    if (it->second.type != ValueType::Set) return nullopt;

    return std::get<unordered_set<string>>(it->second.data);
}

} // namespace miniredis
