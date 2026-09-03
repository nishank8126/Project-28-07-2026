#include "workstation/cad/AttachmentManager.h"
#include <algorithm>

namespace workstation {
namespace cad {

static std::string getFilename(const std::string& path) {
    auto pos = path.find_last_of("/\\");
    return (pos != std::string::npos) ? path.substr(pos + 1) : path;
}

int AttachmentManager::addDxf(const std::string& filepath, std::string* errorMessage) {
    auto attachment = std::make_unique<DxfAttachment>(filepath);
    if (!attachment->load(errorMessage)) return -1;
    int index = static_cast<int>(m_dxfAttachments.size());
    m_dxfAttachments.push_back(std::move(attachment));
    return index;
}

int AttachmentManager::addDwg(const std::string& filepath, std::string* errorMessage) {
    auto attachment = std::make_unique<DwgAttachment>(filepath);
    if (!attachment->load(errorMessage)) return -1;
    int index = static_cast<int>(m_dwgAttachments.size());
    m_dwgAttachments.push_back(std::move(attachment));
    return index;
}

int AttachmentManager::addSnt(const std::string& filepath, std::string* errorMessage) {
    auto attachment = std::make_unique<SntAttachment>(filepath);
    if (!attachment->load(errorMessage)) return -1;
    int index = static_cast<int>(m_sntAttachments.size());
    m_sntAttachments.push_back(std::move(attachment));
    return index;
}

void AttachmentManager::removeAttachment(int index) {
    int dxfCount = static_cast<int>(m_dxfAttachments.size());
    int dwgCount = static_cast<int>(m_dwgAttachments.size());
    if (index < dxfCount) {
        m_dxfAttachments.erase(m_dxfAttachments.begin() + index);
    } else if (index < dxfCount + dwgCount) {
        m_dwgAttachments.erase(m_dwgAttachments.begin() + (index - dxfCount));
    } else if (index < dxfCount + dwgCount + static_cast<int>(m_sntAttachments.size())) {
        m_sntAttachments.erase(m_sntAttachments.begin() + (index - dxfCount - dwgCount));
    }
}

void AttachmentManager::removeAll() {
    m_dxfAttachments.clear();
    m_dwgAttachments.clear();
    m_sntAttachments.clear();
}

DxfAttachment* AttachmentManager::dxfAttachment(int index) {
    return (index >= 0 && index < static_cast<int>(m_dxfAttachments.size())) ? m_dxfAttachments[index].get() : nullptr;
}

DwgAttachment* AttachmentManager::dwgAttachment(int index) {
    return (index >= 0 && index < static_cast<int>(m_dwgAttachments.size())) ? m_dwgAttachments[index].get() : nullptr;
}

SntAttachment* AttachmentManager::sntAttachment(int index) {
    return (index >= 0 && index < static_cast<int>(m_sntAttachments.size())) ? m_sntAttachments[index].get() : nullptr;
}

std::vector<AttachmentInfo> AttachmentManager::attachments() const {
    std::vector<AttachmentInfo> result;
    int globalIdx = 0;
    for (size_t i = 0; i < m_dxfAttachments.size(); ++i)
        result.push_back({AttachmentInfo::Type::DXF, m_dxfAttachments[i]->filepath(), globalIdx++});
    for (size_t i = 0; i < m_dwgAttachments.size(); ++i)
        result.push_back({AttachmentInfo::Type::DWG, m_dwgAttachments[i]->filepath(), globalIdx++});
    for (size_t i = 0; i < m_sntAttachments.size(); ++i)
        result.push_back({AttachmentInfo::Type::SNT, m_sntAttachments[i]->filepath(), globalIdx++});
    return result;
}

} // namespace cad
} // namespace workstation
