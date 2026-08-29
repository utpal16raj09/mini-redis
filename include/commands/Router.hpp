#ifndef MINI_REDIS_ROUTER_HPP
#define MINI_REDIS_ROUTER_HPP

#include "protocol/RespParser.hpp"
#include "storage/Database.hpp"
#include <string>

using namespace std;

namespace miniredis {

class AofEngine; // Forward declaration

class Router {
public:
    explicit Router(Database& db, AofEngine* aof = nullptr);

    // Set or update AOF engine pointer
    void setAofEngine(AofEngine* aof) { aof_ = aof; }

    // Dispatch command and optionally write to AOF if command mutates state
    RespValue dispatch(const RespValue& request, bool log_to_aof = true);

private:
    Database& db_;
    AofEngine* aof_;

    bool isWriteCommand(const string& cmd);
};

} // namespace miniredis

#endif // MINI_REDIS_ROUTER_HPP
