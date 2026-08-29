#include "storage/AofEngine.hpp"
#include "commands/Router.hpp"
#include <iostream>

using namespace std;

namespace miniredis {

// Constructor opens the AOF log file in append mode.
AofEngine::AofEngine(const string& filepath) : filepath_(filepath) {
    file_.open(filepath_, ios::out | ios::app | ios::binary);
    if (!file_.is_open()) {
        cerr << "Error: Failed to open AOF persistence file: " << filepath_ << "\n";
    }
}

// Destructor ensures file handle is closed cleanly.
AofEngine::~AofEngine() {
    if (file_.is_open()) {
        file_.close();
    }
}

// Appends a write command to appendonly.aof and flushes to disk immediately.
void AofEngine::append(const RespValue& command) {
    if (!file_.is_open()) return;

    string serialized = command.serialize();
    file_.write(serialized.data(), serialized.size());
    file_.flush(); // Ensure immediate flush to disk
}

// Loads appendonly.aof upon startup and replays all write commands into Router.
void AofEngine::loadAndReplay(Router& router) {
    ifstream infile(filepath_, ios::in | ios::binary);
    if (!infile.is_open()) {
        cout << "No existing AOF log found. Starting with a fresh database state.\n";
        return;
    }

    // Read full AOF file into memory buffer
    string buffer((istreambuf_iterator<char>(infile)), istreambuf_iterator<char>());
    infile.close();

    if (buffer.empty()) return;

    size_t replayed_count = 0;

    // Iteratively parse and execute recorded RESP write commands
    while (true) {
        auto req = RespParser::parse(buffer);
        if (!req.has_value()) break;

        // Replay command against Router (pass log_to_aof = false so we don't duplicate log entries!)
        router.dispatch(req.value(), false);
        replayed_count++;
    }

    cout << "AOF Persistence: Successfully replayed " << replayed_count << " write commands from " << filepath_ << "\n";
}

} // namespace miniredis
