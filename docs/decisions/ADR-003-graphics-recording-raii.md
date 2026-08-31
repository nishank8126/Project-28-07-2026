# ADR-003 — Use RAII for cache-recording state save/restore

- Status: ACCEPTED
- Date: Piece 2

## Context
Reverse engineering (`ViewContext::CreateCacheElem`, RE E21) shows graphics
creation saves ViewContext state, sets a cache-recording flag, redirects
graphics output to the active recorder, saves ElemDisplayParams, invokes the
producer, then restores ElemDisplayParams and ViewContext state. The original
code uses manual save/restore tied to a ViewContext state byte.

## Observed original technique
Manual save/restore of ViewContext recording flag, active recorder pointer, and
ElemDisplayParams around the graphics producer call.

## Our implementation choice
`GraphicsRecordingScope` — an RAII object constructed around the producer call:
- constructor saves `isRecordingCachedGraphics`, `activeRecorder`,
  `currentDisplayParams` and installs the supplied recorder;
- destructor restores all three unconditionally;
- copy/move deleted (one scope per stack frame).

Exception safety falls out naturally: the destructor runs during stack
unwinding, so state is always restored even if the producer throws.

## Reason
Reproducing manual save/restore is error-prone, especially under exceptions,
and the brief explicitly asked for RAII state restoration. RAII also makes the
"no nested recording session" rule structural (a scope is bound to one frame)
rather than a convention enforced by scattered code.

## Possible future replacement
None anticipated. If a future ViewContext grows more saved state, the scope
simply saves/restores additional fields. If profiling requires avoiding even
the small cost of saving display params when the producer never touches them,
the scope could be made lazy — but correctness is unchanged.
