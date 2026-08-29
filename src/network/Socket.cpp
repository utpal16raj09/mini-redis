#include "network/Socket.hpp"
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>

using namespace std;

namespace miniredis {

// Constructor saves the given socket file descriptor.
Socket::Socket(int fd) : fd_(fd) {}

// Destructor cleans up by closing the socket descriptor.
Socket::~Socket() {
    closeSocket();
}

// Move constructor transfers socket ownership and resets the old object.
Socket::Socket(Socket&& other) noexcept : fd_(other.fd_) {
    other.fd_ = -1; // Reset old object so its destructor doesn't close our socket!
}

// Move assignment operator handles transferring socket ownership.
Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        closeSocket(); // Close our current socket first
        fd_ = other.fd_;
        other.fd_ = -1;
    }
    return *this;
}

// Creates a TCP socket, binds it to the specified port, and starts listening for connections.
bool Socket::listenOn(int port, int backlog) {
    // 1. Create a standard IPv4 TCP socket
    fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ == -1) {
        cerr << "Error: Failed to create socket\n";
        return false;
    }

    // 2. Allow port reuse immediately upon server restart (prevents "Address already in use" errors)
    int opt = 1;
    setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 3. Configure server address structure
    sockaddr_in addr{};
    addr.sin_family = AF_INET;           // IPv4
    addr.sin_addr.s_addr = INADDR_ANY;   // Listen on any available network adapter
    addr.sin_port = htons(port);         // Convert port number to network byte order

    // 4. Bind the socket to port
    if (bind(fd_, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        cerr << "Error: Could not bind to port " << port << "\n";
        closeSocket();
        return false;
    }

    // 5. Start listening for client connections
    if (listen(fd_, backlog) == -1) {
        cerr << "Error: Could not listen on socket\n";
        closeSocket();
        return false;
    }

    return true;
}

// Accepts an incoming connection from a client.
int Socket::acceptConnection(string& client_ip, int& client_port) {
    sockaddr_in client_addr{};
    socklen_t addr_len = sizeof(client_addr);
    
    // Accept the client connection
    int client_fd = accept(fd_, (struct sockaddr*)&client_addr, &addr_len);

    if (client_fd != -1) {
        // Convert client binary IP address into readable string (e.g. "127.0.0.1")
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr.sin_addr), ip_str, INET_ADDRSTRLEN);
        client_ip = ip_str;
        client_port = ntohs(client_addr.sin_port);
    }
    return client_fd;
}

// Sets the socket to non-blocking mode using Linux fcntl.
// In non-blocking mode, read/write calls return immediately instead of waiting forever.
void Socket::setNonBlocking(bool non_blocking) {
    if (fd_ == -1) return;
    
    int flags = fcntl(fd_, F_GETFL, 0);
    if (flags == -1) return;
    
    if (non_blocking) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }
    
    fcntl(fd_, F_SETFL, flags);
}

// Safely closes the socket descriptor if it's currently open.
void Socket::closeSocket() {
    if (fd_ != -1) {
        close(fd_);
        fd_ = -1;
    }
}

} // namespace miniredis
