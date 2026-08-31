#pragma once
#include "workstation/display/ElemDisplayParams.h"
#include "workstation/display/IViewOutput.h"
#include "workstation/display/ElementRenderOverrides.h"

namespace workstation {

class GraphicsRecorder;

// Minimal renderer-independent ViewContext for Pieces 2-3. It holds only the
// state required by graphics generation / cache recording (Piece 2) and
// retained-graphics submission (Piece 3):
//   - recording flag + active recorder (cache generation, RE K)
//   - current element display parameters (saved/restored around generation)
//   - output sink for retained submission (Piece 3, RE: DrawQvElem/IViewOutput)
//   - current render overrides (temporary symbology state, RE: ElemMatSymb)
//   - retainedSubmissionParameter: the additional ViewContext-derived scalar
//     passed by one _DrawQvElem output path. Its proprietary MEANING is
//     UNKNOWN; the name is neutral (RE).
// No camera matrices, projection, viewport, HWND, or D3D state yet.
class ViewContext {
public:
    bool               isRecordingCachedGraphics = false;
    GraphicsRecorder*  activeRecorder = nullptr;
    ElemDisplayParams  currentDisplayParams;
    double             currentViewMetric = 1.0;

    IViewOutput*       output = nullptr;
    ElementRenderOverrides currentRenderOverrides;
    double             retainedSubmissionParameter = 0.0;
};

} // namespace workstation
