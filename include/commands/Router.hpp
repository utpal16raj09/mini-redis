#ifndef MINI_REDIS_ROUTER_HPP
#define MINI_REDIS_ROUTER_HPP

#include "protocol/RespParser.hpp"
#include "storage/Database.hpp"
#include <string>

using namespace std;

namespace miniredis {

class AofEngine; // Forward declaration

// The Router class parses incoming Redis request arrays, matches command names
// (e.g. SET, GET, HSET, LPUSH), executes database operations, and logs write commands to AOF.
class Router {
public:
    Router(Database& db, AofEngine* aof = nullptr);

    // Main command dispatcher function.
    // Executes command against database and appends to AOF log if log_to_aof is true.
    RespValue dispatch(const RespValue& request, bool log_to_aof = true);

private:
    Database& db_;  // Database engine handle
    AofEngine* aof_; // AOF persistence engine handle

    // Returns true if a command mutates database state (e.g. SET, DEL, HSET)
    bool isWriteCommand(const string& cmd);
};

} // namespace miniredis

#endif // MINI_REDIS_ROUTER_HPP
