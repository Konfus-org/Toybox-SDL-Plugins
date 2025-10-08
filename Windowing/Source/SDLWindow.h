#pragma once
#include <Tbx/Windowing/Window.h>
#include <SDL3/SDL.h>

namespace SDLWindowing
{
    class SDLWindow 
        : public Tbx::Window
    {
    public:
        SDLWindow(bool useOpenGl, Tbx::Ref<Tbx::EventBus> eventBus);
        ~SDLWindow() override;

        Tbx::NativeHandle GetNativeHandle() const override;
        Tbx::NativeWindow GetNativeWindow() const override;

        // HACK: used to get around enable shared from this issues
        void SetThis(Tbx::WeakRef<Tbx::Window> window);

        void Open() override;
        void Close() override;
        void Update() override;
        void Focus() override;

        bool IsClosed() override;
        bool IsFocused() override;

        const std::string& GetTitle() const override;
        void SetTitle(const std::string& title) override;

        void SetSize(const Tbx::Size& size) override;
        const Tbx::Size& GetSize() const override;

        void SetMode(const Tbx::WindowMode& mode) override;
        Tbx::WindowMode GetMode() override;

    private:
        SDL_GLContext _glContext = nullptr;
        SDL_Window* _window = nullptr;
        Tbx::WeakRef<Tbx::Window> _this = {};
        Tbx::Ref<Tbx::EventBus> _eventBus = nullptr;
        Tbx::WindowMode _currentMode = Tbx::WindowMode::Windowed;
        Tbx::Size _size = { 800, 800 };
        std::string _title = "New Window";
        bool _isFocused = false;
        bool _isClosed = false;
        bool _useOpenGl = false;
    };
}

