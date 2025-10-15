#include "SDLWindow.h"
#include "Tbx/Debug/Asserts.h"
#include "Tbx/Events/WindowEvents.h"
#include <SDL3/SDL_events.h>

namespace Tbx::Plugins::SDLWindowing
{
    SDLWindow::SDLWindow(bool useOpenGl, Ref<EventBus> eventBus)
        : _eventCarrier(eventBus), _useOpenGl(useOpenGl)
    {
    }

    SDLWindow::~SDLWindow()
    {
        Close();
    }

    NativeHandle SDLWindow::GetNativeHandle() const
    {
        return SDL_GetDisplayForWindow(_window);
    }

    NativeWindow SDLWindow::GetNativeWindow() const
    {
        return _window;
    }

    void SDLWindow::Open()
    {
        _isClosed = false;
        Uint32 flags = SDL_WINDOW_RESIZABLE;

        if (_useOpenGl)
        {
            flags |= SDL_WINDOW_OPENGL;
        }

        switch (_currentMode)
        {
            using enum WindowMode;
        case Windowed:   break;
        case Fullscreen: flags |= SDL_WINDOW_FULLSCREEN; break;
        case Borderless: flags |= SDL_WINDOW_BORDERLESS; break;
        case FullscreenBorderless:
        {
            flags |= SDL_WINDOW_FULLSCREEN;
            flags |= SDL_WINDOW_BORDERLESS;
            break;
        }
        default:
        {
            TBX_ASSERT(false, "Invalid window mode");
        }
        }

        _window = SDL_CreateWindow(_title.c_str(), _size.Width, _size.Height, flags);
        TBX_ASSERT(_window, "SDLWindow: SDL_CreateWindow failed: %s", SDL_GetError());
        _eventCarrier.Post(WindowOpenedEvent(this));
    }

    void SDLWindow::Close()
    {
        // We've already been closed...
        if (_window == nullptr) return;

        // Cleanup...
        SDL_DestroyWindow(_window);
        _window = nullptr;
        _isClosed = true;

        if (_useOpenGl)
        {
            // Reset the context
            SDL_GL_DestroyContext(_glContext);
        }

        _eventCarrier.Post(WindowClosedEvent(this));
    }

    void SDLWindow::Update()
    {
        SDL_Event e = {};
        SDL_PollEvent(&e);
        switch (e.type)
        {
        case SDL_EVENT_QUIT:
        {
            Close();
            break;
        }
        case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
        {
            if (e.window.windowID == SDL_GetWindowID(_window))
            {
                int w, h;
                SDL_GetWindowSize(_window, &w, &h);
                SetSize(Size(w, h));
                SetMode(WindowMode::Fullscreen);
            }
            break;
        }
        case SDL_EVENT_WINDOW_RESIZED:
        {
            if (e.window.windowID == SDL_GetWindowID(_window))
            {
                int w, h;
                SDL_GetWindowSize(_window, &w, &h);
                SetSize(Size(w, h));
            }
            break;
        }
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
        {
            if (e.window.windowID == SDL_GetWindowID(_window))
            {
                Focus();
            }
            break;
        }
        case SDL_EVENT_WINDOW_FOCUS_LOST:
        {
            if (e.window.windowID == SDL_GetWindowID(_window))
            {
                _isFocused = false;
            }
        }
        }

        if (_useOpenGl)
        {
            SDL_GL_SwapWindow(_window);
        }
    }

    void SDLWindow::Focus()
    {
        _isFocused = true;
        SDL_RaiseWindow(_window);
        if (_useOpenGl)
        {
            SDL_GL_MakeCurrent(_window, _glContext);
        }
        _eventCarrier.Post(WindowFocusedEvent(this));
    }

    bool SDLWindow::IsClosed()
    {
        return _isClosed;
    }

    bool SDLWindow::IsFocused()
    {
        return _isFocused;
    }

    const std::string& SDLWindow::GetTitle() const
    {
        return _title;
    }

    void SDLWindow::SetTitle(const std::string& title)
    {
        _title = title;

        if (!_window)
        {
            return;
        }

        SDL_SetWindowTitle(_window, title.c_str());
    }

    const Size& SDLWindow::GetSize() const
    {
        return _size;
    }

    void SDLWindow::SetSize(const Size& size)
    {
        _size = size;

        if (!_window)
        {
            return;
        }

        SDL_SetWindowSize(_window, _size.Width, _size.Height);
        _eventCarrier.Post(WindowResizedEvent(this));
    }

    void SDLWindow::SetMode(const WindowMode& mode)
    {
        _currentMode = mode;
        if (_window == nullptr)
        {
            return;
        }

        switch (_currentMode)
        {
            using enum WindowMode;
        case Windowed:
        {
            SDL_SetWindowFullscreen(_window, false);
            SDL_SetWindowBordered(_window, true);
            break;
        }
        case Fullscreen:
        {
            SDL_SetWindowFullscreen(_window, true);
            break;
        }
        case Borderless:
        {
            SDL_SetWindowFullscreen(_window, false);
            SDL_SetWindowBordered(_window, false);
            break;
        }
        case FullscreenBorderless:
        {
            SDL_SetWindowFullscreen(_window, true);
            SDL_SetWindowBordered(_window, false);
            break;
        }
        }
    }

    WindowMode SDLWindow::GetMode()
    {
        return _currentMode;
    }
}