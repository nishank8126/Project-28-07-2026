# POD Handler Registry — RE & Design (Piece 5)

Red-black tree replacement for the Piece 4 sorted-vector handler registry.
CONFIRMED behaviour from Ghidra RE evidence is separated from DESIGN DECISION;
no proprietary class names, ABI, offsets, or memory layouts reproduced.

## CONFIRMED REVERSE ENGINEERING

### A. PodBlockReadHandler — abstract handler interface (FUN_180077BD0)

RTTI proves `PodBlockReadHandler` is a polymorphic abstract base class.
Vtable: `[0]` destructor/cleanup, `[1]` purecall (concrete handler operation).

Clean-room: `PodBlockHandler` with pure virtual `process(PodDecodeContext&)`.

### B. Key comparison (FUN_18000B940)

Byte-key ordering:
1. Compare common prefix via `memcmp(a, b, min(a.size(), b.size()))`.
2. If common bytes differ: result follows `memcmp` ordering.
3. If common bytes equal: shorter key sorts before longer key.

No locale, no case-fold, no normalization.

### C. Tree insertion (FUN_18000C3B0)

Observed behavior:
- Increment node count
- Handle empty-tree insertion
- Use sentinel node
- Connect new node to parent
- Update left/right tree boundaries
- Maintain parent pointers
- Maintain color/state field
- Perform rotations
- Perform recoloring
- Continue until balancing conditions satisfied

Consistent with red-black tree insertion algorithm.

### D. Lower-bound search (FUN_180077400)

```
current = root
result = NIL
while current != NIL:
    if current.key >= search key:
        result = current
        current = current.left
    else:
        current = current.right
return result
```

### E. DoubleBoundingBlockHandler (FUN_1800780C0)

Reads:
1. 32-bit marker (confirmed: valid marker = 1)
2. 32-bit record count
3. Per record:
   - 32-bit ordering key
   - 6× double-precision bounding-box values
   - 6× uint64 metadata values

Produces `BoundingBoxRecord` entries.

## DESIGN DECISIONS

- `HandlerKey`: independent `std::vector<uint8_t>` wrapper with `KeyLess` static method.
- `HandlerTree`: sentinel-based red-black tree with explicit `NodeColor` enum.
- Sentinel node is black with no payload; used as NIL marker.
- Duplicate keys are allowed (inserted to the right of existing equal key).
- `PodHandlerRegistry`: thin wrapper around `HandlerTree`; preserves the Piece 4 public API (`insert`, `findLowerBound`, `size`).
- `PodDecodeContext`: clean-room context carrying reader + cloud + metadata map.
- `DoubleBoundingBlockHandler`: concrete handler producing `BoundingBoxRecord` entries.
- Handler dispatch integrated into `PodBlockDecoder::processHandlerTable`.

## UNKNOWN (not implemented)

- Full handler dispatch semantics (marker validation beyond marker=1)
- Other concrete handler types beyond `DoubleBoundingBlockHandler`
- Record semantics beyond bounding-box construction
- Post-load metadata stages (FUN_1800774c0 / FUN_180078620 / FUN_180077c80)

## VALIDATION

`wk_pod_handler_tests` (TEST 1–20) passes. Full CTest suite (11/11) green.
Pieces 1–4 unaffected.
