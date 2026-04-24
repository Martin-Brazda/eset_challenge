# ESET Challenge - Multithreaded Recursive String Search (C++17)

## Overview

This project implements a recursive file search utility for the ESET challenge:

- Input: `<path> <needle>`
- Output format: `<file>(<position>): <prefix>...<suffix>`
- Recursive directory traversal
- Large file handling with bounded memory usage
- Multithreaded processing
- Defensive error handling

The implementation prioritizes:

- predictable memory behavior on large files
- practical throughput tuning via `.env`

---

## Build

```bash
make
```

This compiles `main` with:

- `-std=c++17`
- `-pthread`
- `-O2 -march=native`
- warnings enabled (`-Wall -Wextra`)

Clean build artifacts:

```bash
make clean
```

---

## Usage

```bash
./main <filepath> <string>
./main --help
```

### CLI behavior

- Exactly two positional arguments are required.
- Needle must be non-empty and at most 128 characters.
- Exit code:
  - `0` if at least one match is found (or help is requested)
  - `1` on errors or when no match is found

---

## Output Semantics

For each match:

```text
<file>(<position>): <prefix>...<suffix>
```

- `<file>`: currently emitted as filename (basename)
- `<position>`: zero-based byte offset
- `<prefix>`: up to 3 bytes before match
- `<suffix>`: up to 3 bytes after match
- escapes in prefix/suffix:
  - tab -> `\t`
  - newline -> `\n`
  - carriage return -> `\r`
  - backslash -> `\\`
  - other non-printable bytes -> `\xNN`

---

## Configuration (`.env`)

Default values are provided in `.env.example`:

- `NUM_THREADS`: worker thread count
- `QUEUE_MAX_SIZE`: bounded task queue size (`0` means unbounded)
- `LOGGING_ENABLED`: boolean toggle
- `SMALL_FILE_THRESHOLD`: files up to this size are read in one buffer
- `CHUNKING_THRESHOLD`: chunk size for large-file range scanning
- `MULTIFIND`: boolean toggle to report all non-overlapping matches per file
- `BATCH_SIZE`: number of small files grouped into one task

`Config::load_env()` supports human-readable sizes, e.g. `64KB`, `64MB`, `1GB`.

---

## Architecture

## Components

- `CLIParser`
  - validates arguments and needle constraints
  - returns `ParseResult` (`Ok`, `Help`, `Error`)

- `Config`
  - parses `.env` and exposes runtime tunables

- `SearchEngine`
  - wraps `std::boyer_moore_searcher` for in-memory search

- `FileSearcher`
  - searches file content either as:
    - small-file full-buffer scan, or
    - large-file range scan (`mmap`) with overlap handling
  - computes match offset + context

- `DirectoryWalker`
  - recursively traverses files/directories
  - dispatches work to thread pool
  - batches small files
  - splits large files into chunk ranges
  - serializes stdout output safely

- `ThreadPool` + `ThreadQueue`
  - fixed worker pool
  - blocking producer/consumer queue with optional bounded capacity

---

## Data Flow

1. Parse args and validate input.
2. Load `.env` config.
3. Build `ThreadPool`, `SearchEngine`, `FileSearcher`, `DirectoryWalker`.
4. Traverse input path recursively:
   - small files -> buffered and batched
   - large files -> chunked range tasks
5. Each worker emits `SearchResult` entries.
6. Results are escaped/formatted and printed thread-safely.
7. Program returns success/failure code.

---

## Large File Strategy

Large files are scanned with chunked windows:

- each chunk has overlap (`needle_len - 1`) to preserve boundary matches
- context extraction uses an expanded mapped window (extra bytes before and after)
- tasks are grouped into chunk ranges to reduce scheduling overhead
- open large-file descriptors are globally bounded to control FD pressure

This avoids loading huge files into memory while keeping boundary correctness.

---

## Concurrency Model

- fixed-size worker pool from `NUM_THREADS`
- tasks submitted by `DirectoryWalker`
- small-file tasks process file batches
- large-file tasks process multiple chunk ranges
- output contention is reduced by:
  - formatting task-local output buffers first
  - taking the stdout mutex once per result batch

---

## Error Handling and Robustness

The implementation uses:

- argument validation with clear error messages
- `std::error_code` filesystem checks where possible
- defensive handling for file open/read/mmap failures
- recursive traversal guarded by exception handling
- top-level exception boundary in `main`

When individual files fail, processing continues where reasonable.

---

## Performance Notes

Observed performance is workload-dependent:

- many tiny files -> metadata and traversal dominate
- large files -> chunk size and storage throughput dominate
- live paths (e.g. `/var`) change during scan, so repeated runs vary

Compared to `grep`, this tool may be slower (mostly when run singlethreaded) in some cases because it performs extra required work per match:

- byte offsets
- prefix/suffix extraction
- escaping and strict output formatting
- multithreaded task orchestration overhead

### Benchmark suggestions

- benchmark on a static snapshot (not live-changing directories)
- test warm-cache and cold-cache scenarios separately
- compare equivalent semantics (line count vs occurrence count)
- vary:
  - `NUM_THREADS`
  - `CHUNKING_THRESHOLD`
  - `SMALL_FILE_THRESHOLD`
  - `BATCH_SIZE`

The repository includes `benchmark.sh` for thread/chunk sweeps.

---

## Limitations / Current Trade-offs

- output currently prints basename rather than full path
- matching is byte-oriented (no UTF-aware grapheme semantics)
- `MULTIFIND` returns non-overlapping matches by design
- path traversal currently skips Linux pseudo-filesystem roots `/proc` and `/sys`

---


## Quick Start

```bash
make
./benchmark.sh
./main <filepath> <string>
```

