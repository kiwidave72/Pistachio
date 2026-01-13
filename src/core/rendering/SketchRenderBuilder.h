#pragma once

#include "core/rendering/RenderScene.h"
#include "domain/SketchModel.h"

namespace core::rendering
{
    struct SketchRenderOptions
    {
        float z = 0.0f;

        Color entityColor{ 0.1f, 0.2f, 0.9f, 1.0f };
        Color constructionColor{ 0.3f, 0.3f, 0.6f, 0.6f };
        Color pointColor{ 0.9f, 0.9f, 0.9f, 1.0f };

        float lineThickness = 2.0f;
        float pointSize = 6.0f;

        // Selection styling
        Color selectedColor{ 0.95f, 0.75f, 0.15f, 1.0f };
        float selectedThicknessScale = 1.5f;
        const std::vector<domain::sketch::EntityId>* selectedIds = nullptr;
    };

    RenderScene BuildRenderSceneFromSketch(
        const domain::sketch::Sketch& sketch,
        const SketchRenderOptions& opt = {}
    );
}
