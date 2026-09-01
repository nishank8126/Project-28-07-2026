#include "workstation/pointcloud/LasFileReader.h"
#include "workstation/pointcloud/PointCloud.h"
#include "workstation/pointcloud/PointCloudNode.h"
#include "workstation/pointcloud/PointAttributeChannel.h"
#include "workstation/pointcloud/BoundingBox.h"

#include <laszip_api.h>

#include <cstdio>
#include <memory>
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

} // namespace

bool LoadLasFile(const std::string& filepath, PointCloud& outCloud, std::string* errorMessage) {
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
        classifications[readCount] = point->classification;

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
    // precision.
    double originX = (minX + maxX) * 0.5;
    double originY = (minY + maxY) * 0.5;
    double originZ = (minZ + maxZ) * 0.5;

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
    auto node = std::make_unique<PointCloudNode>();
    node->setBounds(bounds);

    node->channels().AddChannel(CreateChannel(
        ChannelId::XYZ, PointFormat::Float32, readCount, positions.data()));
    node->channels().AddChannel(CreateChannel(
        ChannelId::RGB, PointFormat::Float32, readCount, colors.data()));
    node->channels().AddChannel(CreateChannel(
        ChannelId::Intensity, PointFormat::Float32, readCount, intensities.data()));
    node->channels().AddChannel(CreateChannel(
        ChannelId::Classification, PointFormat::UInt8, readCount, classifications.data()));

    outCloud.SetName(BaseName(filepath).c_str());
    outCloud.SetRoot(node.release());
    outCloud.Finalize();

    fprintf(stderr, "[LasFileReader] Loaded %zu points from %s\n", readCount, filepath.c_str());
    fprintf(stderr, "[LasFileReader] First XYZ: (%.3f, %.3f, %.3f)\n",
            positions[0], positions[1], positions[2]);
    fprintf(stderr, "[LasFileReader] Bounds: min=(%.3f, %.3f, %.3f) max=(%.3f, %.3f, %.3f)\n",
            bounds.minX, bounds.minY, bounds.minZ, bounds.maxX, bounds.maxY, bounds.maxZ);
    fprintf(stderr, "[LasFileReader] First RGB: (%.3f, %.3f, %.3f)\n",
            colors[0], colors[1], colors[2]);
    return true;
}

} // namespace pointcloud
} // namespace workstation
