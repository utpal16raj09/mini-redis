#include "network/Connection.hpp"
#include <unistd.h>
#include <sys/socket.h>
#include <iostream>

using namespace std;

namespace miniredis {

Connection::Connection(int fd, Router& router) : fd_(fd), router_(router) {}

bool Connection::onRead() {
    char buf[1024];
    ssize_t bytes = recv(fd_, buf, sizeof(buf), 0);
    
    if (bytes <= 0) {
        return false;
    }

    read_buffer_.append(buf, bytes);

    while (true) {
        auto req = RespParser::parse(read_buffer_);
        if (!req.has_value()) {
            break;
        }

        RespValue response = router_.dispatch(req.value());
        string reply = response.serialize();
        write_buffer_.append(reply);
    }

    // Try sending buffered data immediately
    return onWrite();
}

bool Connection::onWrite() {
    if (write_buffer_.empty()) return true;

    ssize_t bytes_sent = send(fd_, write_buffer_.data(), write_buffer_.size(), MSG_DONTWAIT);
    if (bytes_sent > 0) {
        write_buffer_.erase(0, bytes_sent);
    } else if (bytes_sent == -1) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            return false; // Real socket error
        }
    }

    return true; // Still alive even if pending bytes remain in write_buffer_
}

} // namespace miniredis
