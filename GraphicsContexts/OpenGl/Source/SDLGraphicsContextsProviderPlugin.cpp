#include "SDLGraphicsContextsProviderPlugin.h"
#include "SDLGraphicsContexts.h"

namespace SDLGraphicsContext
{
    Tbx::Ref<Tbx::IGraphicsContext> SDLOpenGlGraphicsContextsProviderPlugin::Provide(Tbx::Ref<Tbx::Window> window)
    {
        return Tbx::Ref<SDLGLGraphicsContext>(
            new SDLGLGraphicsContext(std::any_cast<SDL_Window*>(window->GetNativeWindow())),
            [this](SDLGLGraphicsContext* context) { DeleteGraphicsContext(context); });
    }

    Tbx::GraphicsApi SDLOpenGlGraphicsContextsProviderPlugin::GetApi() const
    {
        return Tbx::GraphicsApi::OpenGL;
    }

    void SDLOpenGlGraphicsContextsProviderPlugin::DeleteGraphicsContext(Tbx::IGraphicsContext* context)
    {
        delete context;
    }
}
