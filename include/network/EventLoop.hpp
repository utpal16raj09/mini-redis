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

class EventLoop {
public:
    EventLoop(int port, Router& router, Database& db);
    ~EventLoop();

    void run();
    void stop();

private:
    int port_;
    int epoll_fd_;
    bool running_;
    Socket server_socket_;
    Router& router_;
    Database& db_;

    unordered_map<int, unique_ptr<Connection>> connections_;

    void addFd(int fd);
    void updateFdFlags(int fd, bool enable_write);
    void removeFd(int fd);
};

} // namespace miniredis

#endif // MINI_REDIS_EVENT_LOOP_HPP
