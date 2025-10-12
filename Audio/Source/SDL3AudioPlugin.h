#pragma once
#include <Tbx/Audio/AudioMixer.h>
#include <Tbx/Assets/AssetLoaders.h>
#include <Tbx/Plugins/Plugin.h>
#include <SDL3/SDL_audio.h>
#include <filesystem>
#include <unordered_map>

namespace Tbx::Plugins::SDL
{
    class SDL3AudioPlugin final
        : public Plugin
        , public IAudioMixer
        , public IAudioLoader
    {
    public:
        explicit SDL3AudioPlugin(Ref<EventBus> eventBus);
        ~SDL3AudioPlugin() override;

        // IAudioMixer
        void Play(const Audio& audio) override;
        void Stop(const Audio& audio) override;
        void SetPitch(const Audio& audio, float pitch) override;
        void SetPlaybackSpeed(const Audio& audio, float speed) override;
        void SetLooping(const Audio& audio, bool loop) override;
        void SetVolume(const Audio& audio, float volume) override;

        // IAudioLoader
        bool CanLoad(const std::filesystem::path& filepath) const override;

    protected:
        Audio LoadAudio(const std::filesystem::path& filepath) override;

    private:
        struct PlaybackInstance
        {
            SDL_AudioStream* Stream = nullptr;
            float Pitch = 1.0f;
            float Speed = 1.0f;
            float Volume = 1.0f;
            bool Loop = false;
            bool IsPlaying = false;
        };

        void ApplyStreamTuning(PlaybackInstance& instance, const Audio& audio);
        bool QueueAudioData(SDL_AudioStream* stream, const Audio& audio);
        void DestroyPlayback(PlaybackInstance& instance);

        static bool IsSupportedExtension(const std::filesystem::path& path);
        static AudioFormat ConvertSpecToFormat(const SDL_AudioSpec& spec);
        static SDL_AudioSpec ConvertFormatToSpec(const AudioFormat& format);

    private:
        SDL_AudioDeviceID _device = 0;
        SDL_AudioSpec _deviceSpec = {};
        std::unordered_map<Uid, PlaybackInstance> _playbackInstances = {};
    };

    TBX_REGISTER_PLUGIN(SDL3AudioPlugin);
}
