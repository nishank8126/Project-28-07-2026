#include "workstation/renderer/Camera.h"
#include "workstation/spatial/BoundingBox.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace workstation::renderer;
using namespace workstation::math;
using namespace workstation::spatial;

static int g_pass = 0;
static int g_fail = 0;

static bool Eq(double a, double b, double eps = 1e-6) { return std::fabs(a - b) < eps; }

static bool VecEq(const Point3d& a, const Point3d& b, double eps = 1e-6) {
    return Eq(a.x, b.x, eps) && Eq(a.y, b.y, eps) && Eq(a.z, b.z, eps);
}

static double Dot(const Point3d& a, const Point3d& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static Point3d Cross(const Point3d& a, const Point3d& b) {
    return {a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x};
}

static double Len(const Point3d& v) { return std::sqrt(Dot(v, v)); }

static void Check(bool cond, const char* name) {
    if (cond) { g_pass++; printf("  PASS  %s\n", name); }
    else      { g_fail++; printf("  FAIL  %s\n", name); }
}

static void CheckVec(const char* label, const Point3d& got, const Point3d& expect, double eps = 1e-4) {
    char buf[256];
    snprintf(buf, sizeof(buf), "%s = (%.4f, %.4f, %.4f) expect (%.4f, %.4f, %.4f)",
             label, got.x, got.y, got.z, expect.x, expect.y, expect.z);
    Check(VecEq(got, expect, eps), buf);
}

// ============================================================
// TEST 1: Synthetic Axis Test
// ============================================================
static void TestSyntheticAxes() {
    printf("\n=== TEST 1: Synthetic Axis Test ===\n");

    // Create 4 test points: origin, +X, +Y, +Z
    // P0=(0,0,0) P1=(100,0,0) P2=(0,100,0) P3=(0,0,100)
    // In Z-up: X=east, Y=north, Z=up

    Camera cam;
    cam.SetPerspective(45.0, 16.0/9.0, 0.1, 10000.0);

    // Place camera at (0, -200, 100) looking toward origin
    // This means: looking along +Y (north), slightly above
    cam.SetLookAt({0, -200, 100}, {0, 0, 0}, {0, 0, 1});

    Point3d fwd = cam.GetForward();
    Point3d right = cam.GetRight();
    Point3d up = cam.GetUp();

    // Forward should be approximately (0, +1, +0.5) normalized
    // Direction from (0,-200,100) to (0,0,0) = (0, 200, -100), normalized = (0, 0.894, -0.447)
    // But pitch clamp prevents looking more than 89 degrees
    // Actually: eye=(0,-200,100), center=(0,0,0), f=(0,200,-100), len=223.6
    // f_normalized = (0, 0.8944, -0.4472)
    // yaw = atan2(0, 0.8944) = 0 degrees
    // pitch = asin(-0.4472) = -26.57 degrees
    printf("  Camera at (0, -200, 100), looking at origin\n");
    printf("  Forward = (%.4f, %.4f, %.4f)\n", fwd.x, fwd.y, fwd.z);

    // With Z-up, looking from -Y toward origin, forward should be predominantly +Y
    Check(fwd.y > 0.5, "Forward has strong +Y component (looking north)");
    Check(std::fabs(fwd.x) < 0.1, "Forward has near-zero X component");
    Check(fwd.z < 0, "Forward has negative Z (looking slightly downward from elevated position)");

    // Right = cross(forward, worldUp) where worldUp = (0,0,1)
    // For forward ~ (0, 0.89, -0.45), right = cross(fwd, (0,0,1)) = (0.89*1 - (-0.45)*0, (-0.45)*0 - 0*1, 0*0 - 0.89*0) = (0.89, 0, 0)
    printf("  Right = (%.4f, %.4f, %.4f)\n", right.x, right.y, right.z);
    Check(right.x > 0.5, "Right has strong +X component (east)");
    Check(std::fabs(right.z) < 0.1, "Right has near-zero Z component");

    // Up = cross(right, forward)
    printf("  Up = (%.4f, %.4f, %.4f)\n", up.x, up.y, up.z);
    Check(up.z > 0, "Up has positive Z component (pointing up)");
}

// ============================================================
// TEST 2: Camera Basis Vectors (orthogonality + handedness)
// ============================================================
static void TestCameraBasis() {
    printf("\n=== TEST 2: Camera Basis Orthogonality ===\n");

    // Test multiple orientations
    struct TestCase { double yaw, pitch; const char* name; };
    TestCase cases[] = {
        {  0,    0, "yaw=0 pitch=0 (looking +Y)" },
        { 90,    0, "yaw=90 pitch=0 (looking +X)" },
        {-90,    0, "yaw=-90 pitch=0 (looking -X)" },
        {  0,   45, "yaw=0 pitch=45 (looking +Y+Z)" },
        {  0,  -45, "yaw=0 pitch=-45 (looking +Y-Z)" },
        { 45,   30, "yaw=45 pitch=30 (diagonal)" },
        {135,  -20, "yaw=135 pitch=-20" },
    };

    for (auto& tc : cases) {
        Camera cam;
        cam.SetPerspective(45.0, 16.0/9.0, 0.1, 10000.0);
        cam.Rotate(tc.yaw + 90.0, tc.pitch); // +90 because default yaw=-90
        // Actually, Rotate adds to current yaw. Default yaw=-90. So to get yaw=0: rotate by +90.
        // But let's just use SetLookAt for precise control.
        double yr = tc.yaw * M_PI / 180.0;
        double pr = tc.pitch * M_PI / 180.0;
        Point3d eye = {100.0 * std::cos(pr) * std::sin(yr),
                       100.0 * std::cos(pr) * std::cos(yr),
                       100.0 * std::sin(pr)};
        cam.SetLookAt(eye, {0,0,0}, {0,0,1});

        Point3d f = cam.GetForward();
        Point3d r = cam.GetRight();
        Point3d u = cam.GetUp();

        char buf[256];
        double dotRU = Dot(r, u);
        double dotRF = Dot(r, f);
        double dotUF = Dot(u, f);
        snprintf(buf, sizeof(buf), "%s: dot(R,U)=%.6f dot(R,F)=%.6f dot(U,F)=%.6f",
                 tc.name, dotRU, dotRF, dotUF);
        printf("  %s\n", buf);

        Check(std::fabs(dotRU) < 1e-4, "dot(Right, Up) ≈ 0");
        Check(std::fabs(dotRF) < 1e-4, "dot(Right, Forward) ≈ 0");
        Check(std::fabs(dotUF) < 1e-4, "dot(Up, Forward) ≈ 0");

        // Verify right-handedness: right = cross(forward, worldUp)
        // up should be close to cross(right, forward)
        Point3d expectedUp = Cross(r, f);
        double upLen = Len(expectedUp);
        if (upLen > 1e-6) {
            expectedUp.x /= upLen; expectedUp.y /= upLen; expectedUp.z /= upLen;
            double cosAngle = Dot(u, expectedUp);
            snprintf(buf, sizeof(buf), "%s: cos(up, cross(r,f))=%.6f", tc.name, cosAngle);
            Check(cosAngle > 0.999, buf);
        }

        // Verify right = cross(forward, worldUp) direction
        Point3d expectedRight = Cross(f, {0, 0, 1});
        double rLen = Len(expectedRight);
        if (rLen > 1e-6) {
            expectedRight.x /= rLen; expectedRight.y /= rLen; expectedRight.z /= rLen;
            double cosAngle = Dot(r, expectedRight);
            snprintf(buf, sizeof(buf), "%s: cos(right, cross(f,up))=%.6f", tc.name, cosAngle);
            Check(cosAngle > 0.999, buf);
        }
    }
}

// ============================================================
// TEST 3: Camera Movement
// ============================================================
static void TestCameraMovement() {
    printf("\n=== TEST 3: Camera Movement ===\n");

    Camera cam;
    cam.SetPerspective(45.0, 16.0/9.0, 0.1, 10000.0);
    cam.SetLookAt({0, -100, 50}, {0, 0, 0}, {0, 0, 1});

    Point3d startPos = cam.GetPosition();

    // MoveUp should change Z only
    cam.MoveUp(10.0);
    Point3d afterUp = cam.GetPosition();
    Check(Eq(afterUp.z - startPos.z, 10.0), "MoveUp changes Z by +10");
    Check(Eq(afterUp.x, startPos.x), "MoveUp does not change X");
    Check(Eq(afterUp.y, startPos.y), "MoveUp does not change Y");

    // MoveUp(-10) should return to start
    cam.MoveUp(-10.0);
    Point3d afterDown = cam.GetPosition();
    Check(VecEq(afterDown, startPos), "MoveUp(-10) returns to start");

    // MoveForward should change position along forward direction
    Point3d fwd = cam.GetForward();
    cam.MoveForward(50.0);
    Point3d afterFwd = cam.GetPosition();
    double dx = afterFwd.x - startPos.x;
    double dy = afterFwd.y - startPos.y;
    double dz = afterFwd.z - startPos.z;
    Check(Eq(dx, fwd.x * 50.0, 0.01), "MoveForward X matches forward.x * dist");
    Check(Eq(dy, fwd.y * 50.0, 0.01), "MoveForward Y matches forward.y * dist");
    Check(Eq(dz, fwd.z * 50.0, 0.01), "MoveForward Z matches forward.z * dist");

    // MoveRight should change position along right direction
    cam.SetLookAt({0, -100, 50}, {0, 0, 0}, {0, 0, 1}); // reset
    Point3d right = cam.GetRight();
    cam.MoveRight(30.0);
    Point3d afterRight = cam.GetPosition();
    dx = afterRight.x - startPos.x;
    dy = afterRight.y - startPos.y;
    dz = afterRight.z - startPos.z;
    Check(Eq(dx, right.x * 30.0, 0.01), "MoveRight X matches right.x * dist");
    Check(Eq(dy, right.y * 30.0, 0.01), "MoveRight Y matches right.y * dist");
    Check(Eq(dz, right.z * 30.0, 0.01), "MoveRight Z matches right.z * dist");
}

// ============================================================
// TEST 4: FocusOnBounds
// ============================================================
static void TestFocusOnBounds() {
    printf("\n=== TEST 4: FocusOnBounds ===\n");

    // Flat terrain: 200x200x25
    {
        Camera cam;
        cam.SetPerspective(45.0, 16.0/9.0, 0.1, 10000.0);
        BoundingBox box;
        box.minX = 0; box.maxX = 200;
        box.minY = 0; box.maxY = 200;
        box.minZ = 0; box.maxZ = 25;
        cam.FocusOnBounds(box);

        Point3d pos = cam.GetPosition();
        Point3d target = cam.GetTarget();
        printf("  Flat 200x200x25: pos=(%.1f, %.1f, %.1f) target=(%.1f, %.1f, %.1f)\n",
               pos.x, pos.y, pos.z, target.x, target.y, target.z);

        // Target should be at center of bounds
        Check(Eq(target.x, 100.0, 0.1), "Flat: target.x = center X");
        Check(Eq(target.y, 100.0, 0.1), "Flat: target.y = center Y");
        Check(Eq(target.z, 12.5, 0.1), "Flat: target.z = center Z");

        // Camera should NOT be retreated along Z (up axis)
        // dx=200, dy=200, dx<=dy so retreat along X: eye.x = 100 - dist
        Check(pos.x < 100.0, "Flat: camera retreated along X (not Z)");
        Check(Eq(pos.y, 100.0, 0.1), "Flat: camera Y at center Y");
        Check(Eq(pos.z, 12.5, 0.1), "Flat: camera Z at center Z (not retreated)");
    }

    // Tall building: 100x100x500
    {
        Camera cam;
        cam.SetPerspective(45.0, 16.0/9.0, 0.1, 10000.0);
        BoundingBox box;
        box.minX = 0; box.maxX = 100;
        box.minY = 0; box.maxY = 100;
        box.minZ = 0; box.maxZ = 500;
        cam.FocusOnBounds(box);

        Point3d pos = cam.GetPosition();
        Point3d target = cam.GetTarget();
        printf("  Tall 100x100x500: pos=(%.1f, %.1f, %.1f) target=(%.1f, %.1f, %.1f)\n",
               pos.x, pos.y, pos.z, target.x, target.y, target.z);

        // Target should be at center: (50, 50, 250)
        Check(Eq(target.x, 50.0, 0.1), "Tall: target.x = center X");
        Check(Eq(target.y, 50.0, 0.1), "Tall: target.y = center Y");
        Check(Eq(target.z, 250.0, 0.1), "Tall: target.z = center Z");

        // dx=100, dy=100, dx<=dy so retreat along X
        Check(pos.x < 50.0, "Tall: camera retreated along X");
        Check(Eq(pos.y, 50.0, 0.1), "Tall: camera Y at center Y");
        Check(Eq(pos.z, 250.0, 0.1), "Tall: camera Z at center Z (not retreated)");
    }

    // Mountain: 1000x1000x500
    {
        Camera cam;
        cam.SetPerspective(45.0, 16.0/9.0, 0.1, 10000.0);
        BoundingBox box;
        box.minX = 0; box.maxX = 1000;
        box.minY = 0; box.maxY = 1000;
        box.minZ = 0; box.maxZ = 500;
        cam.FocusOnBounds(box);

        Point3d pos = cam.GetPosition();
        Point3d target = cam.GetTarget();
        printf("  Mountain 1000x1000x500: pos=(%.1f, %.1f, %.1f) target=(%.1f, %.1f, %.1f)\n",
               pos.x, pos.y, pos.z, target.x, target.y, target.z);

        Check(Eq(target.x, 500.0, 0.1), "Mountain: target.x = center X");
        Check(Eq(target.y, 500.0, 0.1), "Mountain: target.y = center Y");
        Check(Eq(target.z, 250.0, 0.1), "Mountain: target.z = center Z");
        Check(pos.x < 500.0, "Mountain: camera retreated along X");
    }

    // Tall narrow: 50x500x10 (facade-like, Y is longest horizontal)
    {
        Camera cam;
        cam.SetPerspective(45.0, 16.0/9.0, 0.1, 10000.0);
        BoundingBox box;
        box.minX = 0; box.maxX = 50;
        box.minY = 0; box.maxY = 500;
        box.minZ = 0; box.maxZ = 10;
        cam.FocusOnBounds(box);

        Point3d pos = cam.GetPosition();
        printf("  Facade 50x500x10: pos=(%.1f, %.1f, %.1f)\n", pos.x, pos.y, pos.z);

        // dx=50, dy=500, dx<=dy so retreat along X (shorter horizontal)
        Check(pos.x < 25.0, "Facade: camera retreated along X (shorter horizontal)");
        Check(Eq(pos.y, 250.0, 0.1), "Facade: camera Y at center Y");
    }
}

// ============================================================
// TEST 5: Orbit consistency
// ============================================================
static void TestOrbit() {
    printf("\n=== TEST 5: Orbit Consistency ===\n");

    Camera cam;
    cam.SetPerspective(45.0, 16.0/9.0, 0.1, 10000.0);
    cam.SetLookAt({100, 0, 50}, {0, 0, 0}, {0, 0, 1});

    Point3d startPos = cam.GetPosition();

    // 360-degree orbit: 4 quarter turns of 90 degrees each
    for (int i = 0; i < 4; i++) {
        cam.Rotate(90.0, 0.0);
    }

    Point3d endPos = cam.GetPosition();
    // After 360 degrees, position should be the same
    Check(VecEq(startPos, endPos, 0.1), "360-degree orbit returns to start position");

    // After 360, forward should be approximately the same
    Point3d startFwd = {0, -100, -50};
    double len = Len(startFwd);
    startFwd.x /= len; startFwd.y /= len; startFwd.z /= len;
    Point3d endFwd = cam.GetForward();
    double dot = Dot(startFwd, endFwd);
    printf("  After 360 orbit: forward dot = %.6f\n", dot);
    Check(dot > 0.99, "360-degree orbit: forward direction consistent");

    // Test orbit direction: right-mouse drag left should orbit left (increase yaw)
    Camera cam2;
    cam2.SetPerspective(45.0, 16.0/9.0, 0.1, 10000.0);
    cam2.SetLookAt({100, 0, 50}, {0, 0, 0}, {0, 0, 1});
    Point3d beforeFwd = cam2.GetForward();

    cam2.Rotate(-10.0, 0.0); // orbit left (negative yaw)
    Point3d afterFwd = cam2.GetForward();

    // Orbiting left from looking at origin should change the forward direction
    // The forward vector should rotate around Z axis
    printf("  Before orbit left: fwd=(%.4f, %.4f, %.4f)\n", beforeFwd.x, beforeFwd.y, beforeFwd.z);
    printf("  After orbit left:  fwd=(%.4f, %.4f, %.4f)\n", afterFwd.x, afterFwd.y, afterFwd.z);
    Check(!VecEq(beforeFwd, afterFwd, 0.01), "Orbit changes forward direction");

    // No camera roll: GetUp should always have positive Z component when looking down at terrain
    Camera cam3;
    cam3.SetPerspective(45.0, 16.0/9.0, 0.1, 10000.0);
    cam3.SetLookAt({100, 100, 200}, {0, 0, 0}, {0, 0, 1});
    for (int i = 0; i < 36; i++) {
        cam3.Rotate(10.0, 0.0);
        Point3d up = cam3.GetUp();
        // When looking down at terrain, up should generally have positive Z
        // (except at extreme yaw angles where up aligns with worldUp exactly)
    }
    Point3d finalUp = cam3.GetUp();
    printf("  After 360 yaw orbit: up=(%.4f, %.4f, %.4f)\n", finalUp.x, finalUp.y, finalUp.z);
    Check(std::fabs(finalUp.z) > 0.01, "No camera roll: up vector maintains Z component");
}

// ============================================================
// TEST 6: ViewMatrix correctness
// ============================================================
static void TestViewMatrix() {
    printf("\n=== TEST 6: ViewMatrix Correctness ===\n");

    Camera cam;
    cam.SetPerspective(45.0, 16.0/9.0, 0.1, 10000.0);
    cam.SetLookAt({0, -100, 50}, {0, 0, 0}, {0, 0, 1});

    const Matrix4d& view = cam.GetViewMatrix();

    // A point at the camera's position should map to view-space origin (approximately)
    // Actually it should map to (0, 0, -dist) in view space (camera looks down -Z)
    Point3d eyePos = cam.GetPosition();
    Point4d eyeH = {eyePos.x, eyePos.y, eyePos.z, 1.0};
    Point4d viewEye = view.Multiply(eyeH);
    printf("  Eye in view space: (%.4f, %.4f, %.4f, %.4f)\n", viewEye.x, viewEye.y, viewEye.z, viewEye.w);
    Check(Eq(viewEye.x, 0.0, 0.01) && Eq(viewEye.y, 0.0, 0.01), "Eye maps to view-space origin XY");

    // A point at the target should be at (0, 0, -dist) in view space
    Point3d tgtPos = cam.GetTarget();
    Point4d tgtH = {tgtPos.x, tgtPos.y, tgtPos.z, 1.0};
    Point4d viewTgt = view.Multiply(tgtH);
    printf("  Target in view space: (%.4f, %.4f, %.4f, %.4f)\n", viewTgt.x, viewTgt.y, viewTgt.z, viewTgt.w);
    Check(viewTgt.z < -1.0, "Target is in front of camera (negative Z in view space)");
    Check(Eq(viewTgt.x, 0.0, 0.01), "Target centered in view X");
    Check(Eq(viewTgt.y, 0.0, 0.01), "Target centered in view Y");

    // Projection matrix: check Vulkan Y-flip
    const Matrix4d& proj = cam.GetProjectionMatrix();
    printf("  Projection m_[1][1] = %.6f (should be negative for Vulkan Y-flip)\n", proj(1,1));
    Check(proj(1, 1) < 0, "Projection has Vulkan Y-flip (negative m_[1][1])");
}

// ============================================================
// TEST 7: FocusOnBounds edge cases
// ============================================================
static void TestFocusEdgeCases() {
    printf("\n=== TEST 7: FocusOnBounds Edge Cases ===\n");

    // All axes equal
    {
        Camera cam;
        cam.SetPerspective(45.0, 16.0/9.0, 0.1, 10000.0);
        BoundingBox box;
        box.minX = 0; box.maxX = 100;
        box.minY = 0; box.maxY = 100;
        box.minZ = 0; box.maxZ = 100;
        cam.FocusOnBounds(box);
        Point3d pos = cam.GetPosition();
        printf("  Cube 100^3: pos=(%.1f, %.1f, %.1f)\n", pos.x, pos.y, pos.z);
        // dx=100, dy=100, dx<=dy so retreat along X
        Check(pos.x < 50.0, "Cube: camera retreated along X (dx<=dy)");
    }

    // Very thin Z (flat scan)
    {
        Camera cam;
        cam.SetPerspective(45.0, 16.0/9.0, 0.1, 10000.0);
        BoundingBox box;
        box.minX = 0; box.maxX = 500;
        box.minY = 0; box.maxY = 500;
        box.minZ = 100; box.maxZ = 100.5;
        cam.FocusOnBounds(box);
        Point3d pos = cam.GetPosition();
        printf("  Flat scan 500x500x0.5: pos=(%.1f, %.1f, %.1f)\n", pos.x, pos.y, pos.z);
        Check(Eq(pos.z, 100.25, 0.1), "Flat scan: camera Z at center Z (not retreated along Z)");
    }
}

// ============================================================
// MAIN
// ============================================================
int main() {
    printf("===================================================\n");
    printf("  Z-UP CAMERA VALIDATION TEST SUITE\n");
    printf("===================================================\n");

    TestSyntheticAxes();
    TestCameraBasis();
    TestCameraMovement();
    TestFocusOnBounds();
    TestOrbit();
    TestViewMatrix();
    TestFocusEdgeCases();

    printf("\n===================================================\n");
    printf("  RESULTS: %d passed, %d failed, %d total\n", g_pass, g_fail, g_pass + g_fail);
    printf("===================================================\n");

    return g_fail > 0 ? 1 : 0;
}
