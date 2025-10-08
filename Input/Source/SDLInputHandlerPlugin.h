#pragma once
#include <SDL3/SDL.h>
#include <Tbx/Plugins/Plugin.h>
#include <Tbx/Input/IInputHandler.h>
#include <array>
#include <unordered_map>

namespace SDLInput
{
    class SDLInputHandlerPlugin final
        : public Tbx::Plugin
        , public Tbx::IInputHandler
    {
    public:
        SDLInputHandlerPlugin(Tbx::Ref<Tbx::EventBus> eventBus);
        ~SDLInputHandlerPlugin() override;

        void Update() override;

        bool IsGamepadButtonDown(int playerIndex, int button) const override;
        bool IsGamepadButtonUp(int playerIndex, int button) const override;
        bool IsGamepadButtonHeld(int playerIndex, int button) const override;
        float GetGamepadAxis(int playerIndex, int axis) const override;

        bool IsKeyDown(int keyCode) const override;
        bool IsKeyUp(int keyCode) const override;
        bool IsKeyHeld(int keyCode) const override;

        bool IsMouseButtonDown(int button) const override;
        bool IsMouseButtonUp(int button) const override;
        bool IsMouseButtonHeld(int button) const override;
        Tbx::Vector2 GetMousePosition() const override;
        Tbx::Vector2 GetMouseDelta() const override;

        bool OnSDLEvent(SDL_Event* event);
    
    private:
        void InitGamepads();
        void RegisterGamepad(SDL_JoystickID gp);
        void CloseGamepads();
        void CloseGamepad(int id);

        std::unordered_map<int, SDL_Gamepad*> _gamepads = {};

        std::unordered_map<int, std::array<bool, SDL_GAMEPAD_BUTTON_COUNT>> _currGamepadBtnState;
        std::unordered_map<int, std::array<bool, SDL_GAMEPAD_BUTTON_COUNT>> _prevGamepadBtnState;

        std::array<Uint8, SDL_SCANCODE_COUNT> _currKeyState{};
        std::array<Uint8, SDL_SCANCODE_COUNT> _prevKeyState{};

        Uint32 _currMouseState = 0;
        Uint32 _prevMouseState = 0;
    };

    TBX_REGISTER_PLUGIN(SDLInputHandlerPlugin);
}
