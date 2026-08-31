# Evidence Ledger

This ledger records every reverse-engineering finding provided so far, with
explicit separation of CONFIRMED facts, INFERENCES, and UNKNOWN fields. Each
entry lists the original symbol, observed behavior, confirmed facts,
inferences, unknowns, and our equivalent implementation with status.

Legend:
  CONFIRMED  - stated as confirmed in the brief / clearly observed
  INFERENCE  - reasonable interpretation, not proven
  UNKNOWN    - not yet reversed; we use an abstraction or TODO

---

## E1 — High-level object architecture (finding A)
- Original: DgnPlatform file/document, model, model reference, element,
  element reference, element handle, view context, viewport, display style,
  element display parameters, graphics processing, faceting, retained QVision
  elements.
- Confirmed: A mature CAD workstation separates
  Application -> Document -> Model -> Element/ElementRef and separately
  ViewContext -> graphics generation/cache -> retained scene -> renderer.
- Inference: The renderer must not absorb model/cache responsibilities.
- Our equivalent: `Document` owns `Model`s; `Model` owns `Element`s;
  `ElementRef` (display layer) owns the retained-graphics cache.
- Status: IMPLEMENTED (backbone, no renderer yet).

## E2 — Element-level retained graphics cache (finding B)
- Original: `ViewContext::GetCachedGeometry`, `CreateCacheElem`,
  `GetQvCacheElem`, `SaveQvCacheElem`, `GetUnsizedKey`.
- Confirmed: Per-element retained graphics cache. On need: cache hit returns
  retained graphics; cache miss invokes a graphics generator, records output,
  creates a retained object, saves it.
- Inference: Cache is element-keyed, not frame-keyed.
- Our equivalent: `ElementRef` holds `CachedGraphics` (direct + specialized).
- Status: IMPLEMENTED (lookup/save, no generator yet — Piece 2).

## E3 — Direct fast cache path (finding C)
- Original: `ElementRef::GetCachedGraphics(variant)`,
  `ElementRef::SetCachedGraphics(variant)`.
- Confirmed: Ordinary/simple graphics use a cheap per-element cache indexed by
  a small variant integer; no hash-table lookup required per ordinary element.
- Inference: `N` direct slots is an implementation decision.
- Our equivalent: `ElementRef::m_direct` (`std::vector<CachedGraphicsHandle>`,
  `kMaxDirectVariants = 64`). Indexed by `variant` when key is "simple".
- Status: IMPLEMENTED. ADR-001.

## E4 — Specialized per-element variant cache (finding D)
- Original: node ~0x38 bytes with style/display-handler key, display-filter
  key, local-transform key, cache variant, min/max view metric, retained
  graphics pointer, next entry (linked list).
- Confirmed: Specialized cache keyed by style key, filter key, transform key,
  variant; each entry carries a view-metric validity range and retained
  graphics.
- Inference: Layout/intrusive list are proprietary; behavior is what matters.
- Our equivalent: `GraphicsUnsizedKey` + `GraphicsVariant`
  (`minViewMetric`, `maxViewMetric`, `CachedGraphicsHandle`), stored in
  `ElementGraphicsVariantSet` (std::vector).
- Status: IMPLEMENTED.

## E5 — QvUnsizedKey (finding E)
- Confirmed: Logical unsized key contains display-style handler/cache key,
  display-filter key, local transform key, representation/cache variant. Size
  / view metric is stored separately.
- Our equivalent: `GraphicsUnsizedKey` (no metric). Metric range lives in
  `GraphicsVariant`.
- Status: IMPLEMENTED.

## E6 — View-metric validity range (finding F)
- Original: `SaveQvCacheElem` computes `minMetric = M / F`, `maxMetric = M * F`
  when F != 0; zero F -> very broad interval.
- Confirmed: Small camera/zoom changes inside [min,max] are cache hits with no
  retessellation. Leaving the range triggers a more appropriate representation.
- Our equivalent: `SaveCachedGraphics` computes the same; for F <= 0 uses
  `-infinity / +infinity` (semantically unlimited, not proprietary sentinels).
- Status: IMPLEMENTED (TEST 4/5/6).

## E7 — Specialized cache lookup (finding G)
- Confirmed: Lookup checks transform key, variant, style cache key, metric in
  [min,max], display filter matches current view/context. All must succeed.
- Our equivalent: `ElementGraphicsVariantSet::Find` (and `FindCachedGraphics`).
- Status: IMPLEMENTED (TEST 5/6/7/8/9/10).

## E8 — Specialized cache insertion (finding H)
- Original: `FUN_1802908e0` does NOT update duplicate keys; inserts new node
  before first existing node whose `minViewMetric >= new.minMetric`
  (ascending order).
- Confirmed: Multiple cached representations for same element/similar state
  with different/overlapping ranges are allowed (no merge).
- Our equivalent: `ElementGraphicsVariantSet::Insert` uses `std::lower_bound`
  on `minViewMetric`, ascending.
- Status: IMPLEMENTED (TEST 3/11).

## E9 — Specialized node memory pool (finding I)
- Confirmed: Original recycles nodes via HeapZone/free-list allocator.
- Inference: Performance technique, not behavior we must copy now.
- Our equivalent: `std::vector` / `std::unique_ptr`; allocation ownership
  allows a future slab/arena allocator without API change.
- Status: DEFERRED (documented; ADR pending on allocator).

## E10 — Reference-counted cache keys (finding J)
- Confirmed: Recovered code AddRefs/Releases style/filter key objects.
- Our equivalent: `std::shared_ptr` for `CachedGraphicsHandle`; keys are
  immutable value types. No proprietary ref-count ABI.
- Status: IMPLEMENTED (ADR-002).

## E11 — Cache recording / graphics generation (finding K)
- Original: `CreateCacheElem` saves ViewContext state, enters recording mode,
  begins recorder, invokes `IStrokeForCache`/producer, finishes recorder,
  obtains retained graphics, restores state. Guard prevents recursive lookup
  during recording.
- Confirmed: There is a recording scope, a graphics producer interface, and a
  recursive-lookup guard.
- Our equivalent (planned Piece 2): `IElementGraphicsProvider`,
  `GraphicsRecorder`, `GraphicsRecordingScope` (RAII).
- Status: NOT IMPLEMENTED (Piece 2).

## E12 — Element display parameters (finding L)
- Confirmed: ViewContext saves/restores `ElemDisplayParams` during cache
  creation; element rendering may temporarily change symbology/material/line
  params without contaminating the next element.
- Our equivalent (planned): scoped/RAII display parameter state in ViewContext.
- Status: NOT IMPLEMENTED.

## E13 — Display style cache key (finding M)
- Confirmed: Active display-style handler can produce a specialized cache key;
  a style should not be one giant integer invalidating all graphics. Handler
  decides whether geometry actually differs.
- Our equivalent (planned): `IDisplayStyleGeometryKeyProvider`.
- Status: PARTIAL — `DisplayStyleCacheKey` exists as opaque comparable value;
  provider interface deferred.

## E14 — Local transform key (finding N)
- Confirmed: A local transform key enters the specialized cache key in certain
  display-style-dependent cases. Exact algorithm NOT reversed.
- Our equivalent: `uint32_t transformKey` (our own transform-revision hash),
  clearly documented as OUR DESIGN.
- Status: IMPLEMENTED placeholder (documented as OUR DESIGN).

## E15 — QVision retained display architecture (finding O)
- Confirmed: Persistent cache objects; per view: activeDisplayList,
  mainDisplayList, alternateDisplayList. Alternate-list selection can
  clone/rebind retained records; NOT a trivial O(1) pointer swap, but NO full
  model retessellation.
- Our equivalent (planned Piece 6): geometry cache vs retained draw records vs
  active representation vs renderer state.
- Status: NOT IMPLEMENTED.

## E16 — Display mode changes (finding P)
- Confirmed: Smooth/Wireframe mode functions update compact mode flags and
  derived renderer state; do NOT traverse model, rebuild meshes, retessellate,
  recreate vertex buffers, or recompile shaders.
- Permanent rule: mode change must not trigger full geometry regeneration.
- Status: ARCHITECTURE RULE RECORDED (enforced later).

## E17 — GPU buffer reuse — do not overclaim (finding Q)
- Confirmed: Architecture points to retained geometry reuse. Exact GPU
  buffer-sharing semantics NOT proven.
- Our equivalent (planned): renderer reuses persistent buffers; documented as
  OUR intended design, not confirmed Bentley detail.
- Status: NOT IMPLEMENTED.

## E18 — QVision shader architecture (finding R)
- Confirmed: G-buffer-like geometry pass + deferred lighting/merge pass;
  resources for color/albedo, type/object info, normals, materials, depth,
  shadows, environment/reflections, aux buffers.
- Inference: A deferred CAD pipeline is plausible. Exact BRDF NOT proven
  (NOT GGX/Schlick).
- Our equivalent (planned Piece 10): modular shading model, deferred pipeline.
- Status: NOT IMPLEMENTED.

## E19 — Model / graphics separation (finding S)
- Confirmed: Model owns CAD meaning; element graphics cache owns retained
  display representation; renderer owns GPU/display execution. Core must not
  depend on Direct3D.
- Our equivalent: `CachedGraphics` holds no D3D objects; one library with
  include-discipline (display depends on core, not vice versa).
- Status: IMPLEMENTED (architectural constraint enforced in Piece 1).

---

## E20 — GetCachedGeometry overall flow (finding 1, Piece 2)
- Original: `ViewContext::GetCachedGeometry`.
- Confirmed: On needing graphics, first attempts retained-element cache
  lookup (`GetQvCacheElem`). HIT -> return immediately (no regeneration).
  MISS -> `CreateCacheElem` (generate + save).
- Our equivalent: `ElementGraphicsService::Resolve` (NORMAL MODE).
- Status: IMPLEMENTED.

## E21 — CreateCacheElem recording sequence (finding 2, Piece 2)
- Original: `ViewContext::CreateCacheElem`.
- Confirmed: Enters temporary cache-recording mode: chooses recorder,
  `BeginElement`, saves ViewContext state, sets cache-recording flag,
  redirects graphics output to active recorder, saves ElemDisplayParams,
  invokes producer, `EndElement`, restores ElemDisplayParams, restores
  ViewContext state.
- Inference: This is exactly an RAII recording scope. Our `GraphicsRecordingScope`
  models it.
- Our equivalent: `GraphicsRecordingScope` + `GraphicsRecorder::BeginElement`/
  `EndElement` inside `ElementGraphicsService::Resolve`.
- Status: IMPLEMENTED.

## E22 — IStrokeForCache role (finding 4, Piece 2)
- Confirmed: Element graphics are produced through an interface equivalent in
  role to `IStrokeForCache`, reached via a virtual call during cache creation.
- Unknown: Exact proprietary virtual method names / auxiliary virtuals.
- Our equivalent: `IElementGraphicsProvider::EmitGraphics` (only the role
  modeled; no unknown vtable methods reproduced).
- Status: IMPLEMENTED (role only).

## E23 — Recursive cache-recording guard (finding 3, Piece 2)
- Confirmed: A ViewContext state byte controls a branch. When already
  creating cached graphics, `GetCachedGeometry` does NOT perform another
  retained-cache lookup; it invokes the stroker directly and returns no new
  standalone cached element from the nested call.
- Our equivalent: `ViewContext::isRecordingCachedGraphics`; in RECORDING MODE
  `ElementGraphicsService::Resolve` bypasses cache + recorder creation and
  emits directly into `view.activeRecorder`, returning nullptr.
- Status: IMPLEMENTED (TEST 7/8).

## E24 — ElemDisplayParams save/restore (finding 5, Piece 2)
- Confirmed: ElemDisplayParams are copied before generation and restored
  after, so one element's temporary display/symbology changes do not affect
  subsequent elements.
- Unknown: Exact proprietary ElemDisplayParams layout.
- Our equivalent: `ElemDisplayParams { symbologyRevision, materialRevision }`
  (OUR temporary fields) saved/restored by `GraphicsRecordingScope`.
- Status: IMPLEMENTED (TEST 9/10).

## E25 — Display-style interceptor around ViewContext +0x7f0 (finding 6)
- Unknown: Exact semantics not proven. We left a clean TODO only; no behavior
  implemented. Deferred to a later piece when more evidence exists.

## E26 — ViewContext::_DrawCached (finding, Piece 3)
- Confirmed: _DrawCached calls GetCachedGeometry; if no retained graphics, no
  submission; if retained graphics exist, it derives a boolean submission
  selector from element state and invokes virtual _DrawQvElem(retainedGraphics,
  selector). Graphics may remain persistently retained OR be temporary for the
  current draw and then released.
- Unknown: exact element bits controlling the selector.
- Our equivalent: `ElementGraphicsService` resolves; `RetainedGraphicsPresenter`
  submits; persistence/transience handled by `GraphicsRetention`
  (allowPersistentCache option) instead of proprietary element bits.

## E27 — ViewContext::_DrawQvElem (finding, Piece 3)
- Confirmed: obtains a ViewOutput-like object; a boolean selector chooses
  between two output submission paths (PathA / PathB). One path receives an
  additional ViewContext-derived scalar.
- Unknown: semantic meaning of PathA vs PathB, and of the extra scalar. We use
  neutral names and a neutral `retainedSubmissionParameter`.

## E28 — ViewContext::DrawQvElem (finding, Piece 3)
- Confirmed: performs `output.PushTransClip(transform, clip)`, copies current
  override/symbology state, optionally augments temporary symbology, activates
  temporary overrides on output if non-empty, calls _DrawQvElem, restores
  original override state, then `output.PopTransClip()`.
- Inference: this manual push/apply/submit/restore/pop is exactly what our RAII
  `ViewSubmissionScope` + `ElementOverrideScope` model.
- Our equivalent: `RetainedGraphicsPresenter::Present`.

## E29 — IViewOutput / ElemMatSymb / OvrMatSymb (finding, Piece 3)
- Confirmed: export surface shows `IViewOutput::PushTransClip`,
  `IViewOutput::PopTransClip`, and structures corresponding to `ElemMatSymb`,
  `OvrMatSymb`, `ViewContext::GetOverrideMatSymb`. Transform/clip state and
  element/material symbology override state belong in the submission layer, not
  in CachedGraphics.
- Our equivalent: `IViewOutput`, `ElementRenderOverrides` (typed optional
  fields), `ViewContext::currentRenderOverrides`. No proprietary bitmask layout
  (0x01..0x80000000) and no OvrMatSymb field layout reproduced.

## E30 — Matrix3d inversion / determinant (Piece 4)
- Original: (unknown — no recovered routine). `RotMatrix` exists, but no inverse
  or determinant code was provided.
- Confirmed: `RotMatrix` is a type; rotation-by-axis is a confirmed primitive
  (E: rotation factories kept). Inversion/determinant are NOT confirmed.
- Our equivalent: NOT IMPLEMENTED. `Matrix3d::Determinant`, `Matrix3d::TryInverse`,
  and `Transform3d::TryInverse` were written during Piece 4 exploration but then
  REMOVED under the absolute reverse-engineering algorithm rule (no confirmed
  workstation inverse behavior). They may only be added once the workstation
  inverse routine is recovered.
- Status: RE_PENDING (not implemented).

## E31 — Vector3d normalization (Piece 4)
- Original: (unknown — `DVec3d` exists; `length`/`distance` implied by confirmed
  `DPoint3d::distance`, but `normalize` itself is not a confirmed operation).
- Confirmed: distance/length (via confirmed `DPoint3d::distance`).
- Our equivalent: `Vector3d::Length`/`LengthSquared` (used by the confirmed
  distance op) are kept. `Normalized`, `TryNormalize`, `IsZero` were written then
  REMOVED under the absolute rule (vector normalization is an unconfirmed
  algorithm).
- Status: RE_PENDING (not implemented).

## E32 — Range3d transformation (Piece 4)
- Original: `ScanRangeToDRange3d(...)` (range conversion confirmed to exist), but
  the exact algorithm for transforming a range by a Transform3d (e.g. 8-corner
  enclosure vs other method) is NOT confirmed.
- Confirmed: `DRange3d` type; range conversion exists.
- Our equivalent: `Range3d::Extend`/`Contains`/`Intersects`/`Center`/`Diagonal`/
  `Volume` (basic, type-intrinsic) are kept. `Range3d::Transform` (8-corner
  enclosure) was written then REMOVED under the absolute rule (range
  transformation is an unconfirmed algorithm).
- Status: RE_PENDING (not implemented).

## E33 — Transform3d extraction from Matrix4d (Piece 4)
- Original: (unknown). `Transform` exists; `GetLocalToView`/`GetViewToLocal`
  confirm a transform pipeline, but the exact "is this affine / extract linear +
  translation" routine is NOT confirmed.
- Our equivalent: `Transform3d` (linear + translation) and `ToMatrix4d` (the
  confirmed direction: Transform3d -> Matrix4d) are kept. `FromMatrix4d` was
  written then REMOVED under the absolute rule.
- Status: RE_PENDING (not implemented). Confirmed: affine point/vector transform
  and transform composition (Transform3d::operator*) are retained.

## E34 — Tolerance / comparison policy (Piece 4)
- Original: (unknown). No recovered comparison routine or tolerance semantics.
- Our equivalent: `MathConstants.h` keeps `kDefaultAbsTolerance` ONLY as the
  degenerate-W guard for the confirmed `Matrix4d::TryTransformPointProjective`
  (multiplyAndRenormalize). The tolerance COMPARISON POLICY functions
  `NearlyZero`/`NearlyEqual` were written then REMOVED under the absolute rule
  (tolerance policy is an unconfirmed algorithm).
- Status: RE_PENDING (not implemented as a policy).

## E35 — View-metric (world-units-per-view-unit) (Piece 4)
- Original: (unknown). The `GetCachedGeometry` path implies a view metric is used
  for cache validity, but the EXACT metric definition is NOT reversed.
- Our equivalent: `EstimateWorldUnitsPerViewUnit` (inspired by confirmed data
  flow) was written then REMOVED under the absolute rule. `ViewMetric.h` now holds
  only the `ViewMetricAxis` enum placeholder.
- Status: RE_PENDING (not implemented).

## M1 — DMatrix4d::multiplyAndRenormalize (address 0x3d6af0)
- Target function: `DMatrix4d::multiplyAndRenormalize` (0x3d6af0).
- Inputs: `Point3d* destination`, `Point3d const* source`, `int count`. Point3d =
  {x@+0, y@+8, z@+16}, stride 0x18.
- Outputs: `destination` written with {X,Y,Z}; no return, no status.
- Equations (per point, implicit w = 1):
  - X = m00*x + m01*y + m02*z + m03
  - Y = m10*x + m11*y + m12*z + m13
  - Z = m20*x + m21*y + m22*z + m23
  - W = m30*x + m31*y + m32*z + m33
- Branch behavior (EXACT):
  - if (W != 1.0 && W != 0.0) { q = 1.0/W; X*=q; Y*=q; Z*=q; }
  - otherwise NO division (W==0 leaves raw numerators; W==1 no division).
- NO epsilon, NO near-zero rejection, NO failure status.
- Alias behavior: exact in-place (destination==source) supported because x/y/z
  are read before output is written. Groups-of-four is binary optimization.
- Confidence: CONFIRMED.
- Clean mapping: `Matrix4d::MultiplyAndRenormalize(Point3d*,Point3d const*,int)`
  and thin `Point3d MultiplyAndRenormalize(Point3d const&) const`.
- Tests: P4A-B? (no) — P4A-H/I/J/K/L, demo.

## M2 — DMatrix4d::multiplyAffine (address 0x3d6ac0)
- Target function: `DMatrix4d::multiplyAffine` (0x3d6ac0).
- Inputs: `Point3d* destination`, `Point3d const* source`, `int count`.
- Outputs: `destination` = affine transform of each source point.
- Equations (per point, implicit w = 1, first three rows only):
  - X = m00*x + m01*y + m02*z + m03
  - Y = m10*x + m11*y + m12*z + m13
  - Z = m20*x + m21*y + m22*z + m23
- Branch behavior: NO fourth-row calculation, NO W, NO normalization, NO
  validation that the matrix is affine. Fourth row completely ignored.
- count <= 0 is a no-op. Exact source/destination alias supported.
- Confidence: CONFIRMED.
- Clean mapping: `Matrix4d::MultiplyAffine(Point3d*,Point3d const*,int)` and thin
  `Point3d MultiplyAffine(Point3d const&) const`.
- Tests: P4A-D/E/F/G, demo.

## M3 — DMatrix4d::Multiply(Point4d&, Point4d const&) (bsiDMatrix4d_multiplyMatrixPoint, 0x3e3210)
- Target function: `DMatrix4d::Multiply(Point4d&, Point4d const&)`
  (bsiDMatrix4d_multiplyMatrixPoint, 0x3e3210).
- Point4d = {x@+0, y@+8, z@+0x10, w@+0x18}; four doubles.
- Output: P' = M * P with mathematical COLUMN vectors.
  - X = m00*x + m01*y + m02*z + m03*w
  - Y = m10*x + m11*y + m12*z + m13*w
  - Z = m20*x + m21*y + m22*z + m23*w
  - W = m30*x + m31*y + m32*z + m33*w
- Alias behavior: exact in-place (input==output) because all four inputs loaded
  before output written.
- Confidence: CONFIRMED.
- Clean mapping: `Matrix4d::Multiply(Point4d const&) const` and
  `Matrix4d::Multiply(Point4d*,Point4d const*,int)`.
- Tests: P4A-B/C, demo.

## M4 — DMatrix4d::InitFromRowValues (bsiDMatrix4d_initFromRowValues, 0x3e2300)
- Target function: `DMatrix4d::InitFromRowValues` (bsiDMatrix4d_initFromRowValues, 0x3e2300).
- The 16 parameters map exactly to m00..m33 in row-major order (m00@+0x00 ...
  m33@+0x78). Matrix4d is |m00 m01 m02 m03| / |m10 m11 m12 m13| / |m20 m21 m22
  m23| / |m30 m31 m32 m33|, rows contiguous.
- Our clean implementation uses `double m_[4][4]` with row/column accessors.
- Confidence: CONFIRMED.
- Clean mapping: `Matrix4d` 16-value constructor + `double operator()(r,c)`.
- Tests: P4A-A, P4A-M/N, demo.

## M5 — DMatrix4d::InitProduct(A,B) (address 0x3e28b0)
- Target function: `DMatrix4d::InitProduct(A,B)` (0x3e28b0).
- Equation: C[i][j] = A[i][0]*B[0][j] + A[i][1]*B[1][j] + A[i][2]*B[2][j] +
  A[i][3]*B[3][j]. Therefore C = A * B.
- Since P' = M * P, composition order is (A*B)*P = A*(B*P): the RIGHT-HAND
  matrix acts on the point first.
- Alias behavior: computed into temporary storage before writing destination, so
  `A.SetProduct(A,B)` and `B.SetProduct(A,B)` both work.
- Confidence: CONFIRMED.
- Clean mapping: `Matrix4d::SetProduct(A,B)` / `static Matrix4d::Product(A,B)`.
- Tests: P4A-M/N/O/P.

## M6 — bsiDMatrix4d_initFromTransform (address 0x429e30)
- Target function: `bsiDMatrix4d_initFromTransform` (0x429e30).
- Transform = 12 doubles in 3x4 arrangement:
  +0x00 t00 +0x08 t01 +0x10 t02 +0x18 t03 / +0x20 t10 +0x28 t11 +0x30 t12
  +0x38 t13 / +0x40 t20 +0x48 t21 +0x50 t22 +0x58 t23.
- Conversion to Matrix4d EXACTLY:
  | t00 t01 t02 t03 |
  | t10 t11 t12 t13 |
  | t20 t21 t22 t23 |
  |  0   0   0   1  |
  Copies all 12 transform doubles unchanged, then writes m30=0,m31=0,m32=0,m33=1.
- No transpose, no scaling, no normalization, no validation, no tolerance, no
  failure.
- Confidence: CONFIRMED.
- Clean mapping: `Transform3d` 3x4 value type + `Transform3d::ToMatrix4d()`.
- Tests: P4A-Q/R, P4A-S (Piece 3 pass-through), demo.

## M7 — bsiTransform_initFromDMatrix4d / bsiTransform_initFromHMatrix (0x45a9b0 / 0x45acf0)
- Target functions: `Bentley::bsiTransform_initFromDMatrix4d` (0x45a9b0) and
  `Bentley::bsiTransform_initFromHMatrix` (0x45acf0) — same algorithm.
- Input: `Matrix4d` (4x4). Output: `Transform3d` (3x4 affine).
- Tolerance constant: `1.0e-12`, recovered from `DAT_54695610`
  (bits `0x3D719799812DEA11`).
- Algorithm: `w = m33`.
  - CASE 1 (`w == 0.0`): output = identity affine Transform3d
    `|1 0 0 0|/|0 1 0 0|/|0 0 1 0|`; return false; no division.
  - CASE 2 (`w == 1.0`): copy upper 3x4 exactly (tij = mij); then eval success.
  - CASE 3 (`w != 0 && w != 1`): `q = 1.0/w`; `tij = mij * q`; then eval success.
- Success condition (EXACT, strict `<`):
  `abs(m30) + abs(m31) + abs(m32) < 1.0e-12 * m33`.
  - Uses the SUM of all three bottom-row terms.
  - Uses the ORIGINAL `m33` (not normalized).
  - `m33` on RHS, not `abs(m33)`; strict `<`, not `<=`.
  - No NearlyZero / NearlyEqual / numeric_limits epsilon.
- Failure semantics (two distinct false cases):
  1. `m33 == 0` -> identity output, false.
  2. `m33 != 0` but perspective sum fails -> output REMAINS the copied/normalized
     upper 3x4; NOT reset to identity.
- Negative `m33`: RHS `1.0e-12 * negative` is negative, left side nonnegative, so
  the test normally fails (preserve this behavior).
- Confidence: CONFIRMED.
- Clean mapping: `Transform3d::TryFromMatrix4d(const Matrix4d&, Transform3d&)`.
  No production tolerance helper; `1.0e-12` used inline exactly as recovered.
- Tests: P4B-A..J (affine copy, divide-by-2, w==0 identity, persp zero, sum
  below/equal/above threshold, failure preserves output, negative m33, round trip).

## M8 — bsi/ Bentley::DMatrix4d::multiplyAffineVectors (0x3d6ae0)
- Target function: `Bentley::DMatrix4d::multiplyAffineVectors` (0x3d6ae0).
- Logical signature:
  `void multiplyAffineVectors(Point3d* destination, Point3d const* source, int count) const`.
- Point storage: `x +0x00`, `y +0x08`, `z +0x10`, stride `0x18`.
- Per input (x,y,z): `X = m00*x + m01*y + m02*z`;
  `Y = m10*x + m11*y + m12*z`; `Z = m20*x + m21*y + m22*z`.
- Translation slots (`m03`,`m13`,`m23`) are NOT read.
- Fourth row (`m30`,`m31`,`m32`,`m33`) is NOT read.
- No translation, no W, no homogeneous normalization, no division, no tolerance,
  no failure result, no matrix validation.
- `count <= 0` performs no work.
- `destination == source` (exact alias) supported: x/y/z are read before write.
  Arbitrary partially-overlapping ranges are NOT documented as supported.
- The binary unrolls four vectors at a time for `count >= 4`; treated as
  compiled optimization only — clean source uses a simple loop.
- Confidence: CONFIRMED.
- Clean mapping: `Matrix4d::MultiplyAffineVectors(Point3d& dst[], const Point3d& src[], int count) const`
  plus single-value thin wrapper `Point3d MultiplyAffineVectors(const Point3d&) const`.
  Point3d retained as the vector carrier (no Vector3d introduced for this piece).
- Tests: P4C-1 (basic 3x3), P4C-2 (translation ignored), P4C-3 (fourth row ignored),
  P4C-4 (in-place), P4C-5 (multiple/tail), P4C-6 (count==0), P4C-7 (count<0).

## Open / unresolved questions
- Exact cache destruction / element deletion invalidation policy (deferred).
- Exact transform-key computation (E14).
- Exact geometry-invalidation granularity (we use revision counter).
- Exact GPU resource ownership & buffer sharing (E17).
- Exact BRDF / shading math (E18).
- Exact display-list resource sharing semantics (E15).
- Exact proprietary IStrokeForCache auxiliary virtual methods (E22).
- Exact proprietary ElemDisplayParams layout (E24).
- Exact display-style interceptor behavior at ViewContext +0x7f0 (E25).
- Exact proprietary empty-element cache behavior (our policy: no commands ->
  no cache entry).
- Semantic meaning of the two retained submission paths (PathA vs PathB, E27).
- Semantic meaning of the extra ViewContext scalar passed to PathB (E27).
- Semantic meanings of DrawQvElem param_4 / param_5 (E28).
- Meanings of proprietary material/symbology mask bits 0x01..0x80000000 (E29).
- Exact proprietary ElemMatSymb / OvrMatSymb layout (E29).
- Exact rules selecting persistent vs transient QvElem (OUR policy:
  allowPersistentCache request option, E26).
