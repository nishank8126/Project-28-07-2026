# POD DataSource & Stream Adapter — RE & Design (Piece 5)

Clean-room DataSource abstraction with file-backed implementation and 64 KiB
stream adapter. CONFIRMED behaviour from Ghidra RE evidence is separated from
DESIGN DECISION; no proprietary class names, ABI, offsets, or memory layouts
reproduced.

## CONFIRMED REVERSE ENGINEERING

### A. DataSource state machine (FUN_18007B940)

Observed state fields:
- readable flag
- writable flag
- mode/state

Confirmed modes:
- 0 = closed/reset
- 1 = read
- 2 = write
- 3 = read/write

Clean-room: `enum class DataSourceMode { Closed=0, Read=1, Write=2, ReadWrite=3 }`.

### B. Open operations

**OpenRead:** open file, readable=true, writable=false, mode=Read.
**Create/OpenWrite:** create/open file, writable=true, readable=false, mode=Write.
**OpenReadWrite:** open in read/write mode. If fails and fallback flag enabled, attempt creation.

### C. Stream operations (+0xA0, +0xA8, +0xE8, +0xF0)

- **+0xA0** (strongly inferred): fixed-size read operation (reads 4 bytes, 1 byte into internal storage).
- **+0xA8** (strongly inferred): variable-size byte transfer/read.
- **+0xE8**: UNKNOWN semantics.
- **+0xF0**: UNKNOWN semantics.

### D. FUN_18007B940 stream processing

1. Obtain source/stream object.
2. Validate source state.
3. Read 4 bytes using source interface.
4. Read 1 byte.
5. Validate two header byte values ≥ 4.
6. Initialize decoder/state.
7. Obtain source-derived value through virtual operation.
8. Allocate 64 KiB working buffer (0x10000 bytes).
9. Transfer pending data when required.
10. On complete transfer: accumulatedBytes += transferredBytes, clear buffer, reset offset.
11. Call structured serializer (Piece 4 decode path).
12. Transfer remaining bytes.
13. Emit final 8-byte accumulated size.
14. Perform another stream operation.
15. Free temporary 64 KiB buffer.

### E. 256 KB stream buffer (Piece 3)

Already implemented in `PointCloudStreamReader`. Refill algorithm:
- Preserve unread bytes
- Move unread tail
- Request more data
- Grow/replace buffer if required

This is distinct from the 64 KiB working buffer above.

## DESIGN DECISIONS

- `PodDataSource`: abstract interface with `read`, `readSome`, `seek`, `tell`, `canRead`, `canWrite`, `mode`.
- `FileDataSource`: `std::FILE*`-backed implementation with move semantics.
- `PodStreamAdapter`: 64 KiB working buffer wrapping a `PodDataSource`.
- `fixedRead(n)`: fills working buffer with exactly `n` bytes (calls source if needed).
- `transfer(maxBytes)`: returns available bytes up to `maxBytes` from working buffer.
- `consume(bytes)`: marks bytes as consumed, advances offset.
- Clean separation between 64 KiB working buffer (Piece 5) and 256 KiB reader (Piece 3).

## UNKNOWN (not implemented)

- +0xE8 semantics
- +0xF0 semantics
- Proprietary POD container format
- POD compression / LAZ / LASzip
- Exact handler table processing beyond marker=1

## VALIDATION

`wk_pod_datasource_tests` (TEST 21–34) passes. Full CTest suite (11/11) green.
Pieces 1–4 unaffected.
