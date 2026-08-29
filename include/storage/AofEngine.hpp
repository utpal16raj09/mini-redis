#ifndef MINI_REDIS_AOF_ENGINE_HPP
#define MINI_REDIS_AOF_ENGINE_HPP

#include "protocol/RespParser.hpp"
#include <string>
#include <fstream>

using namespace std;

namespace miniredis {

class Router; // Forward declaration

// AofEngine handles writing write commands to disk and replaying AOF logs on startup.
class AofEngine {
public:
    explicit AofEngine(const string& filename = "appendonly.aof");
    ~AofEngine();

    // Appends a RESP serialized write command to the AOF file.
    void append(const RespValue& command);

    // Replays all commands stored in the AOF file to restore database state.
    void loadAndReplay(Router& router);

private:
    string filename_;
    ofstream file_stream_;
};

} // namespace miniredis

#endif // MINI_REDIS_AOF_ENGINE_HPP
