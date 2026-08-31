#include "workstation/pod/PodBlockDecoder.h"
#include "workstation/pod/PodBinaryReader.h"
#include "workstation/pod/PodNodeDecoder.h"
#include "workstation/pod/PodChannelDecoder.h"
#include "workstation/pod/PodDecodeContext.h"
#include "workstation/pod/PodRecords.h"
#include <cstring>
#include "workstation/pointcloud/PointAttributeChannel.h"
#include "workstation/pointcloud/VoxelNode.h"

namespace workstation { namespace pod {

bool PodBlockDecoder::decodeChannels(PodBinaryReader& reader,
                                     pointcloud::PointCloudNode& node) {
    std::uint32_t channelCount = 0;
    if (!reader.readU32(channelCount)) return false;

    for (std::uint32_t i = 0; i < channelCount; ++i) {
        // Read channel record.
        std::uint32_t channelCodeRaw = 0, formatCodeRaw = 0;
        std::uint32_t count = 0, stride = 0;
        if (!reader.readU32(channelCodeRaw)) return false;
        if (!reader.readU32(formatCodeRaw)) return false;
        if (!reader.readU32(count)) return false;
        if (!reader.readU32(stride)) return false;

        // Normalize codes.
        std::uint32_t channelCode = NormalizeChannelCode(channelCodeRaw);
        std::uint32_t formatCode  = NormalizeFormatCode(formatCodeRaw);
        if (channelCode == 0 || formatCode == 0) return false;

        // Read scale/offset.
        double scale[3], offset[3];
        for (int j = 0; j < 3; ++j) if (!reader.readDouble(scale[j])) return false;
        for (int j = 0; j < 3; ++j) if (!reader.readDouble(offset[j])) return false;

        // Calculate and read payload.
        std::size_t payloadBytes = 0;
        if (!CalculatePayloadSize(formatCode, count, stride, payloadBytes)) return false;

        std::vector<uint8_t> payload(payloadBytes);
        if (payloadBytes > 0 && !reader.read(payload.data(), payloadBytes)) return false;

        // Map normalized codes to PointAttributeChannel types.
        pointcloud::ChannelId chId;
        pointcloud::PointFormat fmt;
        switch (channelCode) {
            case 1: chId = pointcloud::ChannelId::XYZ;            break;
            case 2: chId = pointcloud::ChannelId::RGB;            break;
            case 3: chId = pointcloud::ChannelId::Intensity;      break;
            case 4: chId = pointcloud::ChannelId::Classification; break;
            case 5: chId = pointcloud::ChannelId::Normals;        break;
            default: chId = pointcloud::ChannelId::XYZ;            break;
        }
        switch (formatCode) {
            case 1: case 8: case 9:  fmt = pointcloud::PointFormat::Float32; break;
            case 2: case 10:         fmt = pointcloud::PointFormat::Float32; break; // 8 bytes → double stored as float for decode
            case 4: case 5:          fmt = pointcloud::PointFormat::UInt8;   break;
            case 6: case 7:          fmt = pointcloud::PointFormat::Int16;   break;
            default:                 fmt = pointcloud::PointFormat::Float32; break;
        }

        // Create channel (piece 2 channel creation: FUN_180081480).
        // Note: stride from the stream is component count; Create() derives
        // byte stride from elementSize automatically when stride is omitted.
        pointcloud::PointAttributeChannel ch;
        ch.Create(chId, fmt, count, payload.data(), scale, offset);
        node.channels().AddChannel(std::move(ch));
    }
    return true;
}

bool PodBlockDecoder::processHandlerTable(PodBinaryReader& reader,
                                          pointcloud::PointCloud& cloud) {
    std::uint32_t handlerCount = 0;
    if (!reader.readU32(handlerCount)) return false;

    for (std::uint32_t i = 0; i < handlerCount; ++i) {
        std::uint32_t identifier = 0;
        if (!reader.readU32(identifier)) return false;

        // Build key (4-byte identifier).
        std::vector<uint8_t> key(sizeof(identifier));
        std::memcpy(key.data(), &identifier, sizeof(identifier));

        // Lookup in handler tree.
        PodBlockHandler* handler = handlers_.findLowerBound(
            std::span<const uint8_t>(key));
        if (handler) {
            // Create context and dispatch.
            PodDecodeContext ctx(reader, cloud);
            handler->process(ctx);
        }
    }
    return true;
}

bool PodBlockDecoder::decode(PodDataSource& source, pointcloud::PointCloud& cloud) {
    // Validate cloud state (do not overwrite).
    if (cloud.Root() != nullptr) return false;

    // Create buffered reader (256 KB, FUN_18006d760).
    PodBinaryReader reader(source);

    // Read first two uint32 values (FUN_180078840).
    std::uint32_t tableCount = 0, nodeCount = 0;
    if (!reader.readU32(tableCount)) return false;
    if (!reader.readU32(nodeCount))  return false;

    // Read auxiliary uint32 array (if non-zero).
    std::vector<std::uint32_t> auxTable;
    if (tableCount > 0) {
        auxTable.resize(tableCount);
        for (std::uint32_t i = 0; i < tableCount; ++i)
            if (!reader.readU32(auxTable[i])) return false;
    }

    // Process node/block records.
    auto root = pointcloud::CreateVoxelNode(pointcloud::BoundingBox{});
    for (std::uint32_t i = 0; i < nodeCount; ++i) {
        // Read node type flag.
        std::uint32_t typeFlagRaw = 0;
        if (!reader.readU32(typeFlagRaw)) return false;
        NodeType nodeType = (typeFlagRaw == 1) ? NodeType::Hierarchical : NodeType::Normal;

        // Read geometry path.
        std::uint32_t geomPathRaw = 0;
        if (!reader.readU32(geomPathRaw)) return false;
        GeometryPath geomPath = (geomPathRaw == 1) ? GeometryPath::Float64 : GeometryPath::Float32;

        // Decode node (FUN_1800764c0 / FUN_180081100).
        std::uint64_t sourceNodeValue = 0;
        NodeExtraMetadata extraMeta;
        auto node = PodNodeDecoder::DecodeNode(reader, nodeType, geomPath,
                                                sourceNodeValue, extraMeta);
        if (!node) return false;

        // Read and attach channels.
        if (!decodeChannels(reader, *node)) return false;

        // Attach node to cloud root.
        root->AddChild(std::move(node));
    }

    cloud.SetRoot(root.get());
    root_ = std::move(root);

    // Finalize cloud (FUN_18007d990).
    cloud.Finalize();

    // Post-load handler dispatch (FUN_180078620).
    processHandlerTable(reader, cloud);

    // Post-load metadata (FUN_1800774c0 / FUN_180078620 / FUN_180077c80) — stubs.
    postLoad_.onPostLoad(cloud);

    return true;
}

} // namespace pod
} // namespace workstation
