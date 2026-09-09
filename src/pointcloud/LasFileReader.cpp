#include "workstation/pointcloud/LasFileReader.h"
#include "workstation/pointcloud/PointCloud.h"
#include "workstation/pointcloud/PointCloudNode.h"
#include "workstation/pointcloud/PointAttributeChannel.h"
#include "workstation/pointcloud/BoundingBox.h"
#include "workstation/pointcloud/VoxelNode.h"
#include "workstation/spatial/CoordinateNormalizationManager.h"
#include "workstation/spatial/OctreeBuilder.h"
#include "workstation/spatial/Octree.h"

#include <laszip_api.h>

#include <algorithm>
#include <cstdio>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

namespace workstation {
namespace pointcloud {

namespace {

bool PointFormatHasColor(laszip_U8 pointDataFormat) {
    switch (pointDataFormat) {
        case 2: case 3: case 5: case 7: case 8: case 10:
            return true;
        default:
            return false;
    }
}

std::string BaseName(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    return pos == std::string::npos ? path : path.substr(pos + 1);
}

// ---------------------------------------------------------------------------
// Convert a spatial::OctreeNode tree into a VoxelNode/PointCloudNode hierarchy.
// Leaf OctreeNodes become PointCloudNode with sliced point data.
// Internal OctreeNodes become VoxelNode with children.
// ---------------------------------------------------------------------------
std::unique_ptr<PointCloudNode> ConvertOctreeNode(
    const spatial::OctreeNode* src,
    const std::vector<float>& positions,
    const std::vector<float>& colors,
    const std::vector<float>& intensities,
    const std::vector<uint8_t>& classifications,
    uint32_t& outNodeCount,
    uint32_t& outLeafCount)
{
    if (!src) return nullptr;

    outNodeCount++;

    if (src->IsLeaf()) {
        outLeafCount++;
        auto node = std::make_unique<PointCloudNode>();
        node->setBounds(src->bounds);

        uint32_t offset = src->pointOffset;
        uint32_t count = src->pointCount;
        if (count == 0) return nullptr;

        // Copy point slice from flat arrays into this node's channels
        node->channels().AddChannel(CreateChannel(
            ChannelId::XYZ, PointFormat::Float32, count,
            positions.data() + offset * 3));
        node->channels().AddChannel(CreateChannel(
            ChannelId::RGB, PointFormat::Float32, count,
            colors.data() + offset * 3));
        node->channels().AddChannel(CreateChannel(
            ChannelId::Intensity, PointFormat::Float32, count,
            intensities.data() + offset));
        node->channels().AddChannel(CreateChannel(
            ChannelId::Classification, PointFormat::UInt8, count,
            classifications.data() + offset));

        return node;
    }

    // Internal node → VoxelNode with children
    auto voxel = std::make_unique<VoxelNode>();
    voxel->setBounds(src->bounds);
    // Internal nodes carry aggregate point count in totalPoints
    // (no channel data — only leaves have point data)

    for (int i = 0; i < 8; ++i) {
        if (src->children[i]) {
            auto child = ConvertOctreeNode(
                src->children[i].get(), positions, colors, intensities,
                classifications, outNodeCount, outLeafCount);
            if (child) {
                child->setParent(voxel.get());
                voxel->AddChild(std::move(child));
            }
        }
    }

    return voxel;
}

} // namespace

bool LoadLasFile(const std::string& filepath, PointCloud& outCloud, std::string* errorMessage,
                  spatial::CoordinateNormalizationManager* sharedNormalizer) {
    laszip_POINTER reader = nullptr;
    if (laszip_create(&reader)) {
        if (errorMessage) *errorMessage = "laszip_create failed";
        return false;
    }

    laszip_BOOL isCompressed = 0;
    if (laszip_open_reader(reader, filepath.c_str(), &isCompressed)) {
        laszip_CHAR* err = nullptr;
        laszip_get_error(reader, &err);
        if (errorMessage) *errorMessage = err ? err : "failed to open LAS/LAZ file";
        laszip_destroy(reader);
        return false;
    }

    laszip_header_struct* header = nullptr;
    laszip_get_header_pointer(reader, &header);
    laszip_point_struct* point = nullptr;
    laszip_get_point_pointer(reader, &point);

    if (!header || !point) {
        if (errorMessage) *errorMessage = "failed to read header";
        laszip_close_reader(reader);
        laszip_destroy(reader);
        return false;
    }

    // laszip_get_point_count() reports points read so far (starts at 0), not
    // the file's total record count -- that comes from the header itself.
    laszip_I64 pointCount64 = header->number_of_point_records;
    if (pointCount64 == 0) {
        pointCount64 = static_cast<laszip_I64>(header->extended_number_of_point_records);
    }
    size_t pointCount = static_cast<size_t>(pointCount64);

    if (pointCount == 0) {
        if (errorMessage) *errorMessage = "file has no points";
        laszip_close_reader(reader);
        laszip_destroy(reader);
        return false;
    }

    bool hasColor = PointFormatHasColor(header->point_data_format);

    // Read raw (un-recentered) coordinates first and track the actual data
    // bounds ourselves -- some LAS/LAZ writers leave the header's
    // min_x/max_x/min_y/max_y/min_z/max_z degenerate (or stale), which would
    // otherwise silently recenter around the wrong origin and leave every
    // point far outside the camera's view.
    std::vector<double> rawCoords(pointCount * 3);
    std::vector<float> colors(pointCount * 3);
    std::vector<float> intensities(pointCount);
    std::vector<uint8_t> classifications(pointCount);

    double minX = 0, minY = 0, minZ = 0, maxX = 0, maxY = 0, maxZ = 0;
    double coords[3] = {};
    size_t readCount = 0;
    for (; readCount < pointCount; ++readCount) {
        if (laszip_read_point(reader)) break;
        laszip_get_coordinates(reader, coords);

        rawCoords[readCount * 3 + 0] = coords[0];
        rawCoords[readCount * 3 + 1] = coords[1];
        rawCoords[readCount * 3 + 2] = coords[2];

        if (readCount == 0) {
            minX = maxX = coords[0];
            minY = maxY = coords[1];
            minZ = maxZ = coords[2];
        } else {
            if (coords[0] < minX) minX = coords[0]; else if (coords[0] > maxX) maxX = coords[0];
            if (coords[1] < minY) minY = coords[1]; else if (coords[1] > maxY) maxY = coords[1];
            if (coords[2] < minZ) minZ = coords[2]; else if (coords[2] > maxZ) maxZ = coords[2];
        }

        intensities[readCount] = point->intensity / 65535.0f;
        // Legacy point formats (0-5) only carry a 5-bit classification
        // bitfield plus separate synthetic/keypoint/withheld flag bits, so
        // reading point->classification alone truncates any code above 31
        // (e.g. ASPRS 51 = noise) and drops those flags. LAS 1.4 extended
        // formats (6-10) carry the full 8-bit code in extended_classification
        // instead, selected by extended_point_type.
        if (point->extended_point_type) {
            classifications[readCount] = point->extended_classification;
        } else {
            classifications[readCount] = static_cast<uint8_t>(
                point->classification |
                (point->synthetic_flag << 5) |
                (point->keypoint_flag << 6) |
                (point->withheld_flag << 7));
        }

        if (hasColor) {
            colors[readCount * 3 + 0] = point->rgb[0] / 65535.0f;
            colors[readCount * 3 + 1] = point->rgb[1] / 65535.0f;
            colors[readCount * 3 + 2] = point->rgb[2] / 65535.0f;
        } else {
            colors[readCount * 3 + 0] = 0.6f;
            colors[readCount * 3 + 1] = 0.6f;
            colors[readCount * 3 + 2] = 0.6f;
        }
    }

    laszip_close_reader(reader);
    laszip_destroy(reader);

    if (readCount == 0) {
        if (errorMessage) *errorMessage = "failed to read any points";
        return false;
    }

    // Points in a LAS file are typically large UTM-style coordinates;
    // recenter around the actual data's midpoint so float32 storage keeps
    // precision. If a shared normalizer already has an origin (set by a
    // previously loaded SNT attachment or point cloud), reuse it instead of
    // this file's own midpoint so the two register in the same local space -
    // otherwise every file recenters independently and nothing lines up.
    double originX, originY, originZ;
    if (sharedNormalizer && sharedNormalizer->IsNormalized()) {
        sharedNormalizer->GetOrigin(originX, originY, originZ);
    } else {
        originX = (minX + maxX) * 0.5;
        originY = (minY + maxY) * 0.5;
        originZ = (minZ + maxZ) * 0.5;
        if (sharedNormalizer) {
            spatial::BoundingBox worldBounds;
            worldBounds.minX = minX; worldBounds.maxX = maxX;
            worldBounds.minY = minY; worldBounds.maxY = maxY;
            worldBounds.minZ = minZ; worldBounds.maxZ = maxZ;
            sharedNormalizer->RecomputeFromBoundingBox(worldBounds);
        }
    }

    std::vector<float> positions(readCount * 3);
    for (size_t i = 0; i < readCount; ++i) {
        positions[i * 3 + 0] = static_cast<float>(rawCoords[i * 3 + 0] - originX);
        positions[i * 3 + 1] = static_cast<float>(rawCoords[i * 3 + 1] - originY);
        positions[i * 3 + 2] = static_cast<float>(rawCoords[i * 3 + 2] - originZ);
    }

    BoundingBox bounds;
    bounds.minX = minX - originX;
    bounds.minY = minY - originY;
    bounds.minZ = minZ - originZ;
    bounds.maxX = maxX - originX;
    bounds.maxY = maxY - originY;
    bounds.maxZ = maxZ - originZ;

    // Normals are NOT computed here: per-point neighbor search + plane
    // fitting is expensive CPU work (seconds to minutes at LiDAR scale) and
    // most loads never use NormalShading mode. See NormalEstimator.h --
    // ViewportWindow::SetVisualizationMode computes and caches them lazily,
    // the first time that mode is actually selected for a given cloud.
    
    // Build octree for hierarchical culling and streaming
    pointcloud::PointCloud* cloudPtr = &outCloud;
    spatial::OctreeBuilder builder;
    builder.Build(positions.data(), readCount,
                  spatial::OctreeNode::kMaxDepth, 50000); // max 50k points per leaf
    auto* octreeRoot = builder.GetRoot();
    uint32_t nodeCount = builder.GetNodeCount();
    uint32_t leafCount = builder.GetLeafCount();
    uint32_t maxDepth = builder.GetMaxDepthReached();
    
    fprintf(stderr, "[LasFileReader] Octree built: %u nodes (%u leaves), max depth %u\n",
            nodeCount, leafCount, maxDepth);

    // Propagate octree statistics to the cloud for runtime use
    if (cloudPtr) {
        cloudPtr->SetOctreeStats(nodeCount, leafCount, maxDepth);
    }

    // Build VoxelNode hierarchy from the OctreeNode tree.
    // This gives PreparePointCloud and BuildSpatialTreeFromCloud the
    // recursive VoxelNode hierarchy they need for multi-node rendering.
    uint32_t convertedNodes = 0;
    uint32_t convertedLeaves = 0;
    auto root = ConvertOctreeNode(
        octreeRoot, positions, colors, intensities, classifications,
        convertedNodes, convertedLeaves);

    if (!root) {
        // Fallback: flat root (should not happen with valid octree)
        auto flat = std::make_unique<PointCloudNode>();
        flat->setBounds(bounds);
        flat->channels().AddChannel(CreateChannel(
            ChannelId::XYZ, PointFormat::Float32, readCount, positions.data()));
        flat->channels().AddChannel(CreateChannel(
            ChannelId::RGB, PointFormat::Float32, readCount, colors.data()));
        flat->channels().AddChannel(CreateChannel(
            ChannelId::Intensity, PointFormat::Float32, readCount, intensities.data()));
        flat->channels().AddChannel(CreateChannel(
            ChannelId::Classification, PointFormat::UInt8, readCount, classifications.data()));
        root = std::move(flat);
    }

    outCloud.SetName(BaseName(filepath).c_str());
    outCloud.SetRoot(root.release());
    outCloud.Finalize();

    fprintf(stderr, "[LasFileReader] Loaded %zu points from %s\n", readCount, filepath.c_str());
    fprintf(stderr, "[LasFileReader] Octree hierarchy: %u nodes (%u leaves)\n",
            convertedNodes, convertedLeaves);
    fprintf(stderr, "[LasFileReader] First XYZ: (%.3f, %.3f, %.3f)\n",
            positions[0], positions[1], positions[2]);
    fprintf(stderr, "[LasFileReader] Bounds: min=(%.3f, %.3f, %.3f) max=(%.3f, %.3f, %.3f)\n",
            bounds.minX, bounds.minY, bounds.minZ, bounds.maxX, bounds.maxY, bounds.maxZ);
    fprintf(stderr, "[LasFileReader] First RGB: (%.3f, %.3f, %.3f)\n",
            colors[0], colors[1], colors[2]);

    // Classification histogram: diagnostic for "PTC colors aren't showing"
    // reports - lets us see at a glance whether this file's codes actually
    // overlap the loaded palette's codes, or whether it's an unclassified
    // scan (everything code 0/1) that just hasn't been run through whatever
    // classification workflow the PTC's codes assume.
    {
        std::unordered_map<uint8_t, size_t> histogram;
        for (size_t i = 0; i < readCount; ++i) histogram[classifications[i]]++;
        std::vector<std::pair<uint8_t, size_t>> sorted(histogram.begin(), histogram.end());
        std::sort(sorted.begin(), sorted.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });
        fprintf(stderr, "[LasFileReader] Classification codes present (%zu distinct): ",
                sorted.size());
        for (size_t i = 0; i < sorted.size() && i < 20; ++i) {
            fprintf(stderr, "%u=%zu%s", sorted[i].first, sorted[i].second,
                    (i + 1 < sorted.size() && i + 1 < 20) ? ", " : "");
        }
        fprintf(stderr, "\n");
    }
    return true;
}

} // namespace pointcloud
} // namespace workstation
