#pragma once
#include "workstation/cad/DxfAttachment.h"
#include "workstation/cad/DwgAttachment.h"
#include "workstation/cad/SntAttachment.h"
#include "workstation/cad/ShadingDisplay.h"

#include <string>
#include <vector>
#include <memory>

namespace workstation {
namespace cad {

struct AttachmentInfo {
    enum class Type { DXF, DWG, SNT };
    Type type;
    std::string filepath;
    int index = -1;
};

class AttachmentManager {
public:
    AttachmentManager() = default;
    ~AttachmentManager() = default;

    int addDxf(const std::string& filepath, std::string* errorMessage = nullptr);
    int addDwg(const std::string& filepath, std::string* errorMessage = nullptr);
    int addSnt(const std::string& filepath, std::string* errorMessage = nullptr);

    void removeAttachment(int index);
    void removeAll();

    DxfAttachment* dxfAttachment(int index);
    DwgAttachment* dwgAttachment(int index);
    SntAttachment* sntAttachment(int index);

    int attachmentCount() const { return static_cast<int>(m_dxfAttachments.size() + m_dwgAttachments.size() + m_sntAttachments.size()); }
    std::vector<AttachmentInfo> attachments() const;

    ShadingDisplay* shadingDisplay() { return &m_shading; }

    int dxfCount() const { return static_cast<int>(m_dxfAttachments.size()); }
    int dwgCount() const { return static_cast<int>(m_dwgAttachments.size()); }
    int sntCount() const { return static_cast<int>(m_sntAttachments.size()); }

private:
    std::vector<std::unique_ptr<DxfAttachment>> m_dxfAttachments;
    std::vector<std::unique_ptr<DwgAttachment>> m_dwgAttachments;
    std::vector<std::unique_ptr<SntAttachment>> m_sntAttachments;
    ShadingDisplay m_shading;
};

} // namespace cad
} // namespace workstation
