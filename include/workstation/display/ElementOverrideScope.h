#pragma once
#include "workstation/display/ViewContext.h"
#include "workstation/display/IViewOutput.h"
#include "workstation/display/ElementRenderOverrides.h"

namespace workstation {

// RAII wrapper around temporary element render overrides for one retained
// submission (RE: DrawQvElem copies/activates/restores override state).
// If the supplied temporary overrides differ from the current ViewContext
// overrides, they are applied to the output; on destruction the ORIGINAL
// overrides are restored on both the ViewContext and the output. Exception
// safe. Copy/move disabled.
class ElementOverrideScope {
    ViewContext&            m_view;
    IViewOutput*           m_output;
    ElementRenderOverrides m_original;
    bool                   m_active = false;

public:
    ElementOverrideScope(ViewContext& view, const ElementRenderOverrides& temporary)
        : m_view(view), m_output(view.output),
          m_original(view.currentRenderOverrides) {
        if (temporary != m_original) {
            m_view.currentRenderOverrides = temporary;
            m_output->ApplyElementOverrides(temporary);
            m_active = true;
        }
    }

    ~ElementOverrideScope() {
        if (!m_active) return;
        m_view.currentRenderOverrides = m_original;
        m_output->ApplyElementOverrides(m_original);
        m_active = false;
    }

    ElementOverrideScope(const ElementOverrideScope&) = delete;
    ElementOverrideScope& operator=(const ElementOverrideScope&) = delete;
    ElementOverrideScope(ElementOverrideScope&&) = delete;
    ElementOverrideScope& operator=(ElementOverrideScope&&) = delete;
};

} // namespace workstation
