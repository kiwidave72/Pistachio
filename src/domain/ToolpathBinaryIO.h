#pragma once

// -----------------------------------------------------------------------
// ToolpathBinaryIO.h
//
// Fastest possible save/load — raw packed struct arrays, no encoding
// overhead at all (not even msgpack's type-tagging). Same producer and
// consumer (this app, same build), so no cross-platform/version
// portability concerns to design around.
// -----------------------------------------------------------------------

#include "domain/Toolpath.h"

#include <cstdio>
#include <cstdint>

namespace domain::v1 {

    // Explicit on-disk layout — deliberately separate from the runtime
    // ToolpathSegment struct, so a future runtime struct change can't
    // silently corrupt the binary format.
    struct BinarySegmentRecord
    {
        float startX, startY, startZ, startFeedRate;
        float endX, endY, endZ, endFeedRate;
        float extrusionDelta;
        float extrusionWidth;
        int32_t moveType;
    };

    inline bool saveToolpathBinary(const Toolpath& t, const std::string& path)
    {
        FILE* f = fopen(path.c_str(), "wb");
        if (!f) return false;

        const char magic[4] = { 'T','P','B','1' };
        fwrite(magic, 1, 4, f);

        uint32_t layerCount = (uint32_t)t.layers.size();
        fwrite(&layerCount, sizeof(uint32_t), 1, f);

        for (auto& layer : t.layers)
        {
            fwrite(&layer.z, sizeof(float), 1, f);

            uint32_t segCount = (uint32_t)layer.segments.size();
            fwrite(&segCount, sizeof(uint32_t), 1, f);

            std::vector<BinarySegmentRecord> records;
            records.reserve(segCount);
            for (auto& seg : layer.segments)
            {
                BinarySegmentRecord r;
                r.startX = seg.start.position.x; r.startY = seg.start.position.y; r.startZ = seg.start.position.z;
                r.startFeedRate = seg.start.feedRate;
                r.endX = seg.end.position.x; r.endY = seg.end.position.y; r.endZ = seg.end.position.z;
                r.endFeedRate = seg.end.feedRate;
                r.extrusionDelta = seg.extrusionDelta;
                r.extrusionWidth = seg.extrusionWidth;
                r.moveType = (int32_t)seg.moveType;
                records.push_back(r);
            }
            // ONE fwrite for the whole layer's segments — the actual speed win,
            // no per-field/per-segment call overhead.
            if (!records.empty())
                fwrite(records.data(), sizeof(BinarySegmentRecord), records.size(), f);
        }

        fclose(f);
        return true;
    }

    inline bool loadToolpathBinary(Toolpath& t, const std::string& path)
    {
        FILE* f = fopen(path.c_str(), "rb");
        if (!f) return false;

        char magic[4];
        fread(magic, 1, 4, f);
        if (magic[0] != 'T' || magic[1] != 'P' || magic[2] != 'B' || magic[3] != '1')
        {
            fclose(f);
            return false;
        }

        uint32_t layerCount = 0;
        fread(&layerCount, sizeof(uint32_t), 1, f);

        t.layers.clear();
        t.layers.reserve(layerCount);

        for (uint32_t i = 0; i < layerCount; ++i)
        {
            ToolpathLayer layer;
            fread(&layer.z, sizeof(float), 1, f);

            uint32_t segCount = 0;
            fread(&segCount, sizeof(uint32_t), 1, f);

            std::vector<BinarySegmentRecord> records(segCount);
            if (segCount > 0)
                fread(records.data(), sizeof(BinarySegmentRecord), segCount, f);

            layer.segments.reserve(segCount);
            for (auto& r : records)
            {
                ToolpathSegment seg;
                seg.start.position = { r.startX, r.startY, r.startZ };
                seg.start.feedRate = r.startFeedRate;
                seg.end.position = { r.endX, r.endY, r.endZ };
                seg.end.feedRate = r.endFeedRate;
                seg.extrusionDelta = r.extrusionDelta;
                seg.extrusionWidth = r.extrusionWidth;
                seg.moveType = (ToolpathMoveType)r.moveType;
                layer.segments.push_back(seg);
            }

            t.layers.push_back(std::move(layer));
        }

        fclose(f);
        return true;
    }

} // namespace domain::v1