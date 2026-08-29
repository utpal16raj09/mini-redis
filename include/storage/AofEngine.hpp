#ifndef MINI_REDIS_AOF_ENGINE_HPP
#define MINI_REDIS_AOF_ENGINE_HPP

#include "protocol/RespParser.hpp"
#include <string>
#include <fstream>

using namespace std;

namespace miniredis {

class Router; // Forward declaration

// The AofEngine (Append-Only File Engine) provides disk persistence.
// It logs all database-mutating write commands to disk (appendonly.aof)
// and replays them on server boot to restore full memory state.
class AofEngine {
public:
    // Create an AOF persistence engine targeting a specific filename (default: appendonly.aof).
    explicit AofEngine(const string& filepath = "appendonly.aof");
    ~AofEngine();

    // Appends a write command to the appendonly.aof file and flushes it to disk.
    void append(const RespValue& command);

    // Reads the appendonly.aof file line-by-line upon startup and replays all commands.
    void loadAndReplay(Router& router);

private:
    string filepath_; // Path to the AOF log file on disk
    ofstream file_;   // Output stream for appending commands
};

} // namespace miniredis

#endif // MINI_REDIS_AOF_ENGINE_HPP
