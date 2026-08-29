#ifndef MINI_REDIS_SOCKET_HPP
#define MINI_REDIS_SOCKET_HPP

#include <string>

using namespace std;

namespace miniredis {

// Socket class wraps low-level POSIX file descriptors for TCP sockets.
// Manages socket creation, binding, listening, non-blocking mode configuration, and cleanup.
class Socket {
public:
    // Constructor accepting an optional existing socket file descriptor (-1 means uninitialized).
    explicit Socket(int fd = -1);

    // Destructor closes the socket file descriptor if open.
    ~Socket();

    // Disable copy semantics to prevent duplicate closing of socket file descriptors.
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    // Enable move semantics to allow transferring ownership of socket handles.
    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;

    // Creates, configures SO_REUSEADDR, binds to port, and listens for connections.
    bool listenOn(int port, int backlog = 128);

    // Accepts a new client connection on the listening socket.
    // Populates client_ip and client_port, and returns the new client socket file descriptor.
    int acceptConnection(string& client_ip, int& client_port);

    // Enables or disables non-blocking I/O mode using fcntl O_NONBLOCK flag.
    void setNonBlocking(bool non_blocking);

    // Closes the underlying socket file descriptor.
    void closeSocket();

    // Getter for the raw file descriptor.
    int getFd() const { return fd_; }

    // Helper to check if socket file descriptor is valid.
    bool isValid() const { return fd_ != -1; }

private:
    int fd_; // POSIX file descriptor for socket (-1 if invalid/closed)
};

} // namespace miniredis

#endif // MINI_REDIS_SOCKET_HPP
