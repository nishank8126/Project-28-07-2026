# ADR-006 — RAII for submission state

- Status: ACCEPTED
- Date: Piece 3

## Context
Reverse engineering (`DrawQvElem`, RE E28) shows a manual sequence: push
transform/clip, copy override state, optionally augment temporary symbology,
activate temporary overrides on the output, submit retained graphics, restore
original override state, pop transform/clip. The original manages this with
explicit save/restore code.

## Observed original technique
Manual push/apply/submit/restore/pop around each retained draw.

## Our implementation choice
Two RAII scopes:
- `ViewSubmissionScope` — wraps `IViewOutput::PushTransformClip` in the
  constructor and `PopTransformClip` in the destructor.
- `ElementOverrideScope` — captures the original `ViewContext` overrides; if
  the temporary overrides differ, applies them; on destruction restores the
  original overrides on both the ViewContext and the output.

Both have copy/move deleted, so they are bound to exactly one stack frame.

## Reason
Manual save/restore is error-prone, especially when the submit can throw; RAII
guarantees the output and ViewContext are left balanced (no leaked transform/
clip push, original overrides restored) even on exceptions. This mirrors the
rationale in ADR-003 for the cache-recording scope.

## Possible future replacement
None anticipated. If a future ViewContext grows more saved state around
submission, the scopes simply save/restore additional fields.
