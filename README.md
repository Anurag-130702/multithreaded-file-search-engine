# Multithreaded File Search Engine

A multithreaded file search utility built in C++ using a custom thread pool, synchronized task queue, and parallel file processing to improve search performance across large directory structures.

## Features

- Multithreaded file searching using a custom thread pool
- Recursive directory traversal using C++17 filesystem library
- File extension filtering (`.txt`, `.cpp`, `.log`, etc.)
- Single-threaded vs multithreaded benchmark comparison
- Thread synchronization using mutexes and condition variables
- Atomic counters for thread-safe statistics tracking
- Permission-safe filesystem traversal

---

## Architecture

### Workflow

1. Collect files recursively from the target directory
2. Filter files based on allowed extensions
3. Push file-search tasks into a synchronized task queue
4. Worker threads process files concurrently
5. Aggregate benchmark statistics and display results

---

## Concurrency Model

The project uses a custom thread pool implementation:

- Tasks are stored in a shared queue using:
  - `std::queue<std::function<void()>>`
- Worker threads wait on:
  - `std::condition_variable`
- Shared resources are protected using:
  - `std::mutex`
- Statistics tracking uses:
  - `std::atomic`

This avoids repeated thread creation overhead and enables parallel file processing.

---

## Benchmark Example

| Mode | Files Scanned | Matches Found | Time |
|------|------|------|------|
| Single-threaded | 3 | 3 | 0.0097 sec |
| Multithreaded | 3 | 3 | 0.0021 sec |

Observed speedup: **~4.5x**

> Actual performance depends on storage speed, file sizes, and workload distribution.

---

## Benchmark Screenshot

![Benchmark](benchmark.png)

---

## Build & Run

### Compile

```bash
g++ -std=c++17 main.cpp -o file_search
```

### Run

```bash
.\file_search.exe
```

---

## Example Input

```text
Directory: C:\TestSearch
Keyword: hello
Extensions: .txt .cpp
```

---

## Concepts Used

- Multithreading
- Thread Pool Design
- Mutexes
- Condition Variables
- Atomics
- Filesystem APIs
- Producer-Consumer Workflow
- Performance Benchmarking

---

## Future Improvements

- Bounded task queue
- Batched logging system
- Configurable thread count
- Large-scale benchmark dataset generation
- Improved result aggregation

---

## Tech Stack

- C++17
- STL Threads
- Filesystem Library
- GCC 16.1.0