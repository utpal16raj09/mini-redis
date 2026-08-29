#ifndef MINI_REDIS_CONNECTION_HPP
#define MINI_REDIS_CONNECTION_HPP

#include "protocol/RespParser.hpp"
#include "commands/Router.hpp"
#include <string>

using namespace std;

namespace miniredis {

// Connection manages per-client socket non-blocking reads and async write buffering.
class Connection {
public:
    Connection(int fd, Router& router);

    int getFd() const { return fd_; }

    // Reads incoming client data from socket
    bool onRead();

    // Sends buffered output data when EPOLLOUT is triggered
    bool onWrite();

    // Returns true if there is unsent data in the write buffer
    bool hasPendingWrites() const { return !write_buffer_.empty(); }

private:
    int fd_;
    Router& router_;
    string read_buffer_;
    string write_buffer_;
};

} // namespace miniredis

#endif // MINI_REDIS_CONNECTION_HPP
