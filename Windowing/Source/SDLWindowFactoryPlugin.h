#pragma 
#include <Tbx/Plugins/Plugin.h>
#include <Tbx/Windowing/Window.h>
#include <Tbx/Events/AppEvents.h>
#include <Tbx/Events/EventListener.h>

namespace SDLWindowing
{
    class SDLWindowFactoryPlugin final
        : public Tbx::Plugin
        , public Tbx::IWindowFactory
    {
    public:
        SDLWindowFactoryPlugin(Tbx::Ref<Tbx::EventBus> eventBus);
        ~SDLWindowFactoryPlugin() override;

        std::shared_ptr<Tbx::Window> Create(
            const std::string& title,
            const Tbx::Size& size,
            const Tbx::WindowMode& mode,
            Tbx::Ref<Tbx::EventBus> eventBus) override;

    private:
        void OnAppLaunched(const Tbx::AppLaunchedEvent& e);
        void OnAppSettingsChanged(const Tbx::AppSettingsChangedEvent& e);
        void DeleteWindow(Tbx::Window* window);

    private:
        Tbx::EventListener _listener = {};
        bool _usingOpenGl = false;
    };

    TBX_REGISTER_PLUGIN(SDLWindowFactoryPlugin);
}
