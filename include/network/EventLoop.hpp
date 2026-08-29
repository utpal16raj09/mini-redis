#ifndef MINI_REDIS_EVENT_LOOP_HPP
#define MINI_REDIS_EVENT_LOOP_HPP

#include "network/Socket.hpp"
#include "network/Connection.hpp"
#include "commands/Router.hpp"
#include "storage/Database.hpp"
#include <unordered_map>
#include <memory>

using namespace std;

namespace miniredis {

// The EventLoop class is the core reactor engine of MiniRedis.
// It uses Linux epoll to monitor all client sockets concurrently on a single thread.
// It also runs active background tasks, such as purging expired keys.
class EventLoop {
public:
    // Initialize EventLoop with TCP port, database router, and database instance.
    EventLoop(int port, Router& router, Database& db);

    // Clean up epoll descriptor and open client sockets.
    ~EventLoop();

    // Starts the main epoll event loop. Runs until stop() is called.
    void run();

    // Stops the main event loop and closes epoll.
    void stop();

private:
    int port_;             // TCP port number (default: 6379)
    int epoll_fd_;         // Linux epoll instance file descriptor
    bool running_;         // Loop state flag
    Socket server_socket_; // Server listening socket
    Router& router_;       // Redis command router engine
    Database& db_;         // In-memory database instance

    // Map storing all active client connections (client_fd -> Connection object)
    unordered_map<int, unique_ptr<Connection>> connections_;

    void addFd(int fd);
    void updateFdFlags(int fd, bool enable_write);
    void removeFd(int fd);
};

} // namespace miniredis

#endif // MINI_REDIS_EVENT_LOOP_HPP
