#include "network/Socket.hpp"
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>

using namespace std;

namespace miniredis {

// Constructor initializing socket with a file descriptor handle.
Socket::Socket(int fd) : fd_(fd) {}

// Destructor automatically cleans up open socket descriptors.
Socket::~Socket() {
    closeSocket();
}

// Move constructor transfers ownership of socket file descriptor from 'other'.
Socket::Socket(Socket&& other) noexcept : fd_(other.fd_) {
    other.fd_ = -1; // Reset source object's fd so destructor won't close it
}

// Move assignment operator handles transferring ownership of open socket descriptor.
Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        closeSocket(); // Close existing file descriptor before assignment
        fd_ = other.fd_;
        other.fd_ = -1;
    }
    return *this;
}

// Bind socket to port and start listening for connections.
bool Socket::listenOn(int port, int backlog) {
    // 1. Create IPv4 TCP socket (AF_INET = IPv4, SOCK_STREAM = TCP)
    fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ == -1) {
        cerr << "Failed to create socket\n";
        return false;
    }

    // 2. Set SO_REUSEADDR option so server can restart immediately without waiting for TIME_WAIT state to clear
    int opt = 1;
    setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 3. Configure server address structure
    sockaddr_in addr{};
    addr.sin_family = AF_INET;           // IPv4
    addr.sin_addr.s_addr = INADDR_ANY;   // Bind to any available IP interface (0.0.0.0)
    addr.sin_port = htons(port);         // Convert port number to network byte order (Big Endian)

    // 4. Bind socket file descriptor to configured port and address
    if (bind(fd_, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        cerr << "Failed to bind to port " << port << "\n";
        closeSocket();
        return false;
    }

    // 5. Place socket into listening state to accept incoming connection requests
    if (listen(fd_, backlog) == -1) {
        cerr << "Failed to listen on socket\n";
        closeSocket();
        return false;
    }

    return true;
}

// Accept new incoming client connection.
int Socket::acceptConnection(string& client_ip, int& client_port) {
    sockaddr_in client_addr{};
    socklen_t addr_len = sizeof(client_addr);
    
    // Accept client connection request
    int client_fd = accept(fd_, (struct sockaddr*)&client_addr, &addr_len);

    if (client_fd != -1) {
        // Convert binary client IP address to readable dot-decimal string format
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr.sin_addr), ip_str, INET_ADDRSTRLEN);
        client_ip = ip_str;
        client_port = ntohs(client_addr.sin_port); // Convert network byte order to host integer
    }
    return client_fd;
}

// Set file descriptor to non-blocking I/O mode using fcntl system call.
void Socket::setNonBlocking(bool non_blocking) {
    if (fd_ == -1) return;
    
    // Retrieve current file descriptor flags
    int flags = fcntl(fd_, F_GETFL, 0);
    if (flags == -1) return;
    
    // Modify O_NONBLOCK flag based on input
    if (non_blocking) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }
    
    // Save updated flags back to file descriptor
    fcntl(fd_, F_SETFL, flags);
}

// Safely close open socket descriptor.
void Socket::closeSocket() {
    if (fd_ != -1) {
        close(fd_);
        fd_ = -1;
    }
}

} // namespace miniredis
