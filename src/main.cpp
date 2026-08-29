#include "storage/Database.hpp"
#include "storage/AofEngine.hpp"
#include "commands/Router.hpp"
#include "network/EventLoop.hpp"
#include <iostream>

using namespace std;

int main() {
    // 1. Initialize the in-memory database storage engine
    miniredis::Database db;

    // 2. Initialize AOF persistence engine
    miniredis::AofEngine aof("appendonly.aof");

    // 3. Initialize command router and link to DB and AOF
    miniredis::Router router(db, &aof);

    // 4. Replay existing AOF log to restore persistent database state on startup
    aof.loadAndReplay(router);

    // 5. Create non-blocking epoll EventLoop with active TTL purging and non-blocking I/O
    miniredis::EventLoop loop(6379, router, db);

    // 6. Run reactor event loop
    loop.run();

    return 0;
}