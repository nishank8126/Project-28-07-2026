#include "workstation/renderer/CadRenderer.h"
#include "workstation/cad/AciColorTable.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace workstation {
namespace renderer {

CadRenderer::~CadRenderer() { Shutdown(); }

bool CadRenderer::Initialize(vulkan::VulkanAllocator* allocator) {
    m_allocator = allocator;
    return true;
}

void CadRenderer::Shutdown() {
    ClearBuffers();
    m_attachments.clear();
    m_allocator = nullptr;
}

void CadRenderer::SetCoordinateNormalizer(spatial::CoordinateNormalizationManager* normalizer) {
    m_normalizer = normalizer;
}

void CadRenderer::LoadDxfAttachment(cad::DxfAttachment* attachment) {
    if (!attachment) return;
    for (const auto& a : m_attachments) {
        if (a.dxf == attachment) return;
    }
    BuildGeometry(attachment);
}

void CadRenderer::LoadDwgAttachment(cad::DwgAttachment* attachment) {
    if (!attachment) return;
    for (const auto& a : m_attachments) {
        if (a.dwg == attachment) return;
    }
    BuildGeometry(attachment);
}

void CadRenderer::LoadSntAttachment(cad::SntAttachment* attachment) {
    if (!attachment) return;
    for (const auto& a : m_attachments) {
        if (a.snt == attachment) return;
    }
    BuildGeometry(attachment);
}

void CadRenderer::RemoveAttachment(cad::DxfAttachment* attachment) {
    for (auto it = m_attachments.begin(); it != m_attachments.end(); ++it) {
        if (it->dxf == attachment) {
            m_attachments.erase(it);
            m_buffersDirty = true;
            RebuildAllGeometry();
            return;
        }
    }
}

void CadRenderer::RemoveAttachment(cad::DwgAttachment* attachment) {
    for (auto it = m_attachments.begin(); it != m_attachments.end(); ++it) {
        if (it->dwg == attachment) {
            m_attachments.erase(it);
            m_buffersDirty = true;
            RebuildAllGeometry();
            return;
        }
    }
}

void CadRenderer::RemoveAttachment(cad::SntAttachment* attachment) {
    for (auto it = m_attachments.begin(); it != m_attachments.end(); ++it) {
        if (it->snt == attachment) {
            m_attachments.erase(it);
            m_buffersDirty = true;
            RebuildAllGeometry();
            return;
        }
    }
}

void CadRenderer::RemoveAllAttachments() {
    m_attachments.clear();
    ClearBuffers();
}

void CadRenderer::SetLayerVisibility(const std::string& layerName, bool visible) {
    for (auto& att : m_attachments) {
        cad::LayerManager* lm = nullptr;
        if (att.dxf) lm = att.dxf->layerManager();
        else if (att.dwg) lm = att.dwg->layerManager();
        else if (att.snt) lm = att.snt->layerManager();
        if (lm) lm->setLayerVisible(layerName, visible);
    }
    m_buffersDirty = true;
    RebuildAllGeometry();
}

bool CadRenderer::IsLayerVisible(const std::string& layerName) const {
    for (const auto& att : m_attachments) {
        const cad::LayerManager* lm = nullptr;
        if (att.dxf) lm = att.dxf->layerManager();
        else if (att.dwg) lm = att.dwg->layerManager();
        else if (att.snt) lm = att.snt->layerManager();
        if (lm && !lm->isLayerVisible(layerName)) return false;
    }
    return true;
}

void CadRenderer::BuildGeometry(cad::DxfAttachment* attachment) {
    if (!attachment || !attachment->isLoaded()) return;

    auto geom = attachment->buildGeometry();

    AttachmentGeometry attGeom;
    attGeom.dxf = attachment;
    attGeom.lineVertexOffset = static_cast<uint32_t>(m_lineVertices.size());
    attGeom.pointVertexOffset = static_cast<uint32_t>(m_pointVertices.size());
    attGeom.triangleIndexOffset = static_cast<uint32_t>(m_triangleIndices.size());

    float zOffset = static_cast<float>(attachment->zOffset());

    for (size_t i = 0; i + 5 < geom.lineVertices.size(); i += 3) {
        CadLineVertex v;
        v.position[0] = geom.lineVertices[i];
        v.position[1] = geom.lineVertices[i + 1];
        v.position[2] = geom.lineVertices[i + 2] + zOffset;
        size_t colorIdx = (i / 3) * 3;
        if (colorIdx + 2 < geom.lineColors.size()) {
            v.color[0] = geom.lineColors[colorIdx];
            v.color[1] = geom.lineColors[colorIdx + 1];
            v.color[2] = geom.lineColors[colorIdx + 2];
        } else {
            v.color[0] = 1.0f; v.color[1] = 0.85f; v.color[2] = 0.2f;
        }
        m_lineVertices.push_back(v);
    }

    size_t pointCount = geom.pointVertices.size() / 3;
    for (size_t i = 0; i < pointCount; ++i) {
        CadPointVertex v;
        v.position[0] = geom.pointVertices[i * 3];
        v.position[1] = geom.pointVertices[i * 3 + 1];
        v.position[2] = geom.pointVertices[i * 3 + 2] + zOffset;
        size_t colorIdx = i * 3;
        if (colorIdx + 2 < geom.pointColors.size()) {
            v.color[0] = geom.pointColors[colorIdx];
            v.color[1] = geom.pointColors[colorIdx + 1];
            v.color[2] = geom.pointColors[colorIdx + 2];
        } else {
            v.color[0] = 1.0f; v.color[1] = 1.0f; v.color[2] = 1.0f;
        }
        m_pointVertices.push_back(v);
    }

    attGeom.lineVertexCount = static_cast<uint32_t>(m_lineVertices.size()) - attGeom.lineVertexOffset;
    attGeom.pointVertexCount = static_cast<uint32_t>(m_pointVertices.size()) - attGeom.pointVertexOffset;
    attGeom.triangleIndexCount = 0;

    if (!geom.lineVertices.empty()) {
        for (size_t i = 0; i + 2 < geom.lineVertices.size(); i += 3) {
            float x = geom.lineVertices[i], y = geom.lineVertices[i+1], z = geom.lineVertices[i+2] + zOffset;
            attGeom.bounds.minX = std::min(attGeom.bounds.minX, static_cast<double>(x));
            attGeom.bounds.maxX = std::max(attGeom.bounds.maxX, static_cast<double>(x));
            attGeom.bounds.minY = std::min(attGeom.bounds.minY, static_cast<double>(y));
            attGeom.bounds.maxY = std::max(attGeom.bounds.maxY, static_cast<double>(y));
            attGeom.bounds.minZ = std::min(attGeom.bounds.minZ, static_cast<double>(z));
            attGeom.bounds.maxZ = std::max(attGeom.bounds.maxZ, static_cast<double>(z));
        }
    }

    m_attachments.push_back(attGeom);
    m_buffersDirty = true;
}

void CadRenderer::BuildGeometry(cad::DwgAttachment* attachment) {
    if (!attachment || !attachment->isLoaded()) return;

    auto geom = attachment->buildGeometry();

    AttachmentGeometry attGeom;
    attGeom.dwg = attachment;
    attGeom.lineVertexOffset = static_cast<uint32_t>(m_lineVertices.size());
    attGeom.pointVertexOffset = static_cast<uint32_t>(m_pointVertices.size());

    for (size_t i = 0; i + 5 < geom.lineVertices.size(); i += 3) {
        CadLineVertex v;
        v.position[0] = geom.lineVertices[i];
        v.position[1] = geom.lineVertices[i + 1];
        v.position[2] = geom.lineVertices[i + 2];
        size_t colorIdx = (i / 3) * 3;
        if (colorIdx + 2 < geom.lineColors.size()) {
            v.color[0] = geom.lineColors[colorIdx];
            v.color[1] = geom.lineColors[colorIdx + 1];
            v.color[2] = geom.lineColors[colorIdx + 2];
        } else {
            v.color[0] = 0.0f; v.color[1] = 0.8f; v.color[2] = 1.0f;
        }
        m_lineVertices.push_back(v);
    }

    size_t pointCount = geom.pointVertices.size() / 3;
    for (size_t i = 0; i < pointCount; ++i) {
        CadPointVertex v;
        v.position[0] = geom.pointVertices[i * 3];
        v.position[1] = geom.pointVertices[i * 3 + 1];
        v.position[2] = geom.pointVertices[i * 3 + 2];
        size_t colorIdx = i * 3;
        if (colorIdx + 2 < geom.pointColors.size()) {
            v.color[0] = geom.pointColors[colorIdx];
            v.color[1] = geom.pointColors[colorIdx + 1];
            v.color[2] = geom.pointColors[colorIdx + 2];
        } else {
            v.color[0] = 1.0f; v.color[1] = 1.0f; v.color[2] = 1.0f;
        }
        m_pointVertices.push_back(v);
    }

    attGeom.lineVertexCount = static_cast<uint32_t>(m_lineVertices.size()) - attGeom.lineVertexOffset;
    attGeom.pointVertexCount = static_cast<uint32_t>(m_pointVertices.size()) - attGeom.pointVertexOffset;
    attGeom.triangleIndexCount = 0;

    m_attachments.push_back(attGeom);
    m_buffersDirty = true;
}

void CadRenderer::BuildGeometry(cad::SntAttachment* attachment) {
    if (!attachment || !attachment->isLoaded()) return;

    AttachmentGeometry attGeom;
    attGeom.snt = attachment;
    attGeom.lineVertexOffset = static_cast<uint32_t>(m_lineVertices.size());
    attGeom.pointVertexOffset = static_cast<uint32_t>(m_pointVertices.size());

    float zOffset = static_cast<float>(attachment->zOffset());
    const auto& doc = attachment->document();

    for (const auto& entity : doc.entities) {
        if (!attachment->layerManager()->isLayerVisible(entity.layer)) continue;

        float r = entity.colorR / 255.0f;
        float g = entity.colorG / 255.0f;
        float b = entity.colorB / 255.0f;

        if (entity.type == cad::SntEntity::Polyline && entity.vertices.size() >= 2) {
            for (size_t i = 0; i + 1 < entity.vertices.size(); ++i) {
                CadLineVertex v0;
                v0.position[0] = static_cast<float>(entity.vertices[i][0]);
                v0.position[1] = static_cast<float>(entity.vertices[i][1]);
                v0.position[2] = static_cast<float>(entity.vertices[i][2]) + zOffset;
                v0.color[0] = r; v0.color[1] = g; v0.color[2] = b;
                m_lineVertices.push_back(v0);

                CadLineVertex v1;
                v1.position[0] = static_cast<float>(entity.vertices[i + 1][0]);
                v1.position[1] = static_cast<float>(entity.vertices[i + 1][1]);
                v1.position[2] = static_cast<float>(entity.vertices[i + 1][2]) + zOffset;
                v1.color[0] = r; v1.color[1] = g; v1.color[2] = b;
                m_lineVertices.push_back(v1);
            }
            if (entity.closed && entity.vertices.size() > 2) {
                CadLineVertex v0;
                v0.position[0] = static_cast<float>(entity.vertices.back()[0]);
                v0.position[1] = static_cast<float>(entity.vertices.back()[1]);
                v0.position[2] = static_cast<float>(entity.vertices.back()[2]) + zOffset;
                v0.color[0] = r; v0.color[1] = g; v0.color[2] = b;
                m_lineVertices.push_back(v0);

                CadLineVertex v1;
                v1.position[0] = static_cast<float>(entity.vertices.front()[0]);
                v1.position[1] = static_cast<float>(entity.vertices.front()[1]);
                v1.position[2] = static_cast<float>(entity.vertices.front()[2]) + zOffset;
                v1.color[0] = r; v1.color[1] = g; v1.color[2] = b;
                m_lineVertices.push_back(v1);
            }
        }
        else if (entity.type == cad::SntEntity::Circle) {
            auto circleEnts = cad::SntAttachment::generateCircleVertices(entity.center, entity.radius, 64);
            for (const auto& ce : circleEnts) {
                for (const auto& v : ce.vertices) {
                    CadLineVertex lv;
                    lv.position[0] = static_cast<float>(v[0]);
                    lv.position[1] = static_cast<float>(v[1]);
                    lv.position[2] = static_cast<float>(v[2]) + zOffset;
                    lv.color[0] = r; lv.color[1] = g; lv.color[2] = b;
                    m_lineVertices.push_back(lv);
                }
            }
        }
        else if (entity.type == cad::SntEntity::Arc) {
            auto arcEnts = cad::SntAttachment::generateArcVertices(entity.center, entity.radius, entity.startAngle, entity.endAngle, 32);
            for (const auto& ae : arcEnts) {
                for (const auto& v : ae.vertices) {
                    CadLineVertex lv;
                    lv.position[0] = static_cast<float>(v[0]);
                    lv.position[1] = static_cast<float>(v[1]);
                    lv.position[2] = static_cast<float>(v[2]) + zOffset;
                    lv.color[0] = r; lv.color[1] = g; lv.color[2] = b;
                    m_lineVertices.push_back(lv);
                }
            }
        }
        else if (entity.type == cad::SntEntity::ThreeDFace && entity.vertices.size() >= 3) {
            for (size_t i = 0; i < entity.vertices.size(); ++i) {
                size_t next = (i + 1) % entity.vertices.size();
                CadLineVertex v0;
                v0.position[0] = static_cast<float>(entity.vertices[i][0]);
                v0.position[1] = static_cast<float>(entity.vertices[i][1]);
                v0.position[2] = static_cast<float>(entity.vertices[i][2]) + zOffset;
                v0.color[0] = r; v0.color[1] = g; v0.color[2] = b;
                m_lineVertices.push_back(v0);

                CadLineVertex v1;
                v1.position[0] = static_cast<float>(entity.vertices[next][0]);
                v1.position[1] = static_cast<float>(entity.vertices[next][1]);
                v1.position[2] = static_cast<float>(entity.vertices[next][2]) + zOffset;
                v1.color[0] = r; v1.color[1] = g; v1.color[2] = b;
                m_lineVertices.push_back(v1);
            }
        }
        else if (entity.type == cad::SntEntity::Text && !entity.vertices.empty()) {
            CadPointVertex v;
            v.position[0] = static_cast<float>(entity.vertices[0][0]);
            v.position[1] = static_cast<float>(entity.vertices[0][1]);
            v.position[2] = static_cast<float>(entity.vertices[0][2]) + zOffset;
            v.color[0] = r; v.color[1] = g; v.color[2] = b;
            m_pointVertices.push_back(v);
        }
    }

    attGeom.lineVertexCount = static_cast<uint32_t>(m_lineVertices.size()) - attGeom.lineVertexOffset;
    attGeom.pointVertexCount = static_cast<uint32_t>(m_pointVertices.size()) - attGeom.pointVertexOffset;
    attGeom.triangleIndexCount = 0;

    // Compute the true (raw, world-space) bounds from what was just pushed,
    // starting from the first vertex rather than the default {0,0,0} box -
    // SNT survey coordinates are commonly real UTM/State Plane eastings and
    // northings far from the origin, so seeding at {0,0,0} would silently
    // stretch the box back to include world origin and misreport where the
    // attachment actually sits.
    bool boundsSet = false;
    auto expand = [&](float x, float y, float z) {
        if (!boundsSet) {
            attGeom.bounds.minX = attGeom.bounds.maxX = x;
            attGeom.bounds.minY = attGeom.bounds.maxY = y;
            attGeom.bounds.minZ = attGeom.bounds.maxZ = z;
            boundsSet = true;
        } else {
            attGeom.bounds.minX = std::min(attGeom.bounds.minX, static_cast<double>(x));
            attGeom.bounds.maxX = std::max(attGeom.bounds.maxX, static_cast<double>(x));
            attGeom.bounds.minY = std::min(attGeom.bounds.minY, static_cast<double>(y));
            attGeom.bounds.maxY = std::max(attGeom.bounds.maxY, static_cast<double>(y));
            attGeom.bounds.minZ = std::min(attGeom.bounds.minZ, static_cast<double>(z));
            attGeom.bounds.maxZ = std::max(attGeom.bounds.maxZ, static_cast<double>(z));
        }
    };
    for (uint32_t i = attGeom.lineVertexOffset; i < attGeom.lineVertexOffset + attGeom.lineVertexCount; ++i) {
        expand(m_lineVertices[i].position[0], m_lineVertices[i].position[1], m_lineVertices[i].position[2]);
    }
    for (uint32_t i = attGeom.pointVertexOffset; i < attGeom.pointVertexOffset + attGeom.pointVertexCount; ++i) {
        expand(m_pointVertices[i].position[0], m_pointVertices[i].position[1], m_pointVertices[i].position[2]);
    }

    // Feed this attachment's real-world bounds through the shared coordinate
    // normalizer so raw survey coordinates land near the render origin (float32
    // has ~7 digits of precision; UTM/State Plane values can already use most
    // of that just for the integer part) and so a point cloud loaded afterwards
    // can register against the same shared origin this SNT established.
    if (boundsSet && m_normalizer) {
        if (!m_normalizer->IsNormalized()) {
            m_normalizer->RecomputeFromBoundingBox(attGeom.bounds);
        }
        for (uint32_t i = attGeom.lineVertexOffset; i < attGeom.lineVertexOffset + attGeom.lineVertexCount; ++i) {
            float lx, ly, lz;
            auto& p = m_lineVertices[i].position;
            m_normalizer->WorldToLocal(p[0], p[1], p[2], lx, ly, lz);
            p[0] = lx; p[1] = ly; p[2] = lz;
        }
        for (uint32_t i = attGeom.pointVertexOffset; i < attGeom.pointVertexOffset + attGeom.pointVertexCount; ++i) {
            float lx, ly, lz;
            auto& p = m_pointVertices[i].position;
            m_normalizer->WorldToLocal(p[0], p[1], p[2], lx, ly, lz);
            p[0] = lx; p[1] = ly; p[2] = lz;
        }
        attGeom.bounds = m_normalizer->WorldToLocal(attGeom.bounds);
    }

    m_attachments.push_back(attGeom);
    m_buffersDirty = true;
}

void CadRenderer::RebuildAllGeometry() {
    ClearBuffers();
    m_lineVertices.clear();
    m_pointVertices.clear();
    m_surfaceVertices.clear();
    m_triangleIndices.clear();

    std::vector<AttachmentGeometry> savedAttachments = std::move(m_attachments);
    m_attachments.clear();

    for (auto& att : savedAttachments) {
        if (att.dxf) BuildGeometry(att.dxf);
        else if (att.dwg) BuildGeometry(att.dwg);
        else if (att.snt) BuildGeometry(att.snt);
    }

    UploadBuffers();
}

void CadRenderer::UploadBuffers() {
    if (!m_allocator) return;

    ClearBuffers();

    if (!m_lineVertices.empty()) {
        VkDeviceSize size = m_lineVertices.size() * sizeof(CadLineVertex);
        m_lineVertexBuffer = m_allocator->CreateBuffer(
            size,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VMA_MEMORY_USAGE_AUTO,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
        if (m_lineVertexBuffer.IsValid() && m_lineVertexBuffer.mappedData) {
            memcpy(m_lineVertexBuffer.mappedData, m_lineVertices.data(), static_cast<size_t>(size));
            m_lineVertexBuffer.FlushMapped();
        }
        m_lineVertexCount = static_cast<uint32_t>(m_lineVertices.size());
    }

    if (!m_pointVertices.empty()) {
        VkDeviceSize size = m_pointVertices.size() * sizeof(CadPointVertex);
        m_pointVertexBuffer = m_allocator->CreateBuffer(
            size,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VMA_MEMORY_USAGE_AUTO,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
        if (m_pointVertexBuffer.IsValid() && m_pointVertexBuffer.mappedData) {
            memcpy(m_pointVertexBuffer.mappedData, m_pointVertices.data(), static_cast<size_t>(size));
            m_pointVertexBuffer.FlushMapped();
        }
        m_pointVertexCount = static_cast<uint32_t>(m_pointVertices.size());
    }

    if (!m_surfaceVertices.empty()) {
        VkDeviceSize size = m_surfaceVertices.size() * sizeof(CadSurfaceVertex);
        m_surfaceVertexBuffer = m_allocator->CreateBuffer(
            size,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VMA_MEMORY_USAGE_AUTO,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
        if (m_surfaceVertexBuffer.IsValid() && m_surfaceVertexBuffer.mappedData) {
            memcpy(m_surfaceVertexBuffer.mappedData, m_surfaceVertices.data(), static_cast<size_t>(size));
            m_surfaceVertexBuffer.FlushMapped();
        }
        m_triangleVertexCount = static_cast<uint32_t>(m_surfaceVertices.size());
    }

    if (!m_triangleIndices.empty()) {
        VkDeviceSize size = m_triangleIndices.size() * sizeof(uint32_t);
        m_indexBuffer = m_allocator->CreateBuffer(
            size,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VMA_MEMORY_USAGE_AUTO,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
        if (m_indexBuffer.IsValid() && m_indexBuffer.mappedData) {
            memcpy(m_indexBuffer.mappedData, m_triangleIndices.data(), static_cast<size_t>(size));
            m_indexBuffer.FlushMapped();
        }
        m_triangleIndexCount = static_cast<uint32_t>(m_triangleIndices.size());
    }

    m_buffersDirty = false;
}

void CadRenderer::ClearBuffers() {
    if (m_allocator) {
        if (m_lineVertexBuffer.IsValid()) m_allocator->DestroyBuffer(m_lineVertexBuffer);
        if (m_pointVertexBuffer.IsValid()) m_allocator->DestroyBuffer(m_pointVertexBuffer);
        if (m_surfaceVertexBuffer.IsValid()) m_allocator->DestroyBuffer(m_surfaceVertexBuffer);
        if (m_indexBuffer.IsValid()) m_allocator->DestroyBuffer(m_indexBuffer);
    }
    m_lineVertexBuffer = {};
    m_pointVertexBuffer = {};
    m_surfaceVertexBuffer = {};
    m_indexBuffer = {};
    m_lineVertexCount = 0;
    m_pointVertexCount = 0;
    m_triangleVertexCount = 0;
    m_triangleIndexCount = 0;
}

void CadRenderer::SubmitCommands(RenderQueue& queue, VkPipeline linePipeline,
                                  VkPipelineLayout pipelineLayout) {
    if (m_buffersDirty) UploadBuffers();
    if (!HasGeometry()) return;

    RenderCommand cmd = {};
    cmd.pipeline = linePipeline;
    cmd.pipelineLayout = pipelineLayout;
    cmd.descriptorSet = VK_NULL_HANDLE;
    cmd.isVisible = true;
    cmd.isPersistent = true;

    if (m_lineVertexCount > 0) {
        cmd.nodeKey = 0;
        cmd.pointCount = m_lineVertexCount;
        cmd.geometryRevision = 0;
        queue.SubmitCommand(cmd);
    }

    if (m_pointVertexCount > 0) {
        cmd.pointCount = m_pointVertexCount;
        queue.SubmitCommand(cmd);
    }
}

} // namespace renderer
} // namespace workstation
