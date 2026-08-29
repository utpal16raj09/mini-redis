# MiniRedis

A high-performance, single-threaded, event-driven in-memory Redis server implementation written in modern C++17 from scratch.

MiniRedis uses Linux `epoll` I/O multiplexing, implements the Redis Serialization Protocol (RESP2), supports multiple core Redis data structures including Strings, Hashes, Lists, and Sets, features dual-mode TTL expiration, and provides Append-Only File (AOF) persistence with recovery replay.

---

## How Close is MiniRedis to Production Redis?

| Architectural Dimension     | Production Redis                                                 | MiniRedis Implementation                                                   | Similarity |
| --------------------------- | ---------------------------------------------------------------- | -------------------------------------------------------------------------- | ---------- |
| **Networking & I/O Model**  | Single-threaded event loop via `epoll` / `kqueue` / `select`     | Single-threaded non-blocking reactor using Linux `epoll_wait`              | 95%        |
| **Wire Protocol**           | RESP2 & RESP3 framing formats                                    | Stateful stream parser for **RESP2** (`+`, `-`, `:`, `$`, `*`)             | 90%        |
| **Client Socket Buffering** | Non-blocking input/output buffers with writable-event handling   | Asynchronous non-blocking read/write buffers per connection                | 90%        |
| **Data Types**              | Strings, Hashes, Lists, Sets, Sorted Sets, Bitmaps, HyperLogLogs | Strings, Hashes (`unordered_map`), Lists (`deque`), Sets (`unordered_set`) | 80%        |
| **Key Expiration**          | Lazy eviction + active background random sampling                | Lazy eviction + active random sampling every 100ms                         | 90%        |
| **Persistence Engine**      | RDB snapshots + AOF log appending / rewriting                    | Append-Only File logging and startup replay                                | 85%        |

> Similarity figures represent architectural comparisons and are not benchmark-based compatibility measurements.

---

## File Structure

```text
mini-redis/
├── CMakeLists.txt
├── include/
│   ├── network/
│   │   ├── Socket.hpp
│   │   ├── Connection.hpp
│   │   └── EventLoop.hpp
│   ├── protocol/
│   │   └── RespParser.hpp
│   ├── storage/
│   │   ├── Value.hpp
│   │   ├── Database.hpp
│   │   └── AofEngine.hpp
│   └── commands/
│       └── Router.hpp
└── src/
    ├── network/
    │   ├── Socket.cpp
    │   ├── Connection.cpp
    │   └── EventLoop.cpp
    ├── protocol/
    │   └── RespParser.cpp
    ├── storage/
    │   ├── Database.cpp
    │   └── AofEngine.cpp
    ├── commands/
    │   └── Router.cpp
    └── main.cpp
```

---

## Component Guide

### Network Layer

#### `Socket.hpp` / `Socket.cpp`

Wraps Linux POSIX socket APIs:

* `socket()`
* `bind()`
* `listen()`
* `accept()`
* `fcntl()`

Responsibilities include:

* TCP socket creation
* Address binding
* Listening on port `6379`
* Client acceptance
* Non-blocking socket configuration using `O_NONBLOCK`
* Socket lifecycle management

#### `Connection.hpp` / `Connection.cpp`

Manages per-client connection state, including:

* Socket descriptor
* Read buffer
* Write buffer
* Non-blocking `recv()`
* Non-blocking `send()`
* RESP request processing
* Pending response management

`EPOLLOUT` is enabled when a connection has pending response data that needs to be flushed.

#### `EventLoop.hpp` / `EventLoop.cpp`

Implements the core single-threaded `epoll` Reactor loop.

Responsibilities include:

* Creating the `epoll` instance
* Registering the listening socket
* Registering client sockets
* Handling `EPOLLIN`
* Handling `EPOLLOUT`
* Accepting new connections
* Dispatching client read/write events
* Triggering periodic TTL expiration cleanup

---

### Protocol Layer

#### `RespParser.hpp` / `RespParser.cpp`

Implements a stateful Redis Serialization Protocol (RESP2) parser and encoder.

Supported RESP2 types include:

* Simple Strings (`+`)
* Errors (`-`)
* Integers (`:`)
* Bulk Strings (`$`)
* Null Bulk Strings (`$-1`)
* Arrays (`*`)

The parser operates on a stream buffer rather than assuming that one TCP `recv()` call contains one complete command.

It handles:

* Partial TCP reads
* `\r\n` delimiters
* Complete RESP frame detection
* Extraction of command arguments
* Consumption of only successfully parsed frames

---

### Storage Layer

#### `Value.hpp`

Defines the internal representation of stored values using `std::variant`.

Supported types:

```text
String  → std::string
Hash    → std::unordered_map<std::string, std::string>
List    → std::deque<std::string>
Set     → std::unordered_set<std::string>
```

Each stored value can optionally contain an expiration timestamp using `std::chrono::steady_clock`.

Helper functionality includes:

* Expiration checks
* TTL calculation
* Expiration timestamp management

#### `Database.hpp` / `Database.cpp`

Implements the central in-memory key-value store using:

```cpp
std::unordered_map<std::string, Value>
```

The database provides:

* String operations
* Hash operations
* List operations
* Set operations
* Key management
* TTL handling
* Expired-key cleanup

`purgeExpiredKeys()` performs active expiration by sampling keys and removing expired entries.

---

### Persistence Layer

#### `AofEngine.hpp` / `AofEngine.cpp`

Implements Append-Only File persistence and startup recovery.

Mutating commands are serialized into RESP format and appended to:

```text
appendonly.aof
```

On startup, the AOF is parsed and replayed to reconstruct the in-memory database state.

The replay process suppresses re-logging to prevent commands from being appended to the AOF again during recovery.

---

### Command Layer

#### `Router.hpp` / `Router.cpp`

Implements command dispatch and execution.

The router:

1. Receives parsed RESP requests.
2. Validates command arguments.
3. Routes commands to the appropriate database operation.
4. Generates RESP responses.
5. Sends mutating commands to the AOF engine for persistence.

#### `main.cpp`

Application entry point responsible for:

* Creating the `Database`
* Creating the `AofEngine`
* Creating the `Router`
* Replaying persisted AOF data
* Starting the `EventLoop`
* Listening on port `6379`

---

## Core Implementation Mechanics

### 1. Non-Blocking `epoll` Reactor

MiniRedis uses a single-threaded event-driven architecture based on Linux `epoll`.

1. Server and client sockets are configured as non-blocking using `fcntl()`.
2. The main loop calls `epoll_wait()` to wait for I/O events.
3. `EPOLLIN` on the listening socket triggers client acceptance.
4. `EPOLLIN` on client sockets triggers non-blocking reads.
5. Incoming bytes are accumulated in the connection read buffer.
6. Complete RESP frames are parsed and dispatched through the router.
7. Responses are stored in the connection write buffer.
8. `EPOLLOUT` is enabled when pending response data needs to be flushed.

```text
                    EventLoop
                       |
                  epoll_wait()
                       |
          +------------+------------+
          |                         |
       EPOLLIN                   EPOLLOUT
          |                         |
          v                         v
   Read client data          Flush write buffer
          |
          v
     RespParser
          |
          v
        Router
          |
          v
       Database
```

All application state is managed by a single event-loop thread, avoiding synchronization between worker threads.

---

### 2. Dual-Mode TTL Expiration

MiniRedis uses two expiration mechanisms.

#### Lazy Eviction

Expiration is checked when a key is accessed through operations such as:

* `GET`
* `EXISTS`
* `HGET`
* Other key-based operations

If the key has expired, it is removed immediately and treated as missing.

#### Active Eviction

Every 100ms, the event loop performs background expiration.

A limited number of keys are randomly sampled:

```text
purgeExpiredKeys(20)
```

Expired keys found during sampling are removed from memory.

This combination ensures that expired keys are removed both when accessed and when they remain unused.

---

### 3. AOF Persistence and Crash Recovery

Mutating commands are serialized using RESP and appended to the AOF.

```text
Client
  |
  v
Router
  |
  +-------------> Database
  |
  +-------------> AofEngine
                       |
                       v
                appendonly.aof
```

During startup:

```text
appendonly.aof
       |
       v
  RespParser
       |
       v
     Router
       |
       v
   Database
```

The command history is replayed to reconstruct the in-memory database state.

---

## Supported Commands

| Category             | Commands                                   |
| -------------------- | ------------------------------------------ |
| **System**           | `PING`, `INFO`, `FLUSHDB`                  |
| **Strings**          | `SET`, `GET`, `INCR`, `DECR`               |
| **Key & Expiration** | `DEL`, `EXISTS`, `EXPIRE`, `TTL`, `KEYS`   |
| **Hashes**           | `HSET`, `HGET`, `HDEL`, `HGETALL`          |
| **Lists**            | `LPUSH`, `RPUSH`, `LPOP`, `RPOP`, `LRANGE` |
| **Sets**             | `SADD`, `SREM`, `SMEMBERS`                 |

### Example Usage

```bash
redis-cli -p 6379 PING
```

```bash
redis-cli -p 6379 SET greeting "Hello MiniRedis"
redis-cli -p 6379 GET greeting
```

```bash
redis-cli -p 6379 HSET user:100 name "Alice" email "alice@example.com"
redis-cli -p 6379 HGETALL user:100
```

```bash
redis-cli -p 6379 LPUSH queue job1 job2 job3
redis-cli -p 6379 LRANGE queue 0 -1
```

```bash
redis-cli -p 6379 SADD tags cpp linux redis
redis-cli -p 6379 SMEMBERS tags
```

---

## Building and Running

### Prerequisites

* Linux
* GCC or Clang with C++17 support
* CMake 3.16+
* Redis CLI for testing

### Build

```bash
git clone https://github.com/utpal16raj09/mini-redis.git
cd mini-redis

mkdir -p build
cd build

cmake ..
make
```

### Run

```bash
./miniredis
```

The server listens on:

```text
localhost:6379
```

### Test

In another terminal:

```bash
redis-cli -p 6379 PING
```

Example:

```bash
redis-cli -p 6379 SET mykey "Hello MiniRedis"
redis-cli -p 6379 GET mykey
```

---

## Design Goals

MiniRedis was built to explore the systems concepts behind an in-memory database and Redis-style server architecture, including:

* TCP socket programming
* Non-blocking I/O
* Linux `epoll`
* Event-driven architecture
* RESP protocol design
* Stateful stream parsing
* In-memory data structures
* TTL-based expiration
* Active memory reclamation
* Append-Only File persistence
* Crash recovery
* C++17 systems programming
* Modular software architecture

## License

This project is intended for educational and systems-programming purposes.
