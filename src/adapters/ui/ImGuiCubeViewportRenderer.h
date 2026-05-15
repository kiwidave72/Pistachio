#pragma once
#include "ports/IViewportRendererPort.h"

namespace adapters {

class ImGuiCubeViewportRenderer final : public ports::IViewportRendererPort {
public:
    const char* name() const override { return "ImGuiCubeFallback"; }

    void render(const ports::ViewportRect& vp,
                const ports::ViewportCamera& cam,
                const ports::ViewportRenderContext& ctx) override;
};

} // namespace adapters
