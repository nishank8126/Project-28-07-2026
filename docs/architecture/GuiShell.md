# GUI Shell 1 (Qt 6 Widgets)

GUI Shell 1 is **implementation mechanics only**. It establishes a real,
launchable desktop CAD-style application shell (`wk_gui.exe`) without
implementing any CAD, geometry, view, camera, or rendering algorithm.

This document is intentionally **not** an Evidence Ledger math entry. No
reverse-engineered CAD behavior is introduced here.

## Technology

- Qt 6 (Widgets module only, no QML)
- C++17
- `QMainWindow` application shell

## Window ownership

`MainWindow` (derives `QMainWindow`) owns the full window:

- Menu bar: `File`, `Edit`, `View`, `Tools`, `Help`
  - `File > Open...` is present but **disabled** (no file I/O in GUI Shell 1)
  - `File > Exit` closes the window
  - `View` toggles dock visibility
  - `Help > About` shows diagnostic information (Qt version, renderer state)
- Main toolbar (Exit action)
- Status bar: `Ready` plus a permanent `Renderer: not connected` label
- Central widget: `ViewportWidget`
- Dock widgets:
  - Left: `ModelTreeWidget` ("Model Tree")
  - Right: `PropertiesWidget` ("Properties")
  - Bottom: `ToolSettingsWidget` ("Tool Settings / Output")

## Dock layout

```
+-------------------------------------------------------------+
| Menu Bar                                                    |
+-------------------------------------------------------------+
| Toolbar                                                     |
+---------------+-----------------------------+---------------+
|               |                             |               |
| Model Tree    |       VIEWPORT              | Properties    |
|               |                             |               |
|               | Renderer not connected yet  |               |
|               |                             |               |
+---------------+-----------------------------+---------------+
| Tool Settings / Output                                     |
+-------------------------------------------------------------+
| Status Bar                                                  |
+-------------------------------------------------------------+
```

## ViewportWidget role

`ViewportWidget` (derives `QWidget`) reserves the central CAD viewport area:

- Sets `Qt::WA_NativeWindow` so a real native window surface exists.
- Exposes `HWND NativeHandle() const` (Windows) via `winId()` for a future
  D3D11 renderer to attach to.
- `paintEvent` draws **UI-only placeholder text** ("WorkstationCAD Viewport /
  Renderer not connected / GUI Shell 1"). It does **not** draw grids, axes,
  cubes, meshes, CAD entities, or any fake geometry.

## Native HWND boundary

The native window handle is available through `ViewportWidget::NativeHandle()`
on Windows. This is the clean boundary where a future D3D11 viewport would be
created. GUI Shell 1 creates **no** D3D11 device, swap chain, or shaders.

## Renderer intentionally absent

There is **no** renderer object in GUI Shell 1. Existing renderer-neutral
interfaces (`workstation::ViewContext`, `workstation::IViewOutput`) already
exist in the core, but GUI Shell 1 holds none of them with behavior. The
future flow is intended to become:

```
ViewportWidget
    -> View / ViewContext
    -> retained scene
    -> IViewOutput
    -> renderer
    -> D3D11
```

GUI Shell 1 **stops at `ViewportWidget`**.

## Camera / view algorithms intentionally absent

No camera, projection matrix, view matrix, pan/orbit/zoom, selection,
hit-testing, snapping, clipping, tessellation, or mesh/grid/axis drawing is
implemented. Cursor/world coordinate conversion is not reverse engineered, so
the status bar shows no coordinates.

## Existing core integration

`wk_gui` links the existing `workstation` core library. `MainWindow` holds an
empty `Document` only to mark the architectural connection point; no document
is loaded and no CAD elements are fabricated. The Model Tree shows the truthful
"No document loaded" state.

## Deliberately not implemented

D3D11 renderer, swap chain, shaders, camera, projection, view matrices,
clipping, tessellation, mesh/grid/axis drawing, selection, hit testing,
snapping, AccuDraw, CAD tools, file import, DGN/DWG reader, point cloud,
raster, materials, lighting, Python.
