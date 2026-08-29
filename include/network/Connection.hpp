#ifndef MINI_REDIS_CONNECTION_HPP
#define MINI_REDIS_CONNECTION_HPP

#include "protocol/RespParser.hpp"
#include "commands/Router.hpp"
#include <string>

using namespace std;

namespace miniredis {

// The Connection class represents a single connected client.
// It stores incoming network data in read_buffer_, processes Redis commands,
// and buffers outgoing responses in write_buffer_ so we can send them asynchronously.
class Connection {
public:
    // Create a new client connection wrapper around a client file descriptor.
    Connection(int fd, Router& router);

    // Returns the client's socket descriptor.
    int getFd() const { return fd_; }

    // Called when data is ready to be read from the client socket.
    // Reads data, parses Redis commands, executes them, and queues up responses.
    // Returns false if the client disconnected or an error occurred.
    bool onRead();

    // Called when the client socket is ready to receive data.
    // Flushes unsent response bytes from write_buffer_ to the socket.
    // Returns false if a fatal send error occurs.
    bool onWrite();

    // Checks if there is pending data in write_buffer_ that still needs to be sent to the client.
    bool hasPendingWrites() const { return !write_buffer_.empty(); }

private:
    int fd_;            // The client's socket file descriptor.
    Router& router_;    // Reference to the Redis command router.
    string read_buffer_;  // Buffer for incoming data sent by the client.
    string write_buffer_; // Buffer for outgoing responses waiting to be sent to the client.
};

} // namespace miniredis

#endif // MINI_REDIS_CONNECTION_HPP
