#pragma once
#include <Tbx/Windowing/Window.h>
#include <SDL3/SDL.h>

namespace Tbx::Plugins::SDLWindowing
{
    class SDLWindow 
        : public Window
    {
    public:
        SDLWindow(bool useOpenGl, Ref<EventBus> eventBus);
        ~SDLWindow() override;

        NativeHandle GetNativeHandle() const override;
        NativeWindow GetNativeWindow() const override;

        // HACK: used to get around enable shared from this issues
        void SetThis(WeakRef<Window> window);

        void Open() override;
        void Close() override;
        void Update() override;
        void Focus() override;

        bool IsClosed() override;
        bool IsFocused() override;

        const std::string& GetTitle() const override;
        void SetTitle(const std::string& title) override;

        void SetSize(const Size& size) override;
        const Size& GetSize() const override;

        void SetMode(const WindowMode& mode) override;
        WindowMode GetMode() override;

    private:
        SDL_GLContext _glContext = nullptr;
        SDL_Window* _window = nullptr;
        WeakRef<Window> _this = {};
        Ref<EventBus> _eventBus = nullptr;
        WindowMode _currentMode = WindowMode::Windowed;
        Size _size = { 800, 800 };
        std::string _title = "New Window";
        bool _isFocused = false;
        bool _isClosed = false;
        bool _useOpenGl = false;
    };
}

