#include "SDLWindowFactoryPlugin.h"
#include "SDLWindow.h"
#include <SDL3/SDL.h>

namespace Tbx::Plugins::SDLWindowing
{
    SDLWindowFactoryPlugin::SDLWindowFactoryPlugin(Ref<EventBus> eventBus)
        : _listener(eventBus)
        , _usingOpenGl(false)
    {
        TBX_ASSERT(SDL_Init(SDL_INIT_VIDEO)  != 0, "Failed to initialize SDL");
        _listener.Listen(this, &SDLWindowFactoryPlugin::OnAppSettingsChanged);
    }

    SDLWindowFactoryPlugin::~SDLWindowFactoryPlugin()
    {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        SDL_Quit(); // If we are unloading our windowing quit all of SDL
    }

    std::shared_ptr<Window> SDLWindowFactoryPlugin::Create(const std::string& title, const Size& size, const WindowMode& mode, Ref<EventBus> eventBus)
    {
        auto* sdlWindow = new SDLWindow(_usingOpenGl, eventBus);
        auto window = std::shared_ptr<Window>((Window*)sdlWindow, [this](Window* win) { DeleteWindow(win); });

        // HACK: used to get around enable shared from this issues
        {
            sdlWindow->SetThis(window);
        }

        window->SetTitle(title);
        window->SetSize(size);
        window->SetMode(mode);

        return window;
    }

    void SDLWindowFactoryPlugin::OnAppSettingsChanged(const AppSettingsChangedEvent& e)
    {
        _usingOpenGl = e.GetNewSettings().RenderingApi == GraphicsApi::OpenGL;
    }

    void SDLWindowFactoryPlugin::DeleteWindow(Window* window)
    {
        delete window;
    }
}
