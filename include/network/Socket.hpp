#ifndef MINI_REDIS_SOCKET_HPP
#define MINI_REDIS_SOCKET_HPP

#include <string>

using namespace std;

namespace miniredis {

// The Socket class handles creating, binding, listening, and closing network sockets.
// It wraps standard Linux socket calls so we don't have to manually manage raw socket numbers everywhere.
class Socket {
public:
    // Create a Socket wrapper. Defaults to an uninitialized socket (-1).
    explicit Socket(int fd = -1);

    // Destructor automatically closes the network socket when the object is destroyed.
    ~Socket();

    // Prevent copying to avoid two objects closing the exact same network socket.
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    // Allow moving ownership of a socket from one object to another.
    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;

    // Binds the socket to a port (default: 6379) and starts listening for client connections.
    bool listenOn(int port, int backlog = 128);

    // Accepts a new client connecting to the server.
    // Returns the client's file descriptor, and fills in client_ip and client_port.
    int acceptConnection(string& client_ip, int& client_port);

    // Sets the socket to non-blocking mode so network calls like read/write don't freeze the server.
    void setNonBlocking(bool non_blocking);

    // Safely closes the socket file descriptor.
    void closeSocket();

    // Returns the raw socket handle (file descriptor number).
    int getFd() const { return fd_; }

    // Checks if this socket is open and valid.
    bool isValid() const { return fd_ != -1; }

private:
    int fd_; // The raw Linux file descriptor number representing this network connection.
};

} // namespace miniredis

#endif // MINI_REDIS_SOCKET_HPP
