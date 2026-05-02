# kvdb — Project Plan

A learning project: build an embedded key-value store from scratch in Rust to internalize both the language and database fundamentals.

## Goals

- Learn Rust deeply by writing real systems code that exercises ownership, lifetimes, error handling, and traits.
- Understand database internals at the medium-depth level: B-trees, write-ahead logging, basic ACID guarantees.
- End up with a working embedded KV store that survives crashes and behaves correctly under property-based testing.

## Non-goals

- Not building a networked database. No client/server, no wire protocol.
- Not building a SQL engine. No query planner, no parser, no joins.
- Not optimizing for production performance. Correctness first; benchmarks are diagnostic, not a goal.
- Not implementing MVCC or full concurrency. Single-writer transactions are the ceiling.

## Guiding principles

- **Phase by phase.** Each phase ends with a working binary and passing tests. Tag the commit. If the next phase goes sideways, roll back without losing the foundation.
- **Resist scope creep.** Networking, range queries, multi-database support — all parking-lot until the core works.
- **Safe Rust only.** If `unsafe` feels necessary, stop and reconsider. There's almost always a safe path, and the unsafe path defeats the point of choosing Rust for this.
- **Hand-roll the on-disk format.** The whole point is to know which bytes are where. No `serde`/`bincode` for the storage layer.
- **Explicit over implicit.** No silent flushes in `Drop`, no auto-commit, no hidden I/O. The database does what you tell it to.

---

## Phase 0 — Toolchain and project setup

**Status: complete**

- Codespaces dev container based on `mcr.microsoft.com/devcontainers/rust:1-bookworm`.
- `rust-analyzer`, `CodeLLDB`, `Even Better TOML`, `crates`, `Error Lens` extensions.
- `rust-analyzer.check.command` set to `clippy` for inline strict lints.
- `cargo-watch` and `cargo-nextest` installed via `postCreateCommand`.
- `#![warn(clippy::pedantic)]` and `#![warn(clippy::nursery)]` enabled at crate root.
- `thiserror` for library errors, `anyhow` for tests/binaries.
- `.gitignore` covers `target/`, editor cruft, and future `*.db` / `*.wal` artifacts.

**Exit criteria met:** `cargo test`, `cargo clippy`, `cargo fmt --check` all pass.

---

## Phase 1 — In-memory key-value store

Build the public API and the type system. No disk I/O yet. The goal is to get comfortable with Rust ownership and to lock down the shape of the API before persistence complicates things.

### Scope

- Public surface: `Db::open`, `db.set`, `db.get`, `db.delete`, `db.close` (or rely on `Drop`).
- Backed by `HashMap<Vec<u8>, Vec<u8>>`. Keys and values are bytes, not strings.
- Proper error type via `thiserror`, returned as `Result<T, DbError>` everywhere fallible.
- Single-threaded, single-owner. No `Rc`, no `RefCell`, no `Arc` yet — using them now is cheating.

### What you'll learn

- Owned vs. borrowed types: `Vec<u8>` vs. `&[u8]`, `String` vs. `&str`.
- When to clone and when to borrow.
- Lifetimes in return types.
- Designing an error enum that grows cleanly across phases.

### Exit criteria

- API stable enough that Phase 2 only adds methods, doesn't change signatures.
- Test suite covers: set/get round-trip, get-missing returns `NotFound`, delete-then-get returns `NotFound`, overwrite replaces value.
- Clippy clean at pedantic level.

---

## Phase 2 — Append-only log persistence

Add durability the simple way: every write goes to a single append-only file. On startup, replay the file to rebuild the in-memory index. This is essentially the Bitcask design — Riak shipped on it.

### Scope

- Define a record format. Recommended: `[u32 key_len][u32 value_len][u8 tombstone][key][value][u32 crc32]`. All integers little-endian.
- Hand-serialize with `to_le_bytes()` — no `serde`, no `bincode`.
- Pull `crc32fast` for checksums. Detect torn writes on replay.
- `Db::open(path)` now reads an existing log file (if any) and rebuilds the index.
- All writes append a record before updating the in-memory map.

### What you'll learn

- `std::fs::File`, `BufReader`, `BufWriter`, and the difference between application buffering and OS buffering.
- Binary serialization by hand, including length-prefixing and integrity checks.
- Why `write` + `flush` is not the same as durability (foreshadowing Phase 3).

### Exit criteria

- Set/get/delete persist across `Db::open` cycles.
- Corrupted records (bad CRC) are detected on replay and reported clearly.
- Test suite includes: open empty file, open existing file, replay with a deleted-then-overwritten key, replay with a corrupted trailing record.

---

## Phase 3 — Write-ahead log + checkpointing

Now durability gets real. Separate the write-ahead log from the data file. Add `fsync` and feel the throughput cost. Implement checkpointing so the WAL doesn't grow unboundedly.

### Scope

- Two files: `kvdb.wal` and `kvdb.data`.
- `DurabilityMode` enum: `Sync` (fsync every write), `Async` (no fsync), `Periodic` (fsync every N ms or N writes).
- Use `File::sync_all()` for full fsync; understand when `sync_data()` is sufficient.
- Checkpoint operation:
  1. Flush in-memory state to a temp data file.
  2. `fsync` the temp file.
  3. Atomically rename over `kvdb.data` (`std::fs::rename` is atomic on POSIX).
  4. Truncate the WAL.
  5. `fsync` the directory (often forgotten — needed for the rename to be durable).
- Crash recovery: read the data file, then replay any WAL records on top.

### What you'll learn

- Durability is a spectrum, not a boolean. fsync, fdatasync, directory fsync, rename atomicity.
- Why databases are obsessed with crash-consistent state transitions.
- How to write tests that simulate crashes (drop the `Db` mid-operation, reopen, assert state).

### Exit criteria

- Crash-recovery test suite: kill mid-write, kill mid-checkpoint, kill after rename but before WAL truncate.
- All three durability modes work and have measurably different throughput.
- WAL size stays bounded across many writes if checkpointing runs.

---

## Phase 4 — On-disk B-tree

The big one. Replace the in-memory index with a B-tree (or B+tree) stored in fixed-size pages on disk. This is where the project earns its keep as a learning exercise.

### Scope

- Fixed page size, 4 KiB or 8 KiB. Pages are `[u8; PAGE_SIZE]` buffers.
- Page cache that hands out pages to readers and writers and handles eviction.
- Free-page list for reusing space after deletes.
- B-tree operations: search, insert (with node split), delete (with merge or rebalance).
- No memory mapping. Use explicit `read_at` / `write_at` so I/O is visible and testable.

### Page cache design notes

The hardest design question is page ownership under Rust's borrow checker. Three viable approaches:

- **Owned `Page` values** — clone-on-read, simple, slow. Fine for learning.
- **`Arc<Page>` or `Arc<RwLock<Page>>`** — cache and reader share ownership. Reasonable middle ground.
- **Slot-map / arena** — cache returns indices, callers re-look-up on each access. Closest to how production caches work but has its own complexity.

Recommended starting point: `Arc<RwLock<Page>>`. Not the fastest, but correct, and it teaches what production page caches are working around.

### Testing strategy

- Property-based tests via `proptest`. Generate random sequences of inserts and deletes, assert invariants after each operation:
  - Tree remains sorted.
  - All non-root nodes meet minimum occupancy.
  - No duplicate keys.
  - All keys ever inserted (and not deleted) are findable.
- Plan to throw away your first B-tree implementation. Everyone does. The split logic is famously fiddly.

### What you'll learn

- Why fixed-size pages exist and what they enable.
- How a page cache resolves the tension between memory and disk.
- The actual mechanics of B-tree splits, merges, and rebalancing.
- Why ownership questions in a page cache are interesting (and why C makes you ignore them until production).

### Exit criteria

- B-tree replaces the `HashMap` index. WAL still in place for durability.
- Property tests run thousands of operations without violating invariants.
- File size grows and shrinks reasonably as data is added and deleted.

---

## Phase 5 — Basic transactions

Single-writer transactions: `begin_write`, `commit`, `rollback`. The WAL from Phase 3 becomes the atomicity mechanism.

### Scope

- `Db::begin_write(&mut self) -> WriteTransaction<'_>` — borrow checker enforces single-writer at compile time.
- `WriteTransaction` has its own `set` / `delete` methods that buffer changes.
- `commit` writes all changes to the WAL, fsyncs, then applies in memory.
- `rollback` discards buffered changes.
- `Drop` impl on `WriteTransaction` — design decision: roll back, or panic? Document the choice.

### What you'll learn

- How the borrow checker can encode database semantics for free.
- Atomic commit via WAL: all-or-nothing at the file level.
- Why isolation is a separate problem from atomicity (and why we're not solving isolation here).

### Exit criteria

- A transaction either fully commits or has no visible effect, even across crashes.
- `BEGIN; SET k1; SET k2; ROLLBACK` leaves the database unchanged.
- `BEGIN; SET k1; SET k2; <crash>` leaves the database unchanged.
- `BEGIN; SET k1; SET k2; COMMIT; <crash>` leaves both keys present.

---

## Out of scope (parking lot)

Things that come up naturally and should be deferred until everything above is solid:

- Multiple concurrent readers (would need MVCC or copy-on-write tree).
- Range queries / iteration (B+tree leaf links would help here).
- Compression of pages or values.
- Network protocol / client-server split.
- Multiple databases per process / namespacing.
- Schema, types, or any structure on top of bytes.
- Replication.

---

## References

Priority order for the project as a whole:

1. **Database Internals** — Alex Petrov. The single best book for this exact project.
2. **SQLite source code and architecture docs.** The most readable production database in C; concepts translate directly.
3. **CMU 15-445** — Andy Pavlo's free lecture series on YouTube. Excellent on B-trees and recovery.
4. **Designing Data-Intensive Applications** — Martin Kleppmann. Vocabulary and "why" behind the choices.

Rust-specific:

- **The Rust Programming Language** (free, official) — for Phase 1.
- **Programming Rust** — Blandy/Orendorff. Deeper reference once Phase 1 is comfortable.
- **Rust for Rustaceans** — Gjengset. Lifetimes and traits, right when Phase 4 needs them.
- **redb** ([github.com/cberner/redb](https://github.com/cberner/redb)) — a real embedded KV store in Rust, small enough to read and learn from once past Phase 3.

---

## Crate policy

**Use:**

- `thiserror` — library error types.
- `anyhow` — test and binary error handling.
- `crc32fast` — checksums (no learning value in implementing CRC by hand).
- `proptest` — property-based testing in Phase 4.
- `bytes` — optional, shared byte buffers if needed.

**Skip:**

- `tokio` and any async — no value here, will only confuse the lessons.
- `serde` / `bincode` — defeats the on-disk-format learning.
- `memmap2` — hides the I/O you're trying to learn.

**Build yourself:**

The record format, the page layout, the B-tree, the WAL, the page cache, the recovery logic. These *are* the project.