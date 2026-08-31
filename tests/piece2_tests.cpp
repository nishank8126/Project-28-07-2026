// Piece 2 unit tests: element graphics generation + cache recording pipeline.
#include "workstation/core/Document.h"
#include "workstation/core/Model.h"
#include "workstation/core/Element.h"
#include "workstation/display/ElementRef.h"
#include "workstation/display/GraphicsCache.h"
#include "workstation/display/CacheKeys.h"
#include "workstation/display/CachedGraphics.h"
#include "workstation/display/ViewContext.h"
#include "workstation/display/GraphicsRecorder.h"
#include "workstation/display/GraphicsRecordingScope.h"
#include "workstation/display/IElementGraphicsProvider.h"
#include "workstation/display/ElementGraphicsService.h"

#include <iostream>
#include <string>
#include <functional>
#include <vector>
#include <stdexcept>
#include <cassert>

static int g_pass = 0;
static int g_fail = 0;
static std::vector<std::string> g_failures;

static void run(const std::string& name, std::function<void()> f) {
    try { f(); ++g_pass; std::cout << "[PASS] " << name << "\n"; }
    catch (const std::exception& e) {
        ++g_fail; g_failures.push_back(name + ": " + e.what());
        std::cout << "[FAIL] " << name << ": " << e.what() << "\n";
    }
}
static void check(bool c, const std::string& m) { if (!c) throw std::runtime_error(m); }

// ---- shared test fixtures ----
static workstation::Document& sharedDoc() {
    static workstation::Document doc(workstation::DocumentId(100));
    return doc;
}
static workstation::ElementRef makeRef(workstation::Element*& outElem) {
    static workstation::Model* model = sharedDoc().CreateModel("M");
    workstation::Element* e = model->CreateElement("E");
    outElem = e;
    return workstation::ElementRef(e->Id(), e);
}

// ---- providers ----
struct FakeProvider : workstation::IElementGraphicsProvider {
    int commandsPerEmit;
    mutable int callCount = 0;
    explicit FakeProvider(int n) : commandsPerEmit(n) {}
    void EmitGraphics(const workstation::Element&, workstation::ViewContext&,
                     workstation::GraphicsRecorder& r) const override {
        ++callCount;
        for (int i = 0; i < commandsPerEmit; ++i) r.EmitCommand(1, (uint64_t)i);
    }
};

struct EmptyProvider : workstation::IElementGraphicsProvider {
    mutable int callCount = 0;
    void EmitGraphics(const workstation::Element&, workstation::ViewContext&,
                      workstation::GraphicsRecorder&) const override { ++callCount; }
};

struct ParamMutatingProvider : workstation::IElementGraphicsProvider {
    mutable int callCount = 0;
    void EmitGraphics(const workstation::Element&, workstation::ViewContext& v,
                      workstation::GraphicsRecorder& r) const override {
        ++callCount;
        v.currentDisplayParams.symbologyRevision = 999;
        v.currentDisplayParams.materialRevision = 888;
        r.EmitCommand(1, 1);
    }
};

struct ThrowingProvider : workstation::IElementGraphicsProvider {
    mutable int callCount = 0;
    void EmitGraphics(const workstation::Element&, workstation::ViewContext&,
                      workstation::GraphicsRecorder& r) const override {
        ++callCount;
        r.EmitCommand(1, 1);
        throw std::runtime_error("provider failure");
    }
};

// Assembly-like provider: emits a command, then resolves a child WHILE the
// recording session is active, then emits another command.
struct AssemblyProvider : workstation::IElementGraphicsProvider {
    workstation::ElementGraphicsService& svc;
    workstation::ElementRef& childRef;
    const workstation::IElementGraphicsProvider& childProvider;
    workstation::GraphicsUnsizedKey childKey;
    double childMetric;
    double childFactor;
    mutable int callCount = 0;
    AssemblyProvider(workstation::ElementGraphicsService& s,
                     workstation::ElementRef& cr,
                     const workstation::IElementGraphicsProvider& cp,
                     workstation::GraphicsUnsizedKey ck,
                     double cm, double cf)
        : svc(s), childRef(cr), childProvider(cp),
          childKey(std::move(ck)), childMetric(cm), childFactor(cf) {}
    void EmitGraphics(const workstation::Element&, workstation::ViewContext& v,
                      workstation::GraphicsRecorder& r) const override {
        ++callCount;
        r.EmitCommand(1, 11);                  // parent command 1
        workstation::GraphicsResolveRequest childReq{childRef, childProvider,
                                                     childKey, childMetric, childFactor};
        svc.Resolve(v, childReq);              // recording mode -> emits into r
        r.EmitCommand(1, 12);                  // parent command 2
    }
};

// ----- TEST 1 -----
static void test1_miss_invokes_provider_once() {
    workstation::Element* e = nullptr; auto ref = makeRef(e);
    workstation::ViewContext view; workstation::ElementGraphicsService svc;
    FakeProvider prov(3);
    workstation::GraphicsUnsizedKey key; key.transformKey = 1;
    workstation::GraphicsResolveRequest req{ref, prov, key, 10.0, 2.0};
    auto g = svc.Resolve(view, req);
    check(g != nullptr, "expected graphics");
    check(prov.callCount == 1, "provider must run exactly once on miss");
}

// ----- TEST 2 -----
static void test2_command_count_recorded() {
    workstation::Element* e = nullptr; auto ref = makeRef(e);
    workstation::ViewContext view; workstation::ElementGraphicsService svc;
    FakeProvider prov(4);
    workstation::GraphicsUnsizedKey key; key.transformKey = 1;
    workstation::GraphicsResolveRequest req{ref, prov, key, 10.0, 2.0};
    auto g = svc.Resolve(view, req);
    check(g != nullptr, "expected graphics");
    check(g->PrimitiveCount() == 4, "command count should be 4");
}

// ----- TEST 3 -----
static void test3_second_resolve_is_hit() {
    workstation::Element* e = nullptr; auto ref = makeRef(e);
    workstation::ViewContext view; workstation::ElementGraphicsService svc;
    FakeProvider prov(4);
    workstation::GraphicsUnsizedKey key; key.transformKey = 1;
    workstation::GraphicsResolveRequest req{ref, prov, key, 10.0, 2.0};
    auto g1 = svc.Resolve(view, req);
    auto g2 = svc.Resolve(view, req);
    check(g1 != nullptr && g2 != nullptr, "both resolves produce graphics");
    check(prov.callCount == 1, "provider must NOT run again on identical hit");
    check(svc.Stats().cacheHits >= 1, "a cache hit should be counted");
    check(svc.Stats().cacheMisses == 1, "exactly one miss");
}

// ----- TEST 4 -----
static void test4_different_key_rebuilds() {
    workstation::Element* e = nullptr; auto ref = makeRef(e);
    workstation::ViewContext view; workstation::ElementGraphicsService svc;
    FakeProvider prov(4);
    workstation::GraphicsUnsizedKey k1; k1.transformKey = 1;
    workstation::GraphicsUnsizedKey k2; k2.transformKey = 2;
    workstation::GraphicsResolveRequest r1{ref, prov, k1, 10.0, 2.0};
    workstation::GraphicsResolveRequest r2{ref, prov, k2, 10.0, 2.0};
    svc.Resolve(view, r1);
    svc.Resolve(view, r2);
    check(prov.callCount == 2, "different specialized key must rebuild");
}

// ----- TEST 5 -----
static void test5_metric_outside_rebuilds() {
    workstation::Element* e = nullptr; auto ref = makeRef(e);
    workstation::ViewContext view; workstation::ElementGraphicsService svc;
    FakeProvider prov(4);
    workstation::GraphicsUnsizedKey key; key.transformKey = 1;
    workstation::GraphicsResolveRequest rIn{ref, prov, key, 10.0, 2.0};  // [5,20]
    workstation::GraphicsResolveRequest rOut{ref, prov, key, 25.0, 2.0}; // outside
    svc.Resolve(view, rIn);
    svc.Resolve(view, rOut);
    check(prov.callCount == 2, "metric outside interval must rebuild");
}

// ----- TEST 6 -----
static void test6_metric_inside_reuses() {
    workstation::Element* e = nullptr; auto ref = makeRef(e);
    workstation::ViewContext view; workstation::ElementGraphicsService svc;
    FakeProvider prov(4);
    workstation::GraphicsUnsizedKey key; key.transformKey = 1;
    workstation::GraphicsResolveRequest rIn{ref, prov, key, 10.0, 2.0}; // [5,20]
    workstation::GraphicsResolveRequest rIn2{ref, prov, key, 7.0, 2.0}; // inside
    svc.Resolve(view, rIn);
    svc.Resolve(view, rIn2);
    check(prov.callCount == 1, "metric inside interval must reuse");
}

// ----- TEST 7 -----
static void test7_nested_bypasses_cache_lookup() {
    workstation::Element* e = nullptr; auto ref = makeRef(e);
    workstation::ViewContext view; workstation::ElementGraphicsService svc;

    workstation::GraphicsRecorder rec;
    rec.BeginElement(e->Id(), e->GeometryRevision());
    {
        workstation::GraphicsRecordingScope scope(view, rec);
        FakeProvider child(3);
        workstation::GraphicsUnsizedKey key; key.transformKey = 1;
        workstation::GraphicsResolveRequest req{ref, child, key, 10.0, 2.0};
        auto r = svc.Resolve(view, req);
        check(r == nullptr, "nested resolve returns nullptr");
        check(child.callCount == 1, "nested provider invoked");
        check(svc.Stats().cacheHits == 0 && svc.Stats().cacheMisses == 0,
              "no cache lookup during recording mode");
        check(svc.Stats().nestedDirectEmits == 1, "nested emit counted");
    }
    auto g = rec.EndElement();
    check(g && g->PrimitiveCount() == 3, "nested commands recorded into parent recorder");
}

// ----- TEST 8 -----
static void test8_nested_commands_in_parent_recorder() {
    workstation::Element* parent = nullptr; auto pref = makeRef(parent);
    workstation::Element* child = nullptr;  auto cref = makeRef(child);
    workstation::ViewContext view; workstation::ElementGraphicsService svc;

    FakeProvider childProv(3);
    workstation::GraphicsUnsizedKey childKey; childKey.transformKey = 2;
    AssemblyProvider asmProv(svc, cref, childProv, childKey, 10.0, 2.0);

    workstation::GraphicsUnsizedKey parentKey; parentKey.transformKey = 1;
    workstation::GraphicsResolveRequest req{pref, asmProv, parentKey, 10.0, 2.0};
    auto g = svc.Resolve(view, req);

    check(g != nullptr, "parent graphics produced");
    check(g->PrimitiveCount() == 5, "parent(2)+child(3) commands = 5");
    check(asmProv.callCount == 1, "assembly provider ran once");
    check(childProv.callCount == 1, "child provider ran once (during recording)");
    check(svc.Stats().nestedDirectEmits == 1, "one nested direct emit recorded");
    // child must NOT be independently cached from that recording session
    check(workstation::FindCachedGraphics(cref, childKey, 10.0) == nullptr,
          "child not cached during parent recording");
}

// ----- TEST 9 -----
static void test9_scope_restores_on_success() {
    workstation::Element* e = nullptr; auto ref = makeRef(e);
    workstation::ViewContext view;
    view.currentDisplayParams = workstation::ElemDisplayParams{5, 7};
    workstation::ElementGraphicsService svc;
    ParamMutatingProvider prov;
    workstation::GraphicsUnsizedKey key; key.transformKey = 1;
    workstation::GraphicsResolveRequest req{ref, prov, key, 10.0, 2.0};
    svc.Resolve(view, req);
    check(view.isRecordingCachedGraphics == false, "recording flag restored");
    check(view.activeRecorder == nullptr, "active recorder restored");
    check(view.currentDisplayParams.symbologyRevision == 5, "symbology restored");
    check(view.currentDisplayParams.materialRevision == 7, "material restored");
}

// ----- TEST 10 -----
static void test10_scope_restores_on_exception() {
    workstation::Element* e = nullptr; auto ref = makeRef(e);
    workstation::ViewContext view;
    view.currentDisplayParams = workstation::ElemDisplayParams{5, 7};
    workstation::ElementGraphicsService svc;
    ThrowingProvider prov;
    workstation::GraphicsUnsizedKey key; key.transformKey = 1;
    workstation::GraphicsResolveRequest req{ref, prov, key, 10.0, 2.0};
    bool threw = false;
    try { svc.Resolve(view, req); }
    catch (const std::exception&) { threw = true; }
    check(threw, "expected provider exception to propagate");
    check(view.isRecordingCachedGraphics == false, "recording flag restored after throw");
    check(view.activeRecorder == nullptr, "active recorder restored after throw");
    check(view.currentDisplayParams.symbologyRevision == 5, "symbology restored after throw");
    check(view.currentDisplayParams.materialRevision == 7, "material restored after throw");
}

// ----- TEST 11 -----
static void test11_throwing_provider_no_partial_save() {
    workstation::Element* e = nullptr; auto ref = makeRef(e);
    workstation::ViewContext view; workstation::ElementGraphicsService svc;
    ThrowingProvider prov;
    workstation::GraphicsUnsizedKey key; key.transformKey = 1;
    workstation::GraphicsResolveRequest req{ref, prov, key, 10.0, 2.0};
    try { svc.Resolve(view, req); } catch (const std::exception&) {}
    check(workstation::FindCachedGraphics(ref, key, 10.0) == nullptr,
          "partial graphics must not be saved after throw");
}

// ----- TEST 12 -----
static void test12_empty_provider_no_cache_entry() {
    workstation::Element* e = nullptr; auto ref = makeRef(e);
    workstation::ViewContext view; workstation::ElementGraphicsService svc;
    EmptyProvider prov;
    workstation::GraphicsUnsizedKey key; key.transformKey = 1;
    workstation::GraphicsResolveRequest req{ref, prov, key, 10.0, 2.0};
    auto g = svc.Resolve(view, req);
    check(g == nullptr, "empty provider yields nullptr (our Piece 2 policy)");
    check(workstation::FindCachedGraphics(ref, key, 10.0) == nullptr,
          "no usable cache entry saved for empty element");
}

// ----- TEST 13 -----
static void test13_geometry_revision_invalidates() {
    workstation::Element* e = nullptr; auto ref = makeRef(e);
    workstation::ViewContext view; workstation::ElementGraphicsService svc;
    FakeProvider prov(4);
    workstation::GraphicsUnsizedKey key; key.transformKey = 1;
    workstation::GraphicsResolveRequest req{ref, prov, key, 10.0, 2.0};
    auto g1 = svc.Resolve(view, req);
    check(g1 != nullptr, "first resolve builds");
    e->TouchGeometry();   // geometry changed -> stale revision
    auto g2 = svc.Resolve(view, req);
    check(g2 != nullptr, "rebuild after revision change");
    check(prov.callCount == 2, "provider re-run after geometry revision change");
}

int main() {
    run("TEST 1  miss invokes provider once",      test1_miss_invokes_provider_once);
    run("TEST 2  command count recorded",          test2_command_count_recorded);
    run("TEST 3  second resolve is hit",           test3_second_resolve_is_hit);
    run("TEST 4  different key rebuilds",          test4_different_key_rebuilds);
    run("TEST 5  metric outside rebuilds",         test5_metric_outside_rebuilds);
    run("TEST 6  metric inside reuses",            test6_metric_inside_reuses);
    run("TEST 7  nested bypasses cache lookup",    test7_nested_bypasses_cache_lookup);
    run("TEST 8  nested commands in parent",       test8_nested_commands_in_parent_recorder);
    run("TEST 9  scope restores on success",       test9_scope_restores_on_success);
    run("TEST 10 scope restores on exception",     test10_scope_restores_on_exception);
    run("TEST 11 throwing no partial save",        test11_throwing_provider_no_partial_save);
    run("TEST 12 empty provider no cache",         test12_empty_provider_no_cache_entry);
    run("TEST 13 geometry revision invalidates",   test13_geometry_revision_invalidates);

    std::cout << "\n========================================\n";
    std::cout << "PASS: " << g_pass << "  FAIL: " << g_fail << "\n";
    if (g_fail > 0) {
        std::cout << "Failures:\n";
        for (const auto& f : g_failures) std::cout << "  " << f << "\n";
        return 1;
    }
    return 0;
}
