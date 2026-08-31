// Piece 1 unit tests. Lightweight assertion harness (no external deps).
#include "workstation/core/Document.h"
#include "workstation/core/Model.h"
#include "workstation/core/Element.h"
#include "workstation/display/ElementRef.h"
#include "workstation/display/GraphicsCache.h"
#include "workstation/display/CacheKeys.h"
#include "workstation/display/CachedGraphics.h"

#include <iostream>
#include <string>
#include <functional>
#include <vector>
#include <stdexcept>
#include <cmath>

static int g_pass = 0;
static int g_fail = 0;
static std::vector<std::string> g_failures;

static void run(const std::string& name, std::function<void()> f) {
    try {
        f();
        ++g_pass;
        std::cout << "[PASS] " << name << "\n";
    } catch (const std::exception& e) {
        ++g_fail;
        g_failures.push_back(name + ": " + e.what());
        std::cout << "[FAIL] " << name << ": " << e.what() << "\n";
    }
}

static void check(bool cond, const std::string& msg) {
    if (!cond) throw std::runtime_error(msg);
}

// Build a fresh element + ref pair for isolated tests.
static workstation::ElementRef makeRef(workstation::Element*& outElem) {
    static workstation::Document doc(workstation::DocumentId(1));
    static workstation::Model* model = doc.CreateModel("M");
    workstation::Element* e = model->CreateElement("E");
    outElem = e;
    return workstation::ElementRef(e->Id(), e);
}

// ----- TEST 1 -----
static void test1_simple_direct_store_retrieve() {
    workstation::Element* e = nullptr;
    auto ref = makeRef(e);
    workstation::GraphicsUnsizedKey key;
    key.variant = 0;
    auto g = workstation::CreateCachedGraphics(e->Id(), e->GeometryRevision(), 7);
    workstation::SaveCachedGraphics(ref, key, 0.0, 0.0, g);
    auto hit = workstation::FindCachedGraphics(ref, key, 0.0);
    check(hit != nullptr, "expected direct cache hit");
    check(hit->GraphicsId() == g->GraphicsId(), "graphics id mismatch");
    check(hit->PrimitiveCount() == 7, "primitive count mismatch");
}

// ----- TEST 2 -----
static void test2_different_variant_slots() {
    workstation::Element* e = nullptr;
    auto ref = makeRef(e);
    auto g0 = workstation::CreateCachedGraphics(e->Id(), e->GeometryRevision());
    auto g1 = workstation::CreateCachedGraphics(e->Id(), e->GeometryRevision());
    workstation::GraphicsUnsizedKey k0; k0.variant = 0;
    workstation::GraphicsUnsizedKey k1; k1.variant = 1;
    workstation::SaveCachedGraphics(ref, k0, 0.0, 0.0, g0);
    workstation::SaveCachedGraphics(ref, k1, 0.0, 0.0, g1);
    auto h0 = workstation::FindCachedGraphics(ref, k0, 0.0);
    auto h1 = workstation::FindCachedGraphics(ref, k1, 0.0);
    check(h0 && h0->GraphicsId() == g0->GraphicsId(), "variant 0 wrong");
    check(h1 && h1->GraphicsId() == g1->GraphicsId(), "variant 1 wrong");
    check(h0->GraphicsId() != h1->GraphicsId(), "variants must be distinct");
}

// ----- TEST 3 -----
static void test3_specialized_sorted_by_min_metric() {
    workstation::Element* e = nullptr;
    auto ref = makeRef(e);
    auto mkKey = [](uint32_t t) {
        workstation::GraphicsUnsizedKey k; k.transformKey = t; return k;
    };
    // Insert out of order: min 20, 5, 10
    auto gA = workstation::CreateCachedGraphics(e->Id(), e->GeometryRevision());
    auto gB = workstation::CreateCachedGraphics(e->Id(), e->GeometryRevision());
    auto gC = workstation::CreateCachedGraphics(e->Id(), e->GeometryRevision());
    workstation::SaveCachedGraphics(ref, mkKey(1), 20.0, 1.0, gA); // [20,20]
    workstation::SaveCachedGraphics(ref, mkKey(2), 5.0,  1.0, gB); // [5,5]
    workstation::SaveCachedGraphics(ref, mkKey(3), 10.0, 1.0, gC); // [10,10]
    auto* set = ref.Specialized();
    check(set != nullptr, "specialized set missing");
    check(set->Size() == 3, "expected 3 entries");
    const auto& entries = set->Entries();
    check(entries[0].minViewMetric == 5.0,  "entry0 min wrong");
    check(entries[1].minViewMetric == 10.0, "entry1 min wrong");
    check(entries[2].minViewMetric == 20.0, "entry2 min wrong");
}

// ----- TEST 4 -----
static void test4_validity_range_computation() {
    workstation::Element* e = nullptr;
    auto ref = makeRef(e);
    workstation::GraphicsUnsizedKey key; key.transformKey = 1; // specialized
    auto g = workstation::CreateCachedGraphics(e->Id(), e->GeometryRevision());
    workstation::SaveCachedGraphics(ref, key, 10.0, 2.0, g);
    const auto& entries = ref.Specialized()->Entries();
    check(entries.size() == 1, "expected one entry");
    check(std::abs(entries[0].minViewMetric - 5.0) < 1e-9,  "min should be 5");
    check(std::abs(entries[0].maxViewMetric - 20.0) < 1e-9, "max should be 20");
}

// ----- TEST 5 -----
static void test5_metric_inside_hit() {
    workstation::Element* e = nullptr;
    auto ref = makeRef(e);
    workstation::GraphicsUnsizedKey key; key.transformKey = 1;
    auto g = workstation::CreateCachedGraphics(e->Id(), e->GeometryRevision());
    workstation::SaveCachedGraphics(ref, key, 10.0, 2.0, g);
    auto hit = workstation::FindCachedGraphics(ref, key, 10.0);
    check(hit != nullptr, "metric 10 inside [5,20] should hit");
}

// ----- TEST 6 -----
static void test6_metric_outside_miss() {
    workstation::Element* e = nullptr;
    auto ref = makeRef(e);
    workstation::GraphicsUnsizedKey key; key.transformKey = 1;
    auto g = workstation::CreateCachedGraphics(e->Id(), e->GeometryRevision());
    workstation::SaveCachedGraphics(ref, key, 10.0, 2.0, g);
    auto hit = workstation::FindCachedGraphics(ref, key, 25.0);
    check(hit == nullptr, "metric 25 outside [5,20] should miss");
}

// ----- TEST 7 -----
static void test7_different_transform_miss() {
    workstation::Element* e = nullptr;
    auto ref = makeRef(e);
    workstation::GraphicsUnsizedKey save; save.transformKey = 5;
    auto g = workstation::CreateCachedGraphics(e->Id(), e->GeometryRevision());
    workstation::SaveCachedGraphics(ref, save, 10.0, 2.0, g);
    workstation::GraphicsUnsizedKey look; look.transformKey = 6; // different
    auto hit = workstation::FindCachedGraphics(ref, look, 10.0);
    check(hit == nullptr, "different transform key should miss");
}

// ----- TEST 8 -----
static void test8_different_variant_miss() {
    workstation::Element* e = nullptr;
    auto ref = makeRef(e);
    workstation::GraphicsUnsizedKey save; save.transformKey = 1; save.variant = 3;
    auto g = workstation::CreateCachedGraphics(e->Id(), e->GeometryRevision());
    workstation::SaveCachedGraphics(ref, save, 10.0, 2.0, g);
    workstation::GraphicsUnsizedKey look; look.transformKey = 1; look.variant = 4;
    auto hit = workstation::FindCachedGraphics(ref, look, 10.0);
    check(hit == nullptr, "different variant should miss");
}

// ----- TEST 9 -----
static void test9_different_style_miss() {
    workstation::Element* e = nullptr;
    auto ref = makeRef(e);
    workstation::GraphicsUnsizedKey save;
    save.styleKey = workstation::DisplayStyleCacheKey(1);
    auto g = workstation::CreateCachedGraphics(e->Id(), e->GeometryRevision());
    workstation::SaveCachedGraphics(ref, save, 10.0, 2.0, g);
    workstation::GraphicsUnsizedKey look;
    look.styleKey = workstation::DisplayStyleCacheKey(2); // different
    auto hit = workstation::FindCachedGraphics(ref, look, 10.0);
    check(hit == nullptr, "different style key should miss");
}

// ----- TEST 10 -----
static void test10_different_filter_miss() {
    workstation::Element* e = nullptr;
    auto ref = makeRef(e);
    workstation::GraphicsUnsizedKey save;
    save.filterKey = workstation::DisplayFilterCacheKey(1);
    auto g = workstation::CreateCachedGraphics(e->Id(), e->GeometryRevision());
    workstation::SaveCachedGraphics(ref, save, 10.0, 2.0, g);
    workstation::GraphicsUnsizedKey look;
    look.filterKey = workstation::DisplayFilterCacheKey(2); // different
    auto hit = workstation::FindCachedGraphics(ref, look, 10.0);
    check(hit == nullptr, "different filter key should miss");
}

// ----- TEST 11 -----
static void test11_overlapping_representations_allowed() {
    workstation::Element* e = nullptr;
    auto ref = makeRef(e);
    auto mkKey = [](uint32_t t) {
        workstation::GraphicsUnsizedKey k; k.transformKey = t; return k;
    };
    // Overlapping ranges: [5,20] and [15,40] both contain metric 17.
    auto g1 = workstation::CreateCachedGraphics(e->Id(), e->GeometryRevision());
    auto g2 = workstation::CreateCachedGraphics(e->Id(), e->GeometryRevision());
    workstation::SaveCachedGraphics(ref, mkKey(1), 10.0, 2.0, g1); // [5,20]
    workstation::SaveCachedGraphics(ref, mkKey(2), 25.0, 1.6, g2); // [15.625,40]
    check(ref.Specialized()->Size() == 2, "overlapping entries must both be kept");
    auto hit = workstation::FindCachedGraphics(ref, mkKey(2), 17.0);
    check(hit != nullptr, "metric 17 should hit one of the overlapping entries");
}

// ----- TEST 12 -----
static void test12_invalidation_clears() {
    workstation::Element* e = nullptr;
    auto ref = makeRef(e);
    // direct
    workstation::GraphicsUnsizedKey dk; dk.variant = 0;
    auto gd = workstation::CreateCachedGraphics(e->Id(), e->GeometryRevision());
    workstation::SaveCachedGraphics(ref, dk, 0.0, 0.0, gd);
    // specialized
    workstation::GraphicsUnsizedKey sk; sk.transformKey = 1;
    auto gs = workstation::CreateCachedGraphics(e->Id(), e->GeometryRevision());
    workstation::SaveCachedGraphics(ref, sk, 10.0, 2.0, gs);

    check(workstation::FindCachedGraphics(ref, dk, 0.0) != nullptr, "direct before invalidate");
    check(workstation::FindCachedGraphics(ref, sk, 10.0) != nullptr, "specialized before invalidate");

    ref.InvalidateGraphics();

    check(workstation::FindCachedGraphics(ref, dk, 0.0) == nullptr, "direct after invalidate");
    check(workstation::FindCachedGraphics(ref, sk, 10.0) == nullptr, "specialized after invalidate");
}

// ----- TEST 13 -----
static void test13_geometry_revision_invalidates() {
    workstation::Element* e = nullptr;
    auto ref = makeRef(e);
    // direct
    workstation::GraphicsUnsizedKey dk; dk.variant = 0;
    auto gd = workstation::CreateCachedGraphics(e->Id(), e->GeometryRevision());
    workstation::SaveCachedGraphics(ref, dk, 0.0, 0.0, gd);
    // specialized
    workstation::GraphicsUnsizedKey sk; sk.transformKey = 1;
    auto gs = workstation::CreateCachedGraphics(e->Id(), e->GeometryRevision());
    workstation::SaveCachedGraphics(ref, sk, 10.0, 2.0, gs);

    check(workstation::FindCachedGraphics(ref, dk, 0.0) != nullptr, "direct before touch");
    check(workstation::FindCachedGraphics(ref, sk, 10.0) != nullptr, "specialized before touch");

    e->TouchGeometry(); // geometry changed -> stale revision

    check(workstation::FindCachedGraphics(ref, dk, 0.0) == nullptr, "direct stale after touch");
    check(workstation::FindCachedGraphics(ref, sk, 10.0) == nullptr, "specialized stale after touch");
}

int main() {
    run("TEST 1  simple direct store/retrieve", test1_simple_direct_store_retrieve);
    run("TEST 2  different variant slots",       test2_different_variant_slots);
    run("TEST 3  specialized sorted by min",     test3_specialized_sorted_by_min_metric);
    run("TEST 4  validity range [5,20]",         test4_validity_range_computation);
    run("TEST 5  metric inside hit",             test5_metric_inside_hit);
    run("TEST 6  metric outside miss",           test6_metric_outside_miss);
    run("TEST 7  different transform miss",      test7_different_transform_miss);
    run("TEST 8  different variant miss",        test8_different_variant_miss);
    run("TEST 9  different style miss",          test9_different_style_miss);
    run("TEST 10 different filter miss",         test10_different_filter_miss);
    run("TEST 11 overlapping allowed",           test11_overlapping_representations_allowed);
    run("TEST 12 invalidation clears",           test12_invalidation_clears);
    run("TEST 13 geometry revision invalidates", test13_geometry_revision_invalidates);

    std::cout << "\n========================================\n";
    std::cout << "PASS: " << g_pass << "  FAIL: " << g_fail << "\n";
    if (g_fail > 0) {
        std::cout << "Failures:\n";
        for (const auto& f : g_failures) std::cout << "  " << f << "\n";
        return 1;
    }
    return 0;
}
