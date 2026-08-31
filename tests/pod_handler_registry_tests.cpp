#include "workstation/pod/PodHandlerRegistry.h"
#include "workstation/pod/HandlerTree.h"
#include "workstation/pod/DoubleBoundingBlockHandler.h"

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>

static int g_fail = 0;
static void check(bool c, const char* m) {
    if (!c) { printf("  FAIL: %s\n", m); ++g_fail; }
    else    { printf("  PASS: %s\n", m); }
}

using namespace workstation::pod;

// Concrete test handler for registry tests.
struct TestHandler : public PodBlockHandler {
    int id = 0;
    bool process(PodDecodeContext&) override { return true; }
};

int main() {
    printf("== NakshaPointEngine :: Piece 5 (Handler Registry + DataSource) ==\n\n");

    // TEST 1: Empty registry
    {
        PodHandlerRegistry reg;
        check(reg.empty(), "TEST1 empty");
        check(reg.size() == 0, "TEST1 size 0");
        std::vector<uint8_t> key = {0x01};
        check(reg.findLowerBound(key) == nullptr, "TEST1 lower_bound null");
    }

    // TEST 2: Single insertion
    {
        PodHandlerRegistry reg;
        TestHandler h;
        reg.insert({0x01}, &h);
        check(reg.size() == 1, "TEST2 size 1");
        std::vector<uint8_t> key = {0x01};
        check(reg.findLowerBound(key) == &h, "TEST2 exact match");
    }

    // TEST 3: Multiple ordered insertions
    {
        PodHandlerRegistry reg;
        TestHandler h1, h2, h3;
        reg.insert({0x01}, &h1);
        reg.insert({0x02}, &h2);
        reg.insert({0x03}, &h3);
        check(reg.size() == 3, "TEST3 size 3");
        std::vector<uint8_t> k1 = {0x01};
        std::vector<uint8_t> k2 = {0x02};
        std::vector<uint8_t> k3 = {0x03};
        check(reg.findLowerBound(k1) == &h1, "TEST3 find h1");
        check(reg.findLowerBound(k2) == &h2, "TEST3 find h2");
        check(reg.findLowerBound(k3) == &h3, "TEST3 find h3");
    }

    // TEST 4: Reverse ordered insertions
    {
        PodHandlerRegistry reg;
        TestHandler h1, h2, h3;
        reg.insert({0x03}, &h3);
        reg.insert({0x02}, &h2);
        reg.insert({0x01}, &h1);
        check(reg.size() == 3, "TEST4 size 3");
        std::vector<uint8_t> k1 = {0x01};
        std::vector<uint8_t> k2 = {0x02};
        std::vector<uint8_t> k3 = {0x03};
        check(reg.findLowerBound(k1) == &h1, "TEST4 find h1");
        check(reg.findLowerBound(k2) == &h2, "TEST4 find h2");
        check(reg.findLowerBound(k3) == &h3, "TEST4 find h3");
    }

    // TEST 5: Random insertion order
    {
        PodHandlerRegistry reg;
        TestHandler handlers[5];
        reg.insert({0x05}, &handlers[4]);
        reg.insert({0x01}, &handlers[0]);
        reg.insert({0x03}, &handlers[2]);
        reg.insert({0x02}, &handlers[1]);
        reg.insert({0x04}, &handlers[3]);
        check(reg.size() == 5, "TEST5 size 5");
        for (int i = 1; i <= 5; ++i) {
            std::vector<uint8_t> key = {static_cast<uint8_t>(i)};
            check(reg.findLowerBound(key) == &handlers[i-1],
                  ("TEST5 find handler " + std::to_string(i)).c_str());
        }
    }

    // TEST 6: Lower-bound exact match
    {
        PodHandlerRegistry reg;
        TestHandler h1, h2;
        reg.insert({0x01, 0x02}, &h1);
        reg.insert({0x01, 0x03}, &h2);
        std::vector<uint8_t> key = {0x01, 0x02};
        check(reg.findLowerBound(key) == &h1, "TEST6 exact match");
    }

    // TEST 7: Lower-bound between keys
    {
        PodHandlerRegistry reg;
        TestHandler h1, h2;
        reg.insert({0x01, 0x02}, &h1);
        reg.insert({0x01, 0x04}, &h2);
        std::vector<uint8_t> key = {0x01, 0x03};
        check(reg.findLowerBound(key) == &h2, "TEST7 between keys");
    }

    // TEST 8: Lower-bound before first
    {
        PodHandlerRegistry reg;
        TestHandler h1, h2;
        reg.insert({0x02}, &h1);
        reg.insert({0x04}, &h2);
        std::vector<uint8_t> key = {0x01};
        check(reg.findLowerBound(key) == &h1, "TEST8 before first");
    }

    // TEST 9: Lower-bound after last
    {
        PodHandlerRegistry reg;
        TestHandler h1;
        reg.insert({0x01}, &h1);
        std::vector<uint8_t> key = {0xFF};
        check(reg.findLowerBound(key) == nullptr, "TEST9 after last");
    }

    // TEST 10: Prefix keys
    {
        PodHandlerRegistry reg;
        TestHandler h1, h2, h3;
        reg.insert({0x41}, &h1);           // "A"
        reg.insert({0x41, 0x42}, &h2);     // "AB"
        reg.insert({0x41, 0x42, 0x43}, &h3); // "ABC"
        check(reg.size() == 3, "TEST10 size 3");
        std::vector<uint8_t> kA = {0x41};
        std::vector<uint8_t> kAB = {0x41, 0x42};
        std::vector<uint8_t> kABC = {0x41, 0x42, 0x43};
        check(reg.findLowerBound(kA) == &h1, "TEST10 A");
        check(reg.findLowerBound(kAB) == &h2, "TEST10 AB");
        check(reg.findLowerBound(kABC) == &h3, "TEST10 ABC");
    }

    // TEST 11: Binary keys containing zero bytes
    {
        PodHandlerRegistry reg;
        TestHandler h1, h2;
        reg.insert({0x00, 0x00, 0x01}, &h1);
        reg.insert({0x00, 0x00, 0x02}, &h2);
        std::vector<uint8_t> k1 = {0x00, 0x00, 0x01};
        std::vector<uint8_t> k2 = {0x00, 0x00, 0x02};
        check(reg.findLowerBound(k1) == &h1, "TEST11 zero-byte key 1");
        check(reg.findLowerBound(k2) == &h2, "TEST11 zero-byte key 2");
        // Search for {0x00, 0x00, 0x01, 0x00} — should return h2 (past h1)
        std::vector<uint8_t> k3 = {0x00, 0x00, 0x01, 0x00};
        check(reg.findLowerBound(k3) == &h2, "TEST11 zero-byte past match");
    }

    // TEST 12: Duplicate keys
    {
        PodHandlerRegistry reg;
        TestHandler h1, h2;
        reg.insert({0x01, 0x02}, &h1);
        reg.insert({0x01, 0x02}, &h2);
        check(reg.size() == 2, "TEST12 duplicate keys stored");
        std::vector<uint8_t> key = {0x01, 0x02};
        // Lower bound returns the first matching or greater key.
        // Both h1 and h2 have the same key; behavior depends on tree implementation.
        PodBlockHandler* found = reg.findLowerBound(key);
        check(found == &h1 || found == &h2, "TEST12 duplicate returns valid");
    }

    // TEST 13: Tree size
    {
        PodHandlerRegistry reg;
        for (int i = 0; i < 100; ++i) {
            std::vector<uint8_t> key = {static_cast<uint8_t>(i & 0xFF),
                                        static_cast<uint8_t>((i >> 8) & 0xFF)};
            reg.insert(key, nullptr);
        }
        check(reg.size() == 100, "TEST13 size 100");
    }

    // TEST 14: Parent pointers
    {
        PodHandlerRegistry reg;
        TestHandler h1, h2, h3, h4, h5;
        reg.insert({0x03}, &h3);
        reg.insert({0x01}, &h1);
        reg.insert({0x05}, &h5);
        reg.insert({0x02}, &h2);
        reg.insert({0x04}, &h4);
        // If validateInvariants passes, parent pointers are correct.
        check(reg.validateInvariants(), "TEST14 parent pointers valid");
    }

    // TEST 15: Root black
    {
        PodHandlerRegistry reg;
        TestHandler h1, h2, h3;
        reg.insert({0x02}, &h2);
        reg.insert({0x01}, &h1);
        reg.insert({0x03}, &h3);
        check(reg.validateInvariants(), "TEST15 root black");
    }

    // TEST 16: Red-black invariant validation
    {
        PodHandlerRegistry reg;
        TestHandler handlers[10];
        for (int i = 0; i < 10; ++i) {
            reg.insert({static_cast<uint8_t>(i + 1)}, &handlers[i]);
        }
        check(reg.validateInvariants(), "TEST16 red-black valid");
    }

    // TEST 17: Black-height consistency
    {
        PodHandlerRegistry reg;
        TestHandler handlers[15];
        // Insert in random-ish order to force rotations.
        int order[] = {8, 3, 10, 1, 6, 14, 4, 7, 13, 2, 5, 9, 11, 12, 15};
        for (int i = 0; i < 15; ++i) {
            uint8_t val = static_cast<uint8_t>(order[i]);
            reg.insert({val}, &handlers[i]);
        }
        check(reg.validateInvariants(), "TEST17 black-height consistent");
    }

    // TEST 18: Rotations preserve ordering
    {
        PodHandlerRegistry reg;
        TestHandler handlers[7];
        // Insert in order that forces left and right rotations.
        int order[] = {3, 1, 5, 2, 4, 6, 7};
        for (int i = 0; i < 7; ++i) {
            uint8_t val = static_cast<uint8_t>(order[i]);
            reg.insert({val}, &handlers[i]);
        }
        check(reg.validateInvariants(), "TEST18 rotations preserve ordering");
        // Verify all keys are findable.
        for (int i = 1; i <= 7; ++i) {
            std::vector<uint8_t> key = {static_cast<uint8_t>(i)};
            check(reg.findLowerBound(key) != nullptr,
                  ("TEST18 key " + std::to_string(i) + " findable").c_str());
        }
    }

    // TEST 19: Handler association
    {
        PodHandlerRegistry reg;
        TestHandler h1, h2;
        h1.id = 42;
        h2.id = 99;
        reg.insert({0x01}, &h1);
        reg.insert({0x02}, &h2);
        std::vector<uint8_t> k1 = {0x01};
        std::vector<uint8_t> k2 = {0x02};
        auto* found1 = dynamic_cast<TestHandler*>(reg.findLowerBound(k1));
        auto* found2 = dynamic_cast<TestHandler*>(reg.findLowerBound(k2));
        check(found1 && found1->id == 42, "TEST19 handler 42");
        check(found2 && found2->id == 99, "TEST19 handler 99");
    }

    // TEST 20: Registration of DoubleBoundingBlockHandler
    {
        PodHandlerRegistry reg;
        DoubleBoundingBlockHandler handler;
        std::vector<uint8_t> key = {0x01, 0x02, 0x03, 0x04};
        reg.insert(key, &handler);
        check(reg.size() == 1, "TEST20 handler registered");
        std::vector<uint8_t> searchKey = {0x01, 0x02, 0x03, 0x04};
        auto* found = dynamic_cast<DoubleBoundingBlockHandler*>(
            reg.findLowerBound(searchKey));
        check(found == &handler, "TEST20 handler found");
    }

    printf("\n%s\n", g_fail == 0 ? "HANDLER_REG_OK" : "HANDLER_REG_FAIL");
    return g_fail == 0 ? 0 : 1;
}
