# 🔴 MiniRedis

A high-performance, single-threaded, event-driven in-memory Redis server implementation written in modern C++17 from scratch.

MiniRedis uses Linux `epoll` I/O multiplexing, implements the official Redis Serialization Protocol (RESP2), supports multiple core Redis data structures (Strings, Hashes, Lists, Sets), features dual-mode TTL expiration, and provides append-only file (AOF) persistence logging and recovery replay.

---

## 🔍 How Close is MiniRedis to Real Production Redis?

| Architectural Dimension | Real Production Redis | MiniRedis Implementation | Similarity Level |
|---|---|---|---|
| **Networking & I/O Model** | Single-threaded event loop via `epoll` / `kqueue` / `select` | Single-threaded non-blocking reactor loop using Linux `epoll_wait` | 🎯 **95% Identical** |
| **Wire Protocol** | RESP2 & RESP3 framing formats | State-based stream parser for **RESP2** (`+`, `-`, `:`, `$`, `*`) | 🎯 **90% Identical** |
| **Client Socket Buffering** | Non-blocking input/output ring buffers with `EPOLLOUT` interest | Asynchronous non-blocking read/write buffers per connection handle | 🎯 **90% Identical** |
| **Data Type Support** | Strings, Hashes, Lists, Sets, Sorted Sets, Bitmaps, HyperLogLogs | Strings, Hashes (`unordered_map`), Lists (`deque`), Sets (`unordered_set`) | 🎯 **80% Identical** |
| **Key Eviction / Expiration** | Lazy eviction on key access + Active background random sampling (100ms) | Dual-layer: Lazy eviction on access + Active random sampling every 100ms | 🎯 **90% Identical** |
| **Persistence Engine** | RDB snapshots + AOF log file appending / rewriting | Write-Ahead Append-Only File (`appendonly.aof`) log & startup replay | 🎯 **85% Identical** |

---

## 📁 File Structure & Component Guide

### 🌐 Network Layer (`include/network/` & `src/network/`)
* **`Socket.hpp` / `Socket.cpp`**: 
  * Wraps Linux POSIX socket APIs (`socket()`, `bind()`, `listen()`, `accept()`, `fcntl()`).
  * Manages socket lifecycle and configures non-blocking flags (`O_NONBLOCK`).
* **`Connection.hpp` / `Connection.cpp`**: 
  * Manages per-client state, connection file descriptor, streaming read buffer (`read_buffer_`), and asynchronous write buffer (`write_buffer_`).
  * Handles non-blocking `recv()` and non-blocking `send()` (`MSG_DONTWAIT`).
* **`EventLoop.hpp` / `EventLoop.cpp`**: 
  * Core **epoll Reactor Event Loop**.
  * Registers listening socket and client sockets into Linux `epoll` interest list (`EPOLLIN` for reading, `EPOLLOUT` when write buffers are pending).
  * Executes background tasks (active TTL eviction) during `epoll_wait` timeouts.

### 📜 Protocol Layer (`include/protocol/` & `src/protocol/`)
* **`RespParser.hpp` / `RespParser.cpp`**: 
  * Implements the **RESP (Redis Serialization Protocol)** encoder and stateful decoder.
  * Constructs and parses Simple Strings (`+`), Errors (`-`), Integers (`:`), Bulk Strings (`$`), Nulls (`$-1`), and Arrays (`*`).

### 💾 Storage Layer (`include/storage/` & `src/storage/`)
* **`Value.hpp`**: 
  * Variant wrapper (`std::variant`) supporting multiple C++ data structures (`string`, `unordered_map`, `deque`, `unordered_set`) alongside TTL expiration timestamps (`chrono::steady_clock`).
* **`Database.hpp` / `Database.cpp`**: 
  * Central in-memory hash table (`unordered_map<string, Value>`).
  * Provides atomic string, hash, list, set, and key management operations.
  * Implements `purgeExpiredKeys()` for active background memory reclamation.
* **`AofEngine.hpp` / `AofEngine.cpp`**: 
  * Append-Only File (`appendonly.aof`) persistence logger.
  * Logs state-mutating write commands to disk and replays them sequentially upon server startup.

### ⚙️ Command & Entry Point (`include/commands/`, `src/commands/`, `src/`)
* **`Router.hpp` / `Router.cpp`**: 
  * Command pattern dispatcher. Decodes incoming RESP requests and routes them to appropriate database methods. Writes mutating commands to `AofEngine`.
* **`main.cpp`**: 
  * Application entry point. Instantiates `Database`, `AofEngine`, `Router`, replays AOF history, and starts the `EventLoop` on port `6379`.

---

## 🛠️ Detailed Implementation Mechanics

### 1. Epoll Non-Blocking Reactor Loop
Instead of creating a thread per connection or using blocking calls:
1. Sockets are marked non-blocking via `fcntl(fd, F_SETFL, flags | O_NONBLOCK)`.
2. The server main loop calls `epoll_wait(epoll_fd, events, MAX_EVENTS, 100)`.
3. When `EPOLLIN` triggers on the server socket, a new client is accepted.
4. When `EPOLLIN` triggers on a client socket, data is read into `read_buffer_` and parsed into RESP commands.
5. When `EPOLLOUT` triggers, pending data in `write_buffer_` is flushed asynchronously.

### 2. Dual-Layer TTL Eviction
1. **Lazy Eviction:** Every key lookup in `get()`, `exists()`, `hget()`, etc., checks if the expiration timestamp has passed. If expired, the key is immediately erased and `nullopt` is returned.
2. **Active Eviction:** Every 100ms cycle during `epoll_wait` timeout, `purgeExpiredKeys(20)` samples up to 20 keys from the database and deletes any expired keys automatically.

### 3. AOF Persistence & Startup Replay
1. Every write command (`SET`, `DEL`, `INCR`, `HSET`, `LPUSH`, `SADD`, etc.) is serialized back to standard RESP string format.
2. The serialized bytes are appended to `appendonly.aof` and flushed to disk.
3. On boot, `AofEngine::loadAndReplay()` reads `appendonly.aof` line-by-line using `RespParser` and executes commands back into `Router` (with AOF re-logging suppressed).

---

## 📋 Supported Commands Reference

| Category | Commands |
|---|---|
| **System** | `PING [msg]`, `INFO`, `FLUSHDB` |
| **Strings** | `SET key value [EX sec \| PX ms]`, `GET key`, `INCR key`, `DECR key` |
| **Key & Expiration** | `DEL key1 [key2]`, `EXISTS key1 [key2]`, `EXPIRE key seconds`, `TTL key`, `KEYS pattern` |
| **Hashes** | `HSET key field value`, `HGET key field`, `HDEL key field`, `HGETALL key` |
| **Lists** | `LPUSH key val1 [val2]`, `RPUSH key val1 [val2]`, `LPOP key`, `RPOP key`, `LRANGE key start stop` |
| **Sets** | `SADD key mem1 [mem2]`, `SREM key mem1 [mem2]`, `SMEMBERS key` |

---

## 🚀 Building & Running

### Prerequisites
* GCC/Clang with C++17 support
* CMake 3.16+
* Linux OS (for `epoll` support)

### Build Commands
```bash
mkdir -p build && cd build
cmake ..
make
```

### Run Server
```bash
./miniredis
```

### Test with `redis-cli`
In a separate terminal, run:
```bash
redis-cli -p 6379 PING
redis-cli -p 6379 SET greeting "Hello MiniRedis"
redis-cli -p 6379 GET greeting
redis-cli -p 6379 HSET user:100 name "Alice" email "alice@example.com"
redis-cli -p 6379 HGETALL user:100
redis-cli -p 6379 LPUSH queue job1 job2 job3
redis-cli -p 6379 LRANGE queue 0 -1
```
