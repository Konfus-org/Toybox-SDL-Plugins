#pragma once
#include <Tbx/Graphics/GraphicsContext.h>
#include <SDL3/SDL_video.h>

namespace SDLGraphicsContext
{
    class SDLGLGraphicsContext final : public Tbx::IGraphicsContext
    {
    public:
        SDLGLGraphicsContext(SDL_Window* window);
        ~SDLGLGraphicsContext();

        void MakeCurrent() override;

        void Present() override;

        void SetVsync(Tbx::VsyncMode mode) override;

    private:
        SDL_Window* _window = nullptr;
        SDL_GLContext _glContext = nullptr;
    };
}

