// Piece 3 unit tests: retained graphics view-output submission layer.
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
#include "workstation/math/Transform3d.h"

#include <iostream>
#include <string>
#include <functional>
#include <vector>
#include <optional>
#include <stdexcept>

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

// ---- test fixtures ----
static workstation::Document& sharedDoc() {
    static workstation::Document doc(workstation::DocumentId(200));
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
    int n; mutable int callCount = 0;
    explicit FakeProvider(int c) : n(c) {}
    void EmitGraphics(const workstation::Element&, workstation::ViewContext&,
                     workstation::GraphicsRecorder& r) const override {
        ++callCount;
        for (int i = 0; i < n; ++i) r.EmitCommand(1, (uint64_t)i);
    }
};
struct AssemblyProvider : workstation::IElementGraphicsProvider {
    workstation::ElementGraphicsService& svc;
    workstation::ElementRef& childRef;
    const workstation::IElementGraphicsProvider& childProvider;
    workstation::GraphicsUnsizedKey childKey;
    double cm, cf; mutable int callCount = 0;
    AssemblyProvider(workstation::ElementGraphicsService& s, workstation::ElementRef& cr,
                     const workstation::IElementGraphicsProvider& cp,
                     workstation::GraphicsUnsizedKey ck, double c, double f)
        : svc(s), childRef(cr), childProvider(cp), childKey(std::move(ck)), cm(c), cf(f) {}
    void EmitGraphics(const workstation::Element&, workstation::ViewContext& v,
                      workstation::GraphicsRecorder& r) const override {
        ++callCount;
        r.EmitCommand(1, 11);
        workstation::GraphicsResolveRequest cr{childRef, childProvider, childKey, cm, cf};
        svc.Resolve(v, cr);
        r.EmitCommand(1, 12);
    }
};

// ---- recording test double ----
struct ViewOutCall {
    enum Kind { Push, Pop, Apply, SubmitA, SubmitB } kind;
    std::optional<workstation::math::Transform3d> transform;
    std::optional<workstation::ClipVolumeToken> clip;
    workstation::ElementRenderOverrides overrides;
    uint64_t graphicsId = 0;
    double   param = 0;
};

struct RecordingViewOutput : workstation::IViewOutput {
    std::vector<ViewOutCall> calls;
    void PushTransformClip(const workstation::math::Transform3d* t,
                            const workstation::ClipVolumeToken* c) override {
        ViewOutCall o; o.kind = ViewOutCall::Push;
        if (t) o.transform = *t;
        if (c) o.clip = *c;
        calls.push_back(o);
    }
    void PopTransformClip() override { calls.push_back({ViewOutCall::Pop}); }
    void ApplyElementOverrides(const workstation::ElementRenderOverrides& o) override {
        ViewOutCall c; c.kind = ViewOutCall::Apply; c.overrides = o; calls.push_back(c);
    }
    void SubmitRetainedPathA(const workstation::CachedGraphicsHandle& g) override {
        ViewOutCall c; c.kind = ViewOutCall::SubmitA;
        c.graphicsId = g ? g->GraphicsId() : 0; calls.push_back(c);
    }
    void SubmitRetainedPathB(const workstation::CachedGraphicsHandle& g, double p) override {
        ViewOutCall c; c.kind = ViewOutCall::SubmitB;
        c.graphicsId = g ? g->GraphicsId() : 0; c.param = p; calls.push_back(c);
    }
    bool hasPushThenPop() const {
        int p=0; for (auto& c:calls){ if(c.kind==ViewOutCall::Push)p++; if(c.kind==ViewOutCall::Pop)p--; }
        return p==0;
    }
};

struct ThrowingViewOutput : RecordingViewOutput {
    bool throwOnA = true;
    void SubmitRetainedPathA(const workstation::CachedGraphicsHandle&) override { throw std::runtime_error("submit A failure"); }
    void SubmitRetainedPathB(const workstation::CachedGraphicsHandle&, double) override { throw std::runtime_error("submit B failure"); }
};

// ----- TEST 1 -----
static void test1_pathA_ordering_no_overrides() {
    workstation::Element* e = nullptr; auto ref = makeRef(e);
    workstation::ViewContext view; RecordingViewOutput out; view.output = &out;
    FakeProvider prov(2);
    workstation::ElementGraphicsService svc;
    auto g = svc.Resolve(view, {ref, prov, workstation::GraphicsUnsizedKey{}, 10.0, 0.0});
    workstation::RetainedGraphicsPresenter pres;
    workstation::RetainedSubmissionRequest req; req.graphics = g;
    req.path = workstation::RetainedSubmissionPath::PathA;
    pres.Present(view, req);
    check(out.calls.size() == 3, "expected Push, SubmitA, Pop");
    check(out.calls[0].kind == ViewOutCall::Push, "first is Push");
    check(out.calls[1].kind == ViewOutCall::SubmitA, "second is SubmitA");
    check(out.calls[2].kind == ViewOutCall::Pop, "third is Pop");
}

// ----- TEST 2 -----
static void test2_pathB_receives_parameter() {
    workstation::Element* e = nullptr; auto ref = makeRef(e);
    workstation::ViewContext view; RecordingViewOutput out; view.output = &out;
    view.retainedSubmissionParameter = 2.5;
    workstation::ElementGraphicsService svc;
    auto g = svc.Resolve(view, {ref, FakeProvider(1), workstation::GraphicsUnsizedKey{}, 10.0, 0.0});
    workstation::RetainedGraphicsPresenter pres;
    workstation::RetainedSubmissionRequest req; req.graphics = g;
    req.path = workstation::RetainedSubmissionPath::PathB;
    pres.Present(view, req);
    bool found = false;
    for (auto& c : out.calls)
        if (c.kind == ViewOutCall::SubmitB) {
            found = true;
            check(c.graphicsId == g->GraphicsId(), "SubmitB graphics id");
            check(std::abs(c.param - 2.5) < 1e-9, "SubmitB receives view parameter");
        }
    check(found, "SubmitB was called");
}

// ----- TEST 3 -----
static void test3_transform_clip_passthrough() {
    workstation::Element* e = nullptr; auto ref = makeRef(e);
    workstation::ViewContext view; RecordingViewOutput out; view.output = &out;
    workstation::ElementGraphicsService svc;
    auto g = svc.Resolve(view, {ref, FakeProvider(1), workstation::GraphicsUnsizedKey{}, 10.0, 0.0});
    workstation::RetainedGraphicsPresenter pres;
    workstation::RetainedSubmissionRequest req; req.graphics = g;
    const workstation::math::Transform3d expectedXform(
        1.0, 0.0, 0.0, 1.0,
        0.0, 1.0, 0.0, 2.0,
        0.0, 0.0, 1.0, 3.0);
    req.transform = expectedXform;
    req.clip = workstation::ClipVolumeToken{222};
    pres.Present(view, req);
    bool ok = false;
    for (auto& c : out.calls)
        if (c.kind == ViewOutCall::Push && c.transform && c.clip &&
            *c.transform == expectedXform && c.clip->value == 222)
            ok = true;
    check(ok, "transform/clip values reach IViewOutput unchanged");
}

// ----- TEST 4 -----
static void test4_temporary_override_ordering() {
    workstation::Element* e = nullptr; auto ref = makeRef(e);
    workstation::ViewContext view; RecordingViewOutput out; view.output = &out;
    view.currentRenderOverrides.lineColor = 5; // original
    workstation::ElementGraphicsService svc;
    auto g = svc.Resolve(view, {ref, FakeProvider(1), workstation::GraphicsUnsizedKey{}, 10.0, 0.0});
    workstation::RetainedGraphicsPresenter pres;
    workstation::RetainedSubmissionRequest req; req.graphics = g;
    req.path = workstation::RetainedSubmissionPath::PathA;
    req.augmentOverrides = true;
    pres.Present(view, req);
    // Push, Apply(temp lineColor=0xAA), SubmitA, Apply(original lineColor=5), Pop
    check(out.calls.size() == 5, "expected 5 events");
    check(out.calls[0].kind == ViewOutCall::Push, "Push first");
    check(out.calls[1].kind == ViewOutCall::Apply && out.calls[1].overrides.lineColor == 0xAA, "temp override applied");
    check(out.calls[2].kind == ViewOutCall::SubmitA, "submit");
    check(out.calls[3].kind == ViewOutCall::Apply && out.calls[3].overrides.lineColor == 5, "original override restored");
    check(out.calls[4].kind == ViewOutCall::Pop, "Pop last");
}

// ----- TEST 5 -----
static void test5_original_overrides_unchanged() {
    workstation::Element* e = nullptr; auto ref = makeRef(e);
    workstation::ViewContext view; RecordingViewOutput out; view.output = &out;
    view.currentRenderOverrides.lineColor = 5;
    workstation::ElementGraphicsService svc;
    auto g = svc.Resolve(view, {ref, FakeProvider(1), workstation::GraphicsUnsizedKey{}, 10.0, 0.0});
    workstation::RetainedGraphicsPresenter pres;
    workstation::RetainedSubmissionRequest req; req.graphics = g;
    req.augmentOverrides = true;
    pres.Present(view, req);
    check(view.currentRenderOverrides.lineColor == 5, "original overrides unchanged after presentation");
    check(view.currentRenderOverrides.fillColor == std::nullopt, "no stray overrides");
}

// ----- TEST 6 -----
static void test6_submit_throw_restores_and_pops() {
    workstation::Element* e = nullptr; auto ref = makeRef(e);
    workstation::ViewContext view; ThrowingViewOutput out; view.output = &out;
    view.currentRenderOverrides.lineColor = 5;
    workstation::ElementGraphicsService svc;
    auto g = svc.Resolve(view, {ref, FakeProvider(1), workstation::GraphicsUnsizedKey{}, 10.0, 0.0});
    workstation::RetainedGraphicsPresenter pres;
    workstation::RetainedSubmissionRequest req; req.graphics = g;
    req.augmentOverrides = true;
    bool threw = false;
    try { pres.Present(view, req); } catch (const std::exception&) { threw = true; }
    check(threw, "submit threw");
    check(out.hasPushThenPop(), "transform/clip scope balanced (Pop called)");
    bool restored = false;
    for (auto& c : out.calls)
        if (c.kind == ViewOutCall::Apply && c.overrides.lineColor == 5) restored = true;
    check(restored, "original overrides restored after throw");
    check(view.currentRenderOverrides.lineColor == 5, "view overrides restored after throw");
}

// ----- TEST 7 -----
static void test7_apply_or_submitB_throw_balanced() {
    workstation::Element* e = nullptr; auto ref = makeRef(e);
    workstation::ViewContext view; ThrowingViewOutput out; view.output = &out;
    view.retainedSubmissionParameter = 1.0;
    workstation::ElementGraphicsService svc;
    auto g = svc.Resolve(view, {ref, FakeProvider(1), workstation::GraphicsUnsizedKey{}, 10.0, 0.0});
    workstation::RetainedGraphicsPresenter pres;
    workstation::RetainedSubmissionRequest req; req.graphics = g;
    req.path = workstation::RetainedSubmissionPath::PathB;
    bool threw = false;
    try { pres.Present(view, req); } catch (const std::exception&) { threw = true; }
    check(threw, "submit B threw");
    check(out.hasPushThenPop(), "no leaked transform/clip state (balanced)");
}

// ----- TEST 8 -----
static void test8_persistent_remains_in_cache() {
    workstation::Element* e = nullptr; auto ref = makeRef(e);
    workstation::ViewContext view; RecordingViewOutput out; view.output = &out;
    workstation::ElementGraphicsService svc;
    workstation::RetainedGraphicsPresenter pres;
    workstation::ElementPresentationService presSvc(svc, pres);
    FakeProvider prov(4);
    workstation::GraphicsUnsizedKey key; key.transformKey = 1;
    workstation::GraphicsResolveRequest rreq{ref, prov, key, 10.0, 2.0};
    presSvc.Draw(view, {rreq});
    check(workstation::FindCachedGraphics(ref, key, 10.0) != nullptr,
          "persistent graphics remain in cache after presentation");
    check(prov.callCount == 1, "built exactly once");
}

// ----- TEST 9 -----
static void test9_transient_not_in_cache() {
    workstation::Element* e = nullptr; auto ref = makeRef(e);
    workstation::ViewContext view; RecordingViewOutput out; view.output = &out;
    workstation::ElementGraphicsService svc;
    workstation::RetainedGraphicsPresenter pres;
    workstation::ElementPresentationService presSvc(svc, pres);
    FakeProvider prov(4);
    workstation::GraphicsUnsizedKey key; key.transformKey = 1;
    workstation::GraphicsResolveRequest rreq{ref, prov, key, 10.0, 2.0};
    rreq.allowPersistentCache = false;
    presSvc.Draw(view, {rreq});
    check(workstation::FindCachedGraphics(ref, key, 10.0) == nullptr,
          "transient graphics NOT entered into cache");
    check(prov.callCount == 1, "transient still built once");
}

// ----- TEST 10 -----
static void test10_transient_presented_once() {
    workstation::Element* e = nullptr; auto ref = makeRef(e);
    workstation::ViewContext view; RecordingViewOutput out; view.output = &out;
    workstation::ElementGraphicsService svc;
    workstation::RetainedGraphicsPresenter pres;
    workstation::ElementPresentationService presSvc(svc, pres);
    FakeProvider prov(4);
    workstation::GraphicsUnsizedKey key; key.transformKey = 1;
    workstation::GraphicsResolveRequest rreq{ref, prov, key, 10.0, 2.0};
    rreq.allowPersistentCache = false;
    presSvc.Draw(view, {rreq});
    bool submitted = false;
    for (auto& c : out.calls) if (c.kind == ViewOutCall::SubmitA) submitted = true;
    check(submitted, "transient graphics presented (SubmitA)");
    check(pres.Stats().submissions == 1, "one submission recorded");
    check(pres.Stats().transientSubmissions == 1, "transient submission recorded");
}

// ----- TEST 11 -----
static void test11_transient_lifetime_released() {
    workstation::Element* e = nullptr; auto ref = makeRef(e);
    workstation::ViewContext view; RecordingViewOutput out; view.output = &out;
    workstation::ElementGraphicsService svc;
    workstation::GraphicsUnsizedKey key; key.transformKey = 1;
    workstation::GraphicsResolveRequest rreq{ref, FakeProvider(3), key, 10.0, 2.0};
    rreq.allowPersistentCache = false;
    auto res = svc.ResolveForPresentation(view, rreq);
    check(res.retention == workstation::GraphicsRetention::Transient, "flagged transient");
    std::weak_ptr<workstation::CachedGraphics> wp = res.graphics;
    check(!wp.expired(), "alive while held");
    {
        workstation::RetainedGraphicsPresenter pres;
        workstation::RetainedSubmissionRequest sreq; sreq.graphics = res.graphics;
        pres.Present(view, sreq);
        // sreq destroyed at end of block -> releases its strong reference
    }
    res.graphics.reset();
    check(wp.expired(), "no cache entry; shared_ptr released -> destroyed");
    check(workstation::FindCachedGraphics(ref, key, 10.0) == nullptr, "no cache entry remains");
}

// ----- TEST 12 -----
static void test12_no_graphics_no_submission() {
    workstation::ViewContext view; RecordingViewOutput out; view.output = &out;
    workstation::RetainedGraphicsPresenter pres;
    workstation::RetainedSubmissionRequest req; req.graphics = nullptr;
    pres.Present(view, req);
    check(out.calls.empty(), "no PushTransformClip / submission when graphics null");
}

// ----- TEST 13 -----
static void test13_nested_recording_unchanged() {
    workstation::Element* parent = nullptr; auto pref = makeRef(parent);
    workstation::Element* child = nullptr;  auto cref = makeRef(child);
    workstation::ViewContext view;
    workstation::ElementGraphicsService svc;
    FakeProvider childProv(3);
    workstation::GraphicsUnsizedKey childKey; childKey.transformKey = 2;
    AssemblyProvider asmProv(svc, cref, childProv, childKey, 10.0, 2.0);
    workstation::GraphicsUnsizedKey parentKey; parentKey.transformKey = 1;
    workstation::GraphicsResolveRequest req{pref, asmProv, parentKey, 10.0, 2.0};
    auto g = svc.Resolve(view, req);
    check(g && g->PrimitiveCount() == 5, "nested recording: parent(2)+child(3)=5");
    check(asmProv.callCount == 1 && childProv.callCount == 1, "both providers ran");
    check(svc.Stats().nestedDirectEmits == 1, "nested direct emit counted");
}

int main() {
    run("TEST 1  PathA ordering (no overrides)",   test1_pathA_ordering_no_overrides);
    run("TEST 2  PathB receives parameter",        test2_pathB_receives_parameter);
    run("TEST 3  transform/clip passthrough",      test3_transform_clip_passthrough);
    run("TEST 4  temporary override ordering",     test4_temporary_override_ordering);
    run("TEST 5  original overrides unchanged",    test5_original_overrides_unchanged);
    run("TEST 6  submit throw restores+pops",      test6_submit_throw_restores_and_pops);
    run("TEST 7  submitB throw balanced",          test7_apply_or_submitB_throw_balanced);
    run("TEST 8  persistent in cache",             test8_persistent_remains_in_cache);
    run("TEST 9  transient not in cache",          test9_transient_not_in_cache);
    run("TEST 10 transient presented once",        test10_transient_presented_once);
    run("TEST 11 transient lifetime released",     test11_transient_lifetime_released);
    run("TEST 12 no graphics no submission",       test12_no_graphics_no_submission);
    run("TEST 13 nested recording unchanged",      test13_nested_recording_unchanged);

    std::cout << "\n========================================\n";
    std::cout << "PASS: " << g_pass << "  FAIL: " << g_fail << "\n";
    if (g_fail > 0) {
        std::cout << "Failures:\n";
        for (const auto& f : g_failures) std::cout << "  " << f << "\n";
        return 1;
    }
    return 0;
}
