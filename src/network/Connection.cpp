#include "network/Connection.hpp"
#include <unistd.h>
#include <sys/socket.h>
#include <iostream>

using namespace std;

namespace miniredis {

Connection::Connection(int fd, Router& router) : fd_(fd), router_(router) {}

// Triggered whenever the socket has data available to read.
bool Connection::onRead() {
    char buf[1024];
    
    // Read non-blocking data from socket into local stack buffer
    ssize_t bytes = recv(fd_, buf, sizeof(buf), 0);
    
    // bytes <= 0 means either client closed connection (0) or error occurred (-1)
    if (bytes <= 0) {
        return false;
    }

    // Append received bytes to our persistent read buffer
    read_buffer_.append(buf, bytes);

    // Keep parsing and processing complete Redis commands as long as full frames are available
    while (true) {
        auto req = RespParser::parse(read_buffer_);
        if (!req.has_value()) {
            break; // Buffer holds an incomplete command frame; wait for more data from client
        }

        // Execute command and serialize the response back to Redis protocol format
        RespValue response = router_.dispatch(req.value());
        string reply = response.serialize();
        write_buffer_.append(reply);
    }

    // Attempt to flush output buffer to socket right away
    return onWrite();
}

// Triggered when the client socket is ready to accept write bytes.
bool Connection::onWrite() {
    if (write_buffer_.empty()) return true;

    // Send buffered bytes asynchronously (non-blocking)
    ssize_t bytes_sent = send(fd_, write_buffer_.data(), write_buffer_.size(), MSG_DONTWAIT);
    if (bytes_sent > 0) {
        // Erase the sent bytes from the beginning of our write buffer
        write_buffer_.erase(0, bytes_sent);
    } else if (bytes_sent == -1) {
        // EAGAIN or EWOULDBLOCK means the socket buffer is full for now; try again on next loop
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            return false; // Fatal socket write error
        }
    }

    return true;
}

} // namespace miniredis
