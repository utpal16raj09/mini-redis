#include "network/EventLoop.hpp"
#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>

using namespace std;

namespace miniredis {

constexpr int MAX_EVENTS = 64;

EventLoop::EventLoop(int port, Router& router, Database& db) 
    : port_(port), epoll_fd_(-1), running_(false), router_(router), db_(db) {}

EventLoop::~EventLoop() {
    stop();
}

// Registers a socket file descriptor with epoll for incoming READ events (EPOLLIN).
void EventLoop::addFd(int fd) {
    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev);
}

// Updates interest flags for a client socket. Registers EPOLLOUT only when we have unsent write data.
void EventLoop::updateFdFlags(int fd, bool enable_write) {
    epoll_event ev{};
    ev.events = EPOLLIN | (enable_write ? EPOLLOUT : 0);
    ev.data.fd = fd;
    epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev);
}

// Removes a client socket from epoll, closes its connection, and frees memory.
void EventLoop::removeFd(int fd) {
    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
    close(fd);
    connections_.erase(fd);
}

// Stops the event loop.
void EventLoop::stop() {
    running_ = false;
    if (epoll_fd_ != -1) {
        close(epoll_fd_);
        epoll_fd_ = -1;
    }
}

// Main event loop execution function.
void EventLoop::run() {
    // 1. Start listening on TCP port
    if (!server_socket_.listenOn(port_)) {
        cerr << "Failed to start server on port " << port_ << "\n";
        return;
    }

    server_socket_.setNonBlocking(true);

    // 2. Create epoll instance in Linux kernel
    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ == -1) {
        cerr << "Failed to create epoll instance\n";
        return;
    }

    // 3. Register server socket with epoll so we get notified when new clients connect
    addFd(server_socket_.getFd());
    running_ = true;

    cout << "MiniRedis Server running on port " << port_ << " (epoll reactor pattern + AOF + Active Expiry)\n";

    epoll_event events[MAX_EVENTS];

    // 4. Main Event Loop
    while (running_) {
        // Wait for network events with a 100ms timeout
        int nfds = epoll_wait(epoll_fd_, events, MAX_EVENTS, 100);
        
        // Active Expiration: Purge expired keys in background every loop iteration
        db_.purgeExpiredKeys(20);

        if (nfds == -1) {
            if (errno == EINTR) continue; // Signal interruption
            cerr << "epoll_wait error\n";
            break;
        }

        // Loop through all ready events returned by kernel
        for (int i = 0; i < nfds; ++i) {
            int fd = events[i].data.fd;
            uint32_t evs = events[i].events;

            if (fd == server_socket_.getFd()) {
                // Event on server socket means a NEW CLIENT is trying to connect
                string client_ip;
                int client_port;
                int client_fd = server_socket_.acceptConnection(client_ip, client_port);
                
                if (client_fd != -1) {
                    // Set non-blocking mode on new client socket descriptor
                    int flags = fcntl(client_fd, F_GETFL, 0);
                    if (flags != -1) {
                        fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
                    }

                    // Create connection tracking object and add socket to epoll
                    connections_[client_fd] = make_unique<Connection>(client_fd, router_);
                    addFd(client_fd);
                    cout << "Client connected from " << client_ip << ":" << client_port << "\n";
                }
            } else {
                // Event on client socket means READ data is available or socket is ready for WRITE
                auto it = connections_.find(fd);
                if (it == connections_.end()) continue;

                bool alive = true;

                // Handle incoming read data
                if (evs & EPOLLIN) {
                    alive = it->second->onRead();
                }

                // Handle outgoing write data
                if (alive && (evs & EPOLLOUT)) {
                    alive = it->second->onWrite();
                }

                if (!alive) {
                    cout << "Client disconnected (fd: " << fd << ")\n";
                    removeFd(fd);
                } else {
                    // Register EPOLLOUT interest only if there are unsent bytes in write buffer
                    updateFdFlags(fd, it->second->hasPendingWrites());
                }
            }
        }
    }
}

} // namespace miniredis
