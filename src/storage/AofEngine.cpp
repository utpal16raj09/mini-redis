#include "storage/AofEngine.hpp"
#include "commands/Router.hpp"
#include <iostream>

using namespace std;

namespace miniredis {

AofEngine::AofEngine(const string& filename) : filename_(filename) {
    // Open file stream in append mode
    file_stream_.open(filename_, ios::out | ios::app | ios::binary);
    if (!file_stream_.is_open()) {
        cerr << "Failed to open AOF file: " << filename_ << "\n";
    }
}

AofEngine::~AofEngine() {
    if (file_stream_.is_open()) {
        file_stream_.close();
    }
}

void AofEngine::append(const RespValue& command) {
    if (!file_stream_.is_open()) return;

    string serialized = command.serialize();
    file_stream_.write(serialized.data(), serialized.size());
    file_stream_.flush(); // Ensure data is flushed to disk
}

void AofEngine::loadAndReplay(Router& router) {
    ifstream infile(filename_, ios::in | ios::binary);
    if (!infile.is_open()) {
        cout << "No existing AOF file found. Starting fresh.\n";
        return;
    }

    cout << "Replaying AOF file: " << filename_ << "...\n";

    string buffer((istreambuf_iterator<char>(infile)), istreambuf_iterator<char>());
    infile.close();

    size_t commands_replayed = 0;
    while (!buffer.empty()) {
        auto req = RespParser::parse(buffer);
        if (!req.has_value()) break;

        // Dispatch command to database without re-logging to AOF loop
        router.dispatch(req.value(), false);
        commands_replayed++;
    }

    cout << "AOF replay complete. Replayed " << commands_replayed << " commands.\n";
}

} // namespace miniredis
