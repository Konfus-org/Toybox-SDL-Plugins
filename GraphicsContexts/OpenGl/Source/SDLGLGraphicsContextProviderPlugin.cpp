#include "SDLGLGraphicsContextProviderPlugin.h"
#include "SDLGLGraphicsContext.h"

namespace Tbx::Plugins::SDLGraphicsContext
{
   Ref<Tbx::IGraphicsContext> SDLOpenGlGraphicsContextsProviderPlugin::Provide(Ref<Window> window)
    {
        return Ref<SDLGLGraphicsContext>(
            new SDLGLGraphicsContext(std::any_cast<SDL_Window*>(window->GetNativeWindow())),
            [this](SDLGLGraphicsContext* context) { DeleteGraphicsContext(context); });
    }

   GraphicsApi SDLOpenGlGraphicsContextsProviderPlugin::GetApi() const
    {
        return GraphicsApi::OpenGL;
    }

    void SDLOpenGlGraphicsContextsProviderPlugin::DeleteGraphicsContext(IGraphicsContext* context)
    {
        delete context;
    }
}
