# MiniRedis

A Redis-inspired, single-threaded, event-driven in-memory key-value store built from scratch in C++17.

MiniRedis uses Linux `epoll` for non-blocking I/O, implements the Redis Serialization Protocol (RESP2), supports multiple core Redis data structures, provides TTL-based key expiration, and includes Append-Only File (AOF) persistence with startup recovery.

## Architecture

MiniRedis follows a single-threaded event-driven architecture inspired by the core design principles of Redis.

| Subsystem    | MiniRedis Implementation                        |
| ------------ | ----------------------------------------------- |
| Networking   | Non-blocking TCP sockets with Linux `epoll`     |
| Event Loop   | Single-threaded Reactor pattern                 |
| Protocol     | Stateful RESP2 parser and encoder               |
| Storage      | In-memory hash table using `std::unordered_map` |
| Data Types   | Strings, Hashes, Lists, Sets                    |
| Expiration   | Lazy eviction + active background sampling      |
| Persistence  | Append-Only File (AOF) logging and replay       |
| Build System | CMake                                           |
| Language     | C++17                                           |

## Project Structure

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

## Components

### Network Layer

**`Socket`**

Encapsulates Linux POSIX socket operations including:

* Socket creation
* Address binding
* TCP listening
* Client acceptance
* Non-blocking socket configuration
* Socket lifecycle management

**`Connection`**

Maintains per-client state, including:

* Socket descriptor
* Input buffer
* Output buffer
* Non-blocking reads and writes
* RESP request processing

**`EventLoop`**

Implements the main `epoll`-based Reactor loop.

It manages:

* Client connection events
* `EPOLLIN` read readiness
* `EPOLLOUT` write readiness
* Connection lifecycle
* Periodic background expiration checks

### Protocol Layer

**`RespParser`**

Implements a stateful RESP2 stream parser and encoder.

Supported RESP2 types include:

* Simple Strings
* Errors
* Integers
* Bulk Strings
* Null Bulk Strings
* Arrays

The parser handles partial TCP reads by retaining incomplete data until a complete RESP frame is available.

### Storage Layer

**`Value`**

Provides a polymorphic value representation using `std::variant`.

Supported types:

```text
String      → std::string
Hash        → std::unordered_map
List        → std::deque
Set         → std::unordered_set
```

Each value can optionally contain an expiration timestamp.

**`Database`**

Provides the in-memory data store and implements:

* String operations
* Hash operations
* List operations
* Set operations
* Key management
* TTL handling
* Expired-key cleanup

### Persistence Layer

**`AofEngine`**

Implements Append-Only File persistence.

Mutating commands are serialized using RESP and appended to:

```text
appendonly.aof
```

During startup, the AOF is replayed to reconstruct the in-memory database state.

### Command Layer

**`Router`**

Acts as the command dispatcher.

It:

1. Receives parsed RESP commands.
2. Validates command arguments.
3. Routes commands to the appropriate database operation.
4. Generates RESP responses.
5. Sends mutating commands to the AOF engine for persistence.

## Core Design

### Non-Blocking Event-Driven Networking

MiniRedis uses a single-threaded Reactor architecture based on Linux `epoll`.

```text
                    ┌─────────────────┐
                    │   Event Loop    │
                    │    epoll_wait   │
                    └────────┬────────┘
                             │
              ┌──────────────┼──────────────┐
              │              │              │
           EPOLLIN        EPOLLIN        EPOLLOUT
              │              │              │
              ▼              ▼              ▼
        New Connection    Read Request   Flush Response
              │              │              │
              ▼              ▼              ▼
           Socket       RESP Parser    Write Buffer
                             │
                             ▼
                           Router
                             │
                             ▼
                         Database
```

All application state is managed by a single event-loop thread, avoiding synchronization overhead between worker threads.

### TTL Expiration

MiniRedis uses two expiration mechanisms.

**Lazy eviction**

Expiration is checked when a key is accessed. Expired keys are removed before returning the result.

**Active eviction**

The event loop periodically samples keys and removes expired entries in the background.

This prevents expired keys from remaining indefinitely in memory when they are never accessed.

### AOF Persistence

Write operations are recorded in RESP format:

```text
Client
  │
  ▼
Router
  │
  ├──────────────► Database
  │
  └──────────────► AofEngine
                         │
                         ▼
                  appendonly.aof
```

On restart:

```text
appendonly.aof
       │
       ▼
 RespParser
       │
       ▼
   Router
       │
       ▼
   Database
```

This reconstructs the database state from the persisted command history.

## Supported Commands

| Category          | Commands                                   |
| ----------------- | ------------------------------------------ |
| System            | `PING`, `INFO`, `FLUSHDB`                  |
| Strings           | `SET`, `GET`, `INCR`, `DECR`               |
| Keys & Expiration | `DEL`, `EXISTS`, `EXPIRE`, `TTL`, `KEYS`   |
| Hashes            | `HSET`, `HGET`, `HDEL`, `HGETALL`          |
| Lists             | `LPUSH`, `RPUSH`, `LPOP`, `RPOP`, `LRANGE` |
| Sets              | `SADD`, `SREM`, `SMEMBERS`                 |

### Examples

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

## Building

### Requirements

* Linux
* GCC or Clang with C++17 support
* CMake 3.16 or later

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

The server listens on port `6379`.

Test it using the standard Redis CLI:

```bash
redis-cli -p 6379 PING
```

## Project Goals

MiniRedis was built to explore the systems concepts behind an in-memory database, including:

* TCP socket programming
* Non-blocking I/O
* Linux `epoll`
* Event-driven architecture
* Network protocol design
* Stateful stream parsing
* In-memory data structures
* TTL and memory reclamation
* Persistence and crash recovery
* C++17 systems programming
