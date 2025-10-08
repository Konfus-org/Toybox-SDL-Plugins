#include "SDLWindowFactoryPlugin.h"
#include "SDLWindow.h"

namespace SDLWindowing
{
    SDLWindowFactoryPlugin::SDLWindowFactoryPlugin(Tbx::Ref<Tbx::EventBus> eventBus)
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

    std::shared_ptr<Tbx::Window> SDLWindowFactoryPlugin::Create(const std::string& title, const Tbx::Size& size, const Tbx::WindowMode& mode, Tbx::Ref<Tbx::EventBus> eventBus)
    {
        auto* sdlWindow = new SDLWindow(_usingOpenGl, eventBus);
        auto window = std::shared_ptr<Tbx::Window>((Tbx::Window*)sdlWindow, [this](Tbx::Window* win) { DeleteWindow(win); });

        // HACK: used to get around enable shared from this issues
        {
            sdlWindow->SetThis(window);
        }

        window->SetTitle(title);
        window->SetSize(size);
        window->SetMode(mode);

        return window;
    }

    void SDLWindowFactoryPlugin::OnAppSettingsChanged(const Tbx::AppSettingsChangedEvent& e)
    {
        _usingOpenGl = e.GetNewSettings().RenderingApi == Tbx::GraphicsApi::OpenGL;
    }

    void SDLWindowFactoryPlugin::DeleteWindow(Tbx::Window* window)
    {
        delete window;
    }
}
