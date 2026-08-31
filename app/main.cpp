// WorkstationCAD demo: Piece 1 (cache backbone) + Piece 2 (graphics generation
// + cache recording pipeline).
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
#include "workstation/display/RetainedGraphicsPresenter.h"
#include "workstation/display/ElementPresentationService.h"
#include "workstation/math/Transform3d.h"

#include <iostream>

using namespace workstation;

// ---- Piece 2 demo providers ----
struct DemoProvider : IElementGraphicsProvider {
    int n;
    mutable int callCount = 0;
    explicit DemoProvider(int c) : n(c) {}
    void EmitGraphics(const Element&, ViewContext&, GraphicsRecorder& r) const override {
        ++callCount;
        for (int i = 0; i < n; ++i) r.EmitCommand(1, (uint64_t)(100 + i));
    }
};

struct DemoAssemblyProvider : IElementGraphicsProvider {
    ElementGraphicsService& svc;
    ElementRef& childRef;
    const IElementGraphicsProvider& childProvider;
    GraphicsUnsizedKey childKey;
    double childMetric, childFactor;
    DemoAssemblyProvider(ElementGraphicsService& s, ElementRef& cr,
                         const IElementGraphicsProvider& cp,
                         GraphicsUnsizedKey ck, double cm, double cf)
        : svc(s), childRef(cr), childProvider(cp),
          childKey(std::move(ck)), childMetric(cm), childFactor(cf) {}
    void EmitGraphics(const Element&, ViewContext& v, GraphicsRecorder& r) const override {
        r.EmitCommand(1, 11);                  // parent command 1
        GraphicsResolveRequest childReq{childRef, childProvider, childKey,
                                        childMetric, childFactor};
        svc.Resolve(v, childReq);              // recording mode -> into r
        r.EmitCommand(1, 12);                  // parent command 2
    }
};

int main() {
    std::cout << "=== WorkstationCAD Demo (Piece 1 + Piece 2) ===\n\n";

    Document doc(DocumentId(1));
    Model* model = doc.CreateModel("Default");
    Element* elem = model->CreateElement("SampleElement");

    // =========================================================
    // Piece 1: direct + specialized cache
    // =========================================================
    ElementRef ref(elem->Id(), elem);
    std::cout << "--- Piece 1: element cache ---\n";
    auto directG = CreateCachedGraphics(elem->Id(), elem->GeometryRevision(), 12);
    GraphicsUnsizedKey directKey; directKey.variant = 0;
    SaveCachedGraphics(ref, directKey, 0.0, 0.0, directG);
    auto hitDirect = FindCachedGraphics(ref, directKey, 0.0);
    std::cout << "[Direct] lookup -> " << (hitDirect ? "HIT" : "MISS") << "\n";

    GraphicsUnsizedKey specKey; specKey.transformKey = 1;
    auto specG = CreateCachedGraphics(elem->Id(), elem->GeometryRevision(), 200);
    SaveCachedGraphics(ref, specKey, 10.0, 2.0, specG);
    std::cout << "[Specialized] range [" << (10.0/2.0) << "," << (10.0*2.0) << "]\n";
    for (double m : {7.0, 15.0, 25.0})
        std::cout << "    metric " << m << " -> "
                  << (FindCachedGraphics(ref, specKey, m) ? "HIT" : "MISS") << "\n";

    // =========================================================
    // Piece 2: graphics generation + cache recording pipeline
    // =========================================================
    std::cout << "\n--- Piece 2: graphics generation pipeline ---\n";
    ElementGraphicsService svc;
    ViewContext view;

    Element* e2 = model->CreateElement("Piece2Element");
    ElementRef ref2(e2->Id(), e2);

    DemoProvider prov(4);
    GraphicsUnsizedKey pkey; pkey.transformKey = 1;
    GraphicsResolveRequest req{ref2, prov, pkey, 10.0, 2.0};

    auto g1 = svc.Resolve(view, req);
    std::cout << "Resolve element " << e2->Id().Value() << ":\n";
    std::cout << "    cache miss\n";
    std::cout << "    recording graphics\n";
    std::cout << "    commands recorded: "
              << (g1 ? g1->PrimitiveCount() : 0) << "\n";

    auto g2 = svc.Resolve(view, req);
    std::cout << "Resolve element " << e2->Id().Value() << " again:\n";
    std::cout << "    cache hit (provider not invoked, calls="
              << prov.callCount << ")\n";

    GraphicsResolveRequest reqValid{ref2, prov, pkey, 7.0, 2.0};
    auto g3 = svc.Resolve(view, reqValid);
    std::cout << "Resolve at another valid metric (7):\n";
    std::cout << "    cache hit\n";

    GraphicsResolveRequest reqOutside{ref2, prov, pkey, 25.0, 2.0};
    auto g4 = svc.Resolve(view, reqOutside);
    std::cout << "Resolve outside metric interval (25):\n";
    std::cout << "    cache miss\n";
    std::cout << "    new retained representation created (commands="
              << (g4 ? g4->PrimitiveCount() : 0) << ")\n";

    // ---- nested recording ----
    std::cout << "\n--- Piece 2: nested recording ---\n";
    Element* childE = model->CreateElement("ChildElement");
    ElementRef childRef(childE->Id(), childE);
    DemoProvider childProv(3);
    GraphicsUnsizedKey childKey; childKey.transformKey = 2;
    DemoAssemblyProvider asmProv(svc, childRef, childProv, childKey, 10.0, 2.0);
    GraphicsUnsizedKey asmKey; asmKey.transformKey = 5;  // distinct from piece2 pkey
    GraphicsResolveRequest asmReq{ref2, asmProv, asmKey, 10.0, 2.0};
    auto ag = svc.Resolve(view, asmReq);
    std::cout << "Assembly provider:\n";
    std::cout << "    emits command, resolves child while recording,\n";
    std::cout << "    child emits commands into same recorder, emits command\n";
    std::cout << "    resulting parent command count: "
              << (ag ? ag->PrimitiveCount() : 0) << " (expected 5)\n";

    std::cout << "\nService stats: hits=" << svc.Stats().cacheHits
              << " misses=" << svc.Stats().cacheMisses
              << " builds=" << svc.Stats().graphicsBuilds
              << " nestedEmits=" << svc.Stats().nestedDirectEmits << "\n";

    // =========================================================
    // Piece 3: retained graphics view-output submission layer
    // =========================================================
    std::cout << "\n--- Piece 3: retained submission ---\n";
    struct DemoViewOutput : IViewOutput {
        void PushTransformClip(const math::Transform3d* t, const ClipVolumeToken* c) override {
            std::cout << "    [submit] PushTransformClip";
            if (t) std::cout << " transform(t03=" << t->operator()(0, 3)
                             << ", t13=" << t->operator()(1, 3)
                             << ", t23=" << t->operator()(2, 3) << ")";
            if (c) std::cout << " clip=" << c->value;
            std::cout << "\n";
        }
        void PopTransformClip() override { std::cout << "    [submit] PopTransformClip\n"; }
        void ApplyElementOverrides(const ElementRenderOverrides& o) override {
            std::cout << "    [submit] ApplyOverrides";
            if (o.lineColor) std::cout << " lineColor=" << *o.lineColor;
            if (o.auxiliaryValue) std::cout << " aux=" << *o.auxiliaryValue;
            std::cout << "\n";
        }
        void SubmitRetainedPathA(const CachedGraphicsHandle&) override {
            std::cout << "    [submit] PathA\n";
        }
        void SubmitRetainedPathB(const CachedGraphicsHandle&, double p) override {
            std::cout << "    [submit] PathB (param=" << p << ")\n";
        }
    } dout;

    ViewContext pview;
    pview.output = &dout;
    pview.retainedSubmissionParameter = 0.5;

    ElementGraphicsService psvc;
    RetainedGraphicsPresenter presenter;
    ElementPresentationService presSvc(psvc, presenter);

    Element* pe = model->CreateElement("Piece3Element");
    ElementRef pref(pe->Id(), pe);

    // persistent element: first resolve = miss, present PathA
    DemoProvider pprov(4);
    GraphicsUnsizedKey presKey; presKey.transformKey = 1;
    GraphicsResolveRequest presReq{pref, pprov, presKey, 10.0, 2.0};
    std::cout << "[resolve] element " << pe->Id().Value() << " -> MISS\n";
    presSvc.Draw(pview, {presReq});
    // second resolve = hit, present again
    std::cout << "[resolve] element " << pe->Id().Value() << " -> HIT (no regeneration)\n";
    presSvc.Draw(pview, {presReq});
    std::cout << "    persistent cache entry present: "
              << (FindCachedGraphics(pref, presKey, 10.0) != nullptr ? "yes" : "no") << "\n";

    // PathB uses retainedSubmissionParameter
    std::cout << "[resolve+submit] PathB (param=" << pview.retainedSubmissionParameter << ")\n";
    presSvc.Draw(pview, {presReq, std::nullopt, std::nullopt,
                         RetainedSubmissionPath::PathB});

    // transform/clip scope + temporary overrides
    std::cout << "[resolve+submit] with transform/clip + temporary overrides\n";
    presReq.allowPersistentCache = true;
    PresentationDrawRequest oreq{presReq, math::Transform3d(1, 0, 0, 1, 0, 1, 0, 2, 0, 0, 1, 3), ClipVolumeToken{222},
                                 RetainedSubmissionPath::PathA, true, false};
    presSvc.Draw(pview, oreq);

    // transient element: build, present once, prove no cache entry
    std::cout << "[resolve] transient element -> BUILD\n";
    DemoProvider tprov(4);
    GraphicsUnsizedKey tkey; tkey.transformKey = 3;
    GraphicsResolveRequest treq{pref, tprov, tkey, 10.0, 2.0};
    treq.allowPersistentCache = false;
    PresentationDrawRequest tdraw{treq};
    presSvc.Draw(pview, tdraw);
    std::cout << "[cache] transient entry absent: "
              << (FindCachedGraphics(pref, tkey, 10.0) == nullptr ? "yes" : "no") << "\n";

    std::cout << "\nPiece 3 presenter stats: submissions="
              << presenter.Stats().submissions
              << " pathA=" << presenter.Stats().pathASubmissions
              << " pathB=" << presenter.Stats().pathBSubmissions
              << " transient=" << presenter.Stats().transientSubmissions << "\n";

    // =========================================================
    // Piece 4A: verified workstation matrix / affine transform core (M1-M6)
    // =========================================================
    std::cout << "\n--- Piece 4A: verified matrix/affine core ---\n";
    {
        // M4: explicit row values
        math::Matrix4d m(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        std::cout << "Matrix4d row values; m(0,0)=" << m(0,0)
                  << " m(3,3)=" << m(3,3) << "\n";

        // M3: raw Point4d multiply (1,1,1,1) -> (10,26,42,58)
        math::Point4d p4 = m.Multiply(math::Point4d(1, 1, 1, 1));
        std::cout << "Point4d raw multiply = (" << p4.x << "," << p4.y
                  << "," << p4.z << "," << p4.w << ")\n";

        // M2: affine Point3d multiply (1,1,1) implicit w=1 -> (10,26,42)
        math::Point3d pa = m.MultiplyAffine(math::Point3d(1, 1, 1));
        std::cout << "Point3d affine multiply = (" << pa.x << "," << pa.y
                  << "," << pa.z << ")\n";

        // M1: renormalize, W == 2 path: identity with m33=2, point (2,4,6) -> (1,2,3)
        math::Matrix4d ren(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 2);
        math::Point3d pr = ren.MultiplyAndRenormalize(math::Point3d(2, 4, 6));
        std::cout << "Point3d renormalized (W=2) = (" << pr.x << "," << pr.y
                  << "," << pr.z << ")\n";

        // M5: product A*B (B scales by 2) => 2*A
        math::Matrix4d b(2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 2);
        math::Matrix4d ab = math::Matrix4d::Product(m, b);
        std::cout << "Matrix product A*B: ab(0,0)=" << ab(0,0)
                  << " ab(3,3)=" << ab(3,3) << "\n";

        // M6: Transform3d 3x4 -> Matrix4d with final row 0 0 0 1
        math::Transform3d tx(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12);
        math::Matrix4d tm = tx.ToMatrix4d();
        std::cout << "Transform3d->Matrix4d bottom row = ("
                  << tm(3,0) << "," << tm(3,1) << "," << tm(3,2) << "," << tm(3,3) << ")\n";

        // M7: Matrix4d -> Transform3d conversion (m33=2, upper divided by 2)
        math::Matrix4d hm(2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 0, 0, 0, 2);
        math::Transform3d tx2;
        bool convOk = math::Transform3d::TryFromMatrix4d(hm, tx2);
        std::cout << "Matrix4d->Transform3d (M7): ok=" << (convOk ? "true" : "false")
                  << " t(0,3)=" << tx2(0,3) << " t(2,3)=" << tx2(2,3) << "\n";

        // M8: affine VECTOR multiply (linear 3x3 only; translation/fourth row ignored)
        math::Matrix4d vmat(1, 2, 3, 100, 4, 5, 6, 200, 7, 8, 9, 300, 0, 0, 0, 1);
        math::Point3d vout = vmat.MultiplyAffineVectors(math::Point3d(1, 2, 3));
        std::cout << "AffineVectors (M8): (" << vout.x << "," << vout.y << "," << vout.z << ")\n";
    }

    std::cout << "\nDemo complete.\n";
    return 0;
}
