# ADR-001 — Use std::vector for the specialized variant cache

- Status: ACCEPTED
- Date: Piece 1

## Context
Reverse engineering (finding H) shows the specialized per-element graphics
cache is a linked list of ~0x38-byte nodes sorted ascending by
`minViewMetric`. Insertion is performed with a linear scan to find the first
node whose `minViewMetric >= new.minViewMetric` and inserts before it. No
duplicate-key merge occurs.

## Observed original technique
Intrusive singly-linked list with a free-list / HeapZone allocator
(finding I). Optimized for cache-local insertion and low allocation overhead.

## Our implementation choice
`ElementGraphicsVariantSet` stores `std::vector<GraphicsVariant>` and inserts
with `std::lower_bound` on `minViewMetric`, preserving ascending order. No
custom allocator.

## Reason
Piece 1 must be correct, readable, and testable. A vector gives O(log n)
insertion position and O(n) lookup, which is more than adequate for the
element-level cache where the number of variants per element is small. This
matches the brief's explicit guidance ("std::vector with lower_bound is
acceptable. Do not prematurely reproduce the proprietary intrusive
allocator.").

## Possible future replacement
When profiling shows insertion/allocation pressure, replace the backing store
with an arena/slab allocator behind the same `Insert`/`Find`/`Clear` interface
(enabled by RE I, which is a confirmed-original technique we DEFERRED). The
public `ElementGraphicsVariantSet` API will not change.
