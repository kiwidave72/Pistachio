#pragma once
#include <memory>
#include "domain/Model.h"

namespace ports {

    class IRendererPort {
    public:
        virtual ~IRendererPort() = default;

        virtual bool initialize() = 0;
        virtual void shutdown() = 0;
        virtual void render() = 0;
        virtual void setModel(std::shared_ptr<domain::Model> model) = 0;
        virtual void fitAll() = 0;
        virtual void* getFramebufferTexture() = 0;
        virtual void resize(int width, int height) = 0;

        // Camera controls
        virtual void rotate(float dx, float dy) = 0;
        virtual void pan(float dx, float dy) = 0;
        virtual void zoom(float delta) = 0;
        virtual void setViewDirection(int direction) = 0;
    };

} // namespace ports