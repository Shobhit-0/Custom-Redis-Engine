# Custom In-Memory Database Engine & Full-Stack Caching Dashboard

A high-performance, Redis-inspired in-memory key-value database server built entirely from scratch in C++. This project bypasses high-level abstractions to interact directly with POSIX system calls, custom memory engines, and network socket APIs. It features a Full-Stack stock price tracking dashboard that visually demonstrates sub-millisecond memory caching advantages over traditional disk-bound databases.

---

## 🏗️ Architectural Overview

The system is split into three distinct decoupled layers:

1. **The Core Database Engine (C++)**: A standalone server listening for raw TCP connections. It manages data entirely in RAM utilizing a custom, self-balancing algorithmic tree and handles connections concurrently via a non-blocking event loop.
2. **The REST API Middleware (Python / Flask)**: Acts as an abstraction bridge. Since web browsers communicate using stateless HTTP, this proxy translates web incoming requests into structured, length-prefixed TCP binary streams handled by the C++ engine.
3. **The Web Dashboard (HTML5 / CSS3 / JavaScript)**: A live client interface simulating stock price queries. It metrics and graphs real-time request latencies to contrast disk-bound database calls against memory-cached retrievals.

---

## 🛠️ Deep Technical Features

* **Custom Treap Implementation**: Rather than relying on standard hash tables or standard library maps, data is arranged in a custom **Treap (Tree + Heap)** data structure. It uses randomized tree priorities and directional rotations to maintain an $O(\log n)$ balanced lookup complexity while keeping keys perfectly sorted.
* **Non-Blocking I/O Event Loop**: Engineered around the Linux/macOS `poll()` system call. Sockets are toggled into `O_NONBLOCK` mode, allowing a single thread to concurrently process thousands of active database clients without blocking execution states.
* **Append-Only File (AOF) Persistence**: To prevent loss of data, a fast $O(1)$ persistence tracker is implemented. Every data mutation (`SET`, `DEL`, `LPUSH`) logs sequentially to an on-disk `database.aof` file. Upon bootstrap, the engine parses and replays the AOF transaction log to fully restore memory state.
* **Time-To-Live (TTL) Automated Eviction**: Features automated volatile memory cleaning. Optional expiration limits can be bound to key spaces. Query pipelines analyze structural timestamps to clean up expired indices on demand.
* **Multi-Model Data Support**: Extends beyond standard key-value string structures. Includes integrated Deque blocks providing message broker primitives via binary-safe `LPUSH` and `RPOP` command queues.

---

## 📂 Project Directory Structure

```text
├── server.cpp       # Main database engine (Sockets, Treap, Event Loop, AOF, TTL)
├── client.cpp       # CLI client utility to dispatch custom network protocols
├── app.py           # Flask HTTP middleware connecting web clients to the TCP core
└── index.html       # Frontend interface charting latency comparisons