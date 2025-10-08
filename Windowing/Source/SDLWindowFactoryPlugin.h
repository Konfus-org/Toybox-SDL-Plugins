#pragma 
#include <Tbx/Plugins/Plugin.h>
#include <Tbx/Windowing/Window.h>
#include <Tbx/Events/AppEvents.h>
#include <Tbx/Events/EventListener.h>

namespace Tbx::Plugins::SDLWindowing
{
    class SDLWindowFactoryPlugin final
        : public Plugin
        , public IWindowFactory
    {
    public:
        SDLWindowFactoryPlugin(Ref<EventBus> eventBus);
        ~SDLWindowFactoryPlugin() override;

        std::shared_ptr<Window> Create(
            const std::string& title,
            const Size& size,
            const WindowMode& mode,
            Ref<EventBus> eventBus) override;

    private:
        void OnAppLaunched(const AppLaunchedEvent& e);
        void OnAppSettingsChanged(const AppSettingsChangedEvent& e);
        void DeleteWindow(Window* window);

    private:
        EventListener _listener = {};
        bool _usingOpenGl = false;
    };

    TBX_REGISTER_PLUGIN(SDLWindowFactoryPlugin);
}
