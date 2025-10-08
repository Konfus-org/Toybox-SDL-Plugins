#pragma 
#include <Tbx/Plugins/Plugin.h>
#include <Tbx/Graphics/GraphicsContext.h>
#include <Tbx/Windowing/Window.h>

namespace Tbx::Plugins::SDLGraphicsContext
{
    class SDLOpenGlGraphicsContextsProviderPlugin final
        : public Plugin
        , public IGraphicsContextProvider
    {
    public:
        SDLOpenGlGraphicsContextsProviderPlugin(Ref<EventBus> eventBus) {}
       GraphicsApi GetApi() const override;
       Ref<IGraphicsContext> Provide(Ref<Window> window) override;

    private:
        void DeleteGraphicsContext(IGraphicsContext* context);
    };

    TBX_REGISTER_PLUGIN(SDLOpenGlGraphicsContextsProviderPlugin);
}
