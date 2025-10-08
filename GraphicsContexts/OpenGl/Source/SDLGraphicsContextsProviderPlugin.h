#pragma 
#include <Tbx/Plugins/Plugin.h>
#include <Tbx/Graphics/GraphicsContext.h>
#include <Tbx/Windowing/Window.h>

namespace SDLGraphicsContext
{
    class SDLOpenGlGraphicsContextsProviderPlugin final
        : public Tbx::Plugin
        , public Tbx::IGraphicsContextProvider
    {
    public:
        SDLOpenGlGraphicsContextsProviderPlugin(Tbx::Ref<Tbx::EventBus> eventBus) {}
        Tbx::GraphicsApi GetApi() const override;
        Tbx::Ref<Tbx::IGraphicsContext> Provide(Tbx::Ref<Tbx::Window> window) override;

    private:
        void DeleteGraphicsContext(Tbx::IGraphicsContext* context);
    };

    TBX_REGISTER_PLUGIN(SDLOpenGlGraphicsContextsProviderPlugin);
}
