// Piece 4A unit tests: VERIFIED WORKSTATION MATRIX / AFFINE TRANSFORM CORE.
// Every test is derived directly from confirmed reverse-engineering evidence
// M1-M6 (see docs/reverse_engineering/EvidenceLedger.md). No algorithm is
// tested that is not authorized: no inverse, no normalization, no tolerance
// policy, no range transform.
#include "workstation/math/Point3d.h"
#include "workstation/math/Point4d.h"
#include "workstation/math/Matrix4d.h"
#include "workstation/math/Transform3d.h"

#include "workstation/core/Document.h"
#include "workstation/core/Model.h"
#include "workstation/core/Element.h"
#include "workstation/display/ElementRef.h"
#include "workstation/display/GraphicsCache.h"
#include "workstation/display/CacheKeys.h"
#include "workstation/display/CachedGraphics.h"
#include "workstation/display/ViewContext.h"
#include "workstation/display/GraphicsRecorder.h"
#include "workstation/display/IElementGraphicsProvider.h"
#include "workstation/display/ElementGraphicsService.h"
#include "workstation/display/SubmissionTokens.h"
#include "workstation/display/ElementRenderOverrides.h"
#include "workstation/display/IViewOutput.h"
#include "workstation/display/ViewSubmissionScope.h"
#include "workstation/display/ElementOverrideScope.h"
#include "workstation/display/RetainedGraphicsPresenter.h"
#include "workstation/display/ElementPresentationService.h"

#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <optional>
#include <stdexcept>

using namespace workstation;
using namespace workstation::math;

static int g_pass = 0, g_fail = 0;
static std::vector<std::string> g_failures;

static void run(const std::string& name, std::function<void()> f) {
    try { f(); ++g_pass; std::cout << "[PASS] " << name << "\n"; }
    catch (const std::exception& e) {
        ++g_fail; g_failures.push_back(name + ": " + e.what());
        std::cout << "[FAIL] " << name << ": " << e.what() << "\n";
    }
}
static void check(bool c, const std::string& m) { if (!c) throw std::runtime_error(m); }

// TEST INFRASTRUCTURE ONLY (not a production algorithm): reference product
// implementing the exact M5 equation, used to cross-check Matrix4d::Product.
static Matrix4d refProduct(const Matrix4d& A, const Matrix4d& B) {
    Matrix4d r;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) {
            double s = 0.0;
            for (int k = 0; k < 4; ++k) s += A(i, k) * B(k, j);
            r(i, j) = s;
        }
    return r;
}

// ---- TEST A — Matrix row storage (M4) ----
static void testA_row_storage() {
    Matrix4d m(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            check(m(r, c) == double(r * 4 + c + 1), "row storage mismatch");
}

// ---- TEST B — raw Point4d multiplication (M3) ----
static void testB_point4d_raw() {
    Matrix4d m(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    Point4d p = m.Multiply(Point4d(1, 1, 1, 1));
    check(p == Point4d(10, 26, 42, 58), "Point4d raw multiply");
}

// ---- TEST C — Point4d exact in-place multiplication (M3) ----
static void testC_point4d_inplace() {
    Matrix4d m(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    Point4d p(1, 1, 1, 1);
    m.Multiply(&p, &p, 1);
    check(p == Point4d(10, 26, 42, 58), "Point4d in-place multiply");
}

// ---- TEST D — affine Point3d multiplication (M2) ----
static void testD_affine_point() {
    Matrix4d m(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    Point3d p = m.MultiplyAffine(Point3d(1, 1, 1));
    check(p == Point3d(10, 26, 42), "affine point multiply");
}

// ---- TEST E — affine multiply ignores fourth row (M2) ----
static void testE_affine_ignores_fourth_row() {
    Matrix4d a(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    Matrix4d b(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 100, 200, 300, 400);
    const Point3d s(2, 3, 4);
    check(a.MultiplyAffine(s) == b.MultiplyAffine(s), "affine ignores 4th row");
}

// ---- TEST F — affine exact in-place array (M2) ----
static void testF_affine_inplace_array() {
    Matrix4d m(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    Point3d d[2] = { Point3d(1, 1, 1), Point3d(2, 3, 4) };
    m.MultiplyAffine(d, d, 2);
    check(d[0] == Point3d(10, 26, 42), "affine in-place [0]");
    check(d[1] == Point3d(24, 64, 104), "affine in-place [1]");
}

// ---- TEST G — affine count <= 0 (M2) ----
static void testG_affine_count_le_zero() {
    Matrix4d m(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    Point3d dst[1] = { Point3d(7, 7, 7) };
    Point3d src[1] = { Point3d(1, 1, 1) };
    m.MultiplyAffine(dst, src, 0);
    check(dst[0] == Point3d(7, 7, 7), "affine count=0 untouched");
    m.MultiplyAffine(dst, src, -1);
    check(dst[0] == Point3d(7, 7, 7), "affine count=-1 untouched");
}

// ---- TEST H — renormalization W == 1 (M1) ----
static void testH_renorm_w1() {
    Matrix4d id(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
    Point3d r = id.MultiplyAndRenormalize(Point3d(1, 2, 3)); // W = 1 -> no division
    check(r == Point3d(1, 2, 3), "renorm W==1 no division");
}

// ---- TEST I — renormalization W == 0 (M1) ----
static void testI_renorm_w0() {
    // 4th row all zero -> W = 0 -> XYZ returned as raw numerators (no failure)
    Matrix4d z(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0);
    Point3d r = z.MultiplyAndRenormalize(Point3d(1, 2, 3));
    check(r == Point3d(1, 2, 3), "renorm W==0 returns raw numerators, no failure");
}

// ---- TEST J — renormalization W == 2 (M1) ----
static void testJ_renorm_w2() {
    Matrix4d ren(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 2);
    Point3d r = ren.MultiplyAndRenormalize(Point3d(2, 4, 6)); // W=2 -> /2
    check(r == Point3d(1, 2, 3), "renorm W==2 divides by 2");
}

// ---- TEST K — renormalization W == 0.5 (M1) ----
static void testK_renorm_wfrac() {
    Matrix4d ren(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0.5);
    Point3d r = ren.MultiplyAndRenormalize(Point3d(1, 2, 3)); // W=0.5 -> *2
    check(r == Point3d(2, 4, 6), "renorm W==0.5 divides (no tolerance logic)");
}

// ---- TEST L — renormalized exact in-place array (M1) ----
static void testL_renorm_inplace_array() {
    Matrix4d ren(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 2);
    Point3d d[1] = { Point3d(2, 4, 6) };
    ren.MultiplyAndRenormalize(d, d, 1);
    check(d[0] == Point3d(1, 2, 3), "renorm in-place array");
}

// ---- TEST M — matrix product equation (M5) ----
static void testM_product_equation() {
    Matrix4d A(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    Matrix4d B(1, 0, 0, 0, 0, 2, 0, 0, 0, 0, 3, 0, 0, 0, 0, 4);
    Matrix4d C = Matrix4d::Product(A, B);
    Matrix4d ref = refProduct(A, B);
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            check(C(r, c) == ref(r, c), "product equation mismatch");
}

// ---- TEST N — product transformation order (M5) ----
static void testN_product_order() {
    Matrix4d A(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    Matrix4d B(2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 2);
    Point4d P(1, 2, 3, 4);
    Point4d r1 = Matrix4d::Product(A, B).Multiply(P);
    Point4d r2 = A.Multiply(B.Multiply(P));
    check(r1 == r2, "(A*B)*P == A*(B*P)");
}

// ---- TEST O — product destination aliases left operand (M5) ----
static void testO_product_alias_left() {
    Matrix4d A(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    Matrix4d Aorig = A;
    Matrix4d B(2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 2);
    A.SetProduct(A, B);
    check(A == Matrix4d::Product(Aorig, B), "A.SetProduct(A,B) correct");
}

// ---- TEST P — product destination aliases right operand (M5) ----
static void testP_product_alias_right() {
    Matrix4d A(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    Matrix4d B(2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 2);
    Matrix4d Borig = B;
    B.SetProduct(A, B);
    check(B == Matrix4d::Product(A, Borig), "B.SetProduct(A,B) correct");
}

// ---- TEST Q — Transform3d row mapping (M6) ----
static void testQ_transform_row_mapping() {
    Transform3d t(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12);
    Matrix4d m = t.ToMatrix4d();
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 4; ++c)
            check(m(r, c) == double(r * 4 + c + 1), "transform top rows copied");
    check(m(3, 0) == 0.0 && m(3, 1) == 0.0 && m(3, 2) == 0.0 && m(3, 3) == 1.0,
          "transform bottom row 0 0 0 1");
}

// ---- TEST R — translation column mapping (M6) ----
static void testR_translation_column() {
    Transform3d t(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12);
    Matrix4d m = t.ToMatrix4d();
    check(m(0, 3) == 4.0 && m(1, 3) == 8.0 && m(2, 3) == 12.0,
          "translation column unchanged");
}

// ---- Piece 3 fixtures (for TEST S / TEST T) ----
static Document& sharedDoc() {
    static Document doc(DocumentId(200));
    return doc;
}
static ElementRef makeRef(Element*& outElem) {
    static Model* model = sharedDoc().CreateModel("M");
    Element* e = model->CreateElement("E");
    outElem = e;
    return ElementRef(e->Id(), e);
}
struct FakeProvider : IElementGraphicsProvider {
    int n; mutable int callCount = 0;
    explicit FakeProvider(int c) : n(c) {}
    void EmitGraphics(const Element&, ViewContext&, GraphicsRecorder& r) const override {
        ++callCount;
        for (int i = 0; i < n; ++i) r.EmitCommand(1, (uint64_t)i);
    }
};
struct RecordingViewOutput : IViewOutput {
    enum Kind { Push, Pop, Apply, SubmitA, SubmitB } kind;
    struct Call { Kind kind; std::optional<Transform3d> transform; std::optional<ClipVolumeToken> clip;
                  ElementRenderOverrides overrides; uint64_t graphicsId = 0; double param = 0; };
    std::vector<Call> calls;
    void PushTransformClip(const Transform3d* t, const ClipVolumeToken* c) override {
        Call o; o.kind = Push; if (t) o.transform = *t; if (c) o.clip = *c; calls.push_back(o);
    }
    void PopTransformClip() override { calls.push_back({Pop}); }
    void ApplyElementOverrides(const ElementRenderOverrides& o) override {
        Call c; c.kind = Apply; c.overrides = o; calls.push_back(c);
    }
    void SubmitRetainedPathA(const CachedGraphicsHandle& g) override {
        Call c; c.kind = SubmitA; c.graphicsId = g ? g->GraphicsId() : 0; calls.push_back(c);
    }
    void SubmitRetainedPathB(const CachedGraphicsHandle&, double p) override {
        Call c; c.kind = SubmitB; c.param = p; calls.push_back(c);
    }
    bool hasPushThenPop() const {
        int p = 0; for (auto& c : calls) { if (c.kind == Push) ++p; if (c.kind == Pop) --p; }
        return p == 0;
    }
};
struct ThrowingViewOutput : RecordingViewOutput {
    void SubmitRetainedPathA(const CachedGraphicsHandle&) override { throw std::runtime_error("A"); }
    void SubmitRetainedPathB(const CachedGraphicsHandle&, double) override { throw std::runtime_error("B"); }
};

// ---- TEST S — Piece 3 transform pass-through (M6 + migration) ----
static void testS_piece3_passthrough() {
    Element* e = nullptr; auto ref = makeRef(e);
    ViewContext view; RecordingViewOutput out; view.output = &out;
    ElementGraphicsService svc;
    auto g = svc.Resolve(view, {ref, FakeProvider(1), GraphicsUnsizedKey{}, 10.0, 0.0});
    RetainedGraphicsPresenter pres;
    RetainedSubmissionRequest req; req.graphics = g;
    const Transform3d tx(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12);
    req.transform = tx;
    pres.Present(view, req);
    bool ok = false;
    for (auto& c : out.calls)
        if (c.kind == RecordingViewOutput::Push && c.transform && *c.transform == tx) ok = true;
    check(ok, "Piece 3 observes same Transform3d values unchanged");
}

// ---- TEST T — Piece 3 exception safety with real Transform3d ----
static void testT_piece3_exception_safety() {
    Element* e = nullptr; auto ref = makeRef(e);
    ViewContext view; ThrowingViewOutput out; view.output = &out;
    ElementGraphicsService svc;
    auto g = svc.Resolve(view, {ref, FakeProvider(1), GraphicsUnsizedKey{}, 10.0, 0.0});
    RetainedGraphicsPresenter pres;
    RetainedSubmissionRequest req; req.graphics = g;
    req.transform = Transform3d(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12);
    bool threw = false;
    try { pres.Present(view, req); } catch (...) { threw = true; }
    check(threw, "submit threw");
    check(out.hasPushThenPop(), "transform/clip scope balanced under throw");
}

// ---- Piece 4B: Matrix4d -> Transform3d (M7) ----
// A. m33 == 1, exact affine, perspective terms zero, return true
static void test4B_A_affine_copy() {
    Matrix4d m(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 0, 0, 0, 1);
    Transform3d out;
    bool ok = Transform3d::TryFromMatrix4d(m, out);
    check(ok, "4B-A returns true");
    check(out == Transform3d(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12), "4B-A upper 3x4 copied");
}

// B. m33 == 2, upper divided by exactly 2, return true when sum passes
static void test4B_B_divide_by_two() {
    Matrix4d m(2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 0, 0, 0, 2);
    Transform3d out;
    bool ok = Transform3d::TryFromMatrix4d(m, out);
    check(ok, "4B-B returns true");
    check(out == Transform3d(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12), "4B-B divided by 2");
}

// C. m33 == 0 -> identity output, return false
static void test4B_C_w_zero() {
    Matrix4d m(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 0, 0, 0, 0);
    Transform3d out;
    bool ok = Transform3d::TryFromMatrix4d(m, out);
    check(!ok, "4B-C returns false");
    check(out == Transform3d(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0), "4B-C identity output");
}

// D. perspective terms exactly zero, m33 == 1 -> true (0 < 1e-12)
static void test4B_D_persp_zero() {
    Matrix4d m(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
    Transform3d out;
    bool ok = Transform3d::TryFromMatrix4d(m, out);
    check(ok, "4B-D returns true");
    check(out == Transform3d(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0), "4B-D identity transform");
}

// E. perspective sum below threshold -> true
static void test4B_E_sum_below() {
    Matrix4d m(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1e-13, 1e-13, 1e-13, 1);
    Transform3d out;
    bool ok = Transform3d::TryFromMatrix4d(m, out);
    check(ok, "4B-E returns true (sum < 1e-12)");
    check(out == Transform3d(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0), "4B-E upper copied");
}

// F. perspective sum equal to threshold -> false (strict <)
static void test4B_F_sum_equal() {
    Matrix4d m(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1e-12, 0, 0, 1);
    Transform3d out;
    bool ok = Transform3d::TryFromMatrix4d(m, out);
    check(!ok, "4B-F returns false (sum == threshold, strict <)");
    check(out == Transform3d(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0), "4B-F output populated, not identity");
}

// G. perspective sum above threshold -> false
static void test4B_G_sum_above() {
    Matrix4d m(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 2e-12, 0, 0, 1);
    Transform3d out;
    bool ok = Transform3d::TryFromMatrix4d(m, out);
    check(!ok, "4B-G returns false (sum > threshold)");
}

// H. perspective failure preserves populated upper 3x4 (w == 2 path)
static void test4B_H_failure_preserves_output() {
    Matrix4d m(2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 2e-12, 0, 0, 2);
    Transform3d out;
    bool ok = Transform3d::TryFromMatrix4d(m, out);
    check(!ok, "4B-H returns false");
    // CASE 3 normalized upper by w=2 -> (1,2,3,4,...). NOT identity.
    check(out == Transform3d(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12), "4B-H output populated (normalized)");
}

// I. negative m33: RHS = 1e-12 * negative m33 -> false for nonnegative sum
static void test4B_I_negative_m33() {
    Matrix4d m(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, -1);
    Transform3d out;
    bool ok = Transform3d::TryFromMatrix4d(m, out);
    check(!ok, "4B-I returns false (RHS negative)");
    // CASE 3: q = 1/(-1) = -1 -> upper negated.
    check(out == Transform3d(-1, 0, 0, 0, 0, -1, 0, 0, 0, 0, -1, 0), "4B-I upper negated by w");
}

// J. round trip Transform3d -> Matrix4d -> Transform3d (M6-valid)
static void test4B_J_roundtrip() {
    Transform3d t(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12);
    Matrix4d m = t.ToMatrix4d();           // M6: bottom row 0 0 0 1
    Transform3d back;
    bool ok = Transform3d::TryFromMatrix4d(m, back);
    check(ok, "4B-J returns true");
    check(back == t, "4B-J values preserved");
}

// ---- Piece 4C: affine VECTOR multiply (M8) ----
static void test4C_1_basic() {
    // upper-left 3x3 = [[1,2,3],[4,5,6],[7,8,9]]; translation/fourth row ignored
    Matrix4d m(1, 2, 3, 100, 4, 5, 6, 200, 7, 8, 9, 300, 0, 0, 0, 1);
    Point3d r = m.MultiplyAffineVectors(Point3d(1, 2, 3));
    check(r == Point3d(14, 32, 50), "4C-1 basic 3x3 vector multiply");
}

static void test4C_2_translation_ignored() {
    Matrix4d a(1, 2, 3, 100, 4, 5, 6, 200, 7, 8, 9, 300, 0, 0, 0, 1);
    Matrix4d b(1, 2, 3, 999, 4, 5, 6, 888, 7, 8, 9, 777, 0, 0, 0, 1);
    Point3d v(1, 2, 3);
    check(a.MultiplyAffineVectors(v) == b.MultiplyAffineVectors(v), "4C-2 translation ignored");
}

static void test4C_3_fourth_row_ignored() {
    Matrix4d a(1, 2, 3, 100, 4, 5, 6, 200, 7, 8, 9, 300, 0, 0, 0, 1);
    Matrix4d b(1, 2, 3, 100, 4, 5, 6, 200, 7, 8, 9, 300, 99, 88, 77, 66);
    Point3d v(1, 2, 3);
    check(a.MultiplyAffineVectors(v) == b.MultiplyAffineVectors(v), "4C-3 fourth row ignored");
}

static void test4C_4_inplace() {
    Matrix4d m(1, 2, 3, 100, 4, 5, 6, 200, 7, 8, 9, 300, 0, 0, 0, 1);
    Point3d d[1] = { Point3d(1, 2, 3) };
    m.MultiplyAffineVectors(d, d, 1);
    check(d[0] == Point3d(14, 32, 50), "4C-4 exact in-place array");
}

static void test4C_5_multiple() {
    Matrix4d m(1, 2, 3, 100, 4, 5, 6, 200, 7, 8, 9, 300, 0, 0, 0, 1);
    Point3d src[7];
    Point3d dst[7];
    for (int i = 0; i < 7; ++i) src[i] = Point3d(1, 2, 3);
    m.MultiplyAffineVectors(dst, src, 7);
    for (int i = 0; i < 7; ++i)
        check(dst[i] == Point3d(14, 32, 50), "4C-5 multiple vectors (count>4 tail)");
}

static void test4C_6_count_zero() {
    Matrix4d m(1, 2, 3, 100, 4, 5, 6, 200, 7, 8, 9, 300, 0, 0, 0, 1);
    Point3d dst[1] = { Point3d(7, 7, 7) };
    Point3d src[1] = { Point3d(1, 2, 3) };
    m.MultiplyAffineVectors(dst, src, 0);
    check(dst[0] == Point3d(7, 7, 7), "4C-6 count==0 untouched");
}

static void test4C_7_count_neg() {
    Matrix4d m(1, 2, 3, 100, 4, 5, 6, 200, 7, 8, 9, 300, 0, 0, 0, 1);
    Point3d dst[1] = { Point3d(7, 7, 7) };
    Point3d src[1] = { Point3d(1, 2, 3) };
    m.MultiplyAffineVectors(dst, src, -1);
    check(dst[0] == Point3d(7, 7, 7), "4C-7 count<0 untouched");
}

int main() {
    run("P4A-A  Matrix row storage (M4)",          testA_row_storage);
    run("P4A-B  Point4d raw multiply (M3)",        testB_point4d_raw);
    run("P4A-C  Point4d in-place (M3)",            testC_point4d_inplace);
    run("P4A-D  Affine Point3d (M2)",              testD_affine_point);
    run("P4A-E  Affine ignores 4th row (M2)",       testE_affine_ignores_fourth_row);
    run("P4A-F  Affine in-place array (M2)",       testF_affine_inplace_array);
    run("P4A-G  Affine count<=0 (M2)",             testG_affine_count_le_zero);
    run("P4A-H  Renorm W==1 (M1)",                 testH_renorm_w1);
    run("P4A-I  Renorm W==0 (M1)",                 testI_renorm_w0);
    run("P4A-J  Renorm W==2 (M1)",                 testJ_renorm_w2);
    run("P4A-K  Renorm W==0.5 (M1)",               testK_renorm_wfrac);
    run("P4A-L  Renorm in-place array (M1)",       testL_renorm_inplace_array);
    run("P4A-M  Product equation (M5)",            testM_product_equation);
    run("P4A-N  Product order (M5)",               testN_product_order);
    run("P4A-O  Product alias left (M5)",          testO_product_alias_left);
    run("P4A-P  Product alias right (M5)",         testP_product_alias_right);
    run("P4A-Q  Transform3d row mapping (M6)",     testQ_transform_row_mapping);
    run("P4A-R  Translation column (M6)",         testR_translation_column);
    run("P4A-S  Piece3 transform pass-through",    testS_piece3_passthrough);
    run("P4A-T  Piece3 exception safety",         testT_piece3_exception_safety);

    run("P4B-A  m33==1 affine copy (M7)",         test4B_A_affine_copy);
    run("P4B-B  m33==2 divide by 2 (M7)",        test4B_B_divide_by_two);
    run("P4B-C  m33==0 -> identity/false (M7)",  test4B_C_w_zero);
    run("P4B-D  persp zero, m33==1 true (M7)",   test4B_D_persp_zero);
    run("P4B-E  persp sum below thresh (M7)",    test4B_E_sum_below);
    run("P4B-F  persp sum == thresh false (M7)", test4B_F_sum_equal);
    run("P4B-G  persp sum above thresh false (M7)", test4B_G_sum_above);
    run("P4B-H  failure preserves output (M7)",  test4B_H_failure_preserves_output);
    run("P4B-I  negative m33 (M7)",              test4B_I_negative_m33);
    run("P4B-J  round trip (M6+M7)",             test4B_J_roundtrip);

    run("P4C-1  basic 3x3 vector multiply (M8)", test4C_1_basic);
    run("P4C-2  translation ignored (M8)",       test4C_2_translation_ignored);
    run("P4C-3  fourth row ignored (M8)",        test4C_3_fourth_row_ignored);
    run("P4C-4  exact in-place array (M8)",      test4C_4_inplace);
    run("P4C-5  multiple vectors (M8)",          test4C_5_multiple);
    run("P4C-6  count==0 untouched (M8)",         test4C_6_count_zero);
    run("P4C-7  count<0 untouched (M8)",          test4C_7_count_neg);

    std::cout << "\n========================================\n";
    std::cout << "PASS: " << g_pass << "  FAIL: " << g_fail << "\n";
    if (g_fail > 0) {
        std::cout << "Failures:\n";
        for (const auto& f : g_failures) std::cout << "  " << f << "\n";
        return 1;
    }
    return 0;
}
