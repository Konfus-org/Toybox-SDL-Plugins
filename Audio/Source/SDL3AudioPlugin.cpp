#include "SDL3AudioPlugin.h"
#include "Tbx/Audio/Audio.h"
#include "Tbx/Debug/Tracers.h"
#include <SDL3/SDL_init.h>
#include <algorithm>
#include <cctype>
#include <limits>
#include <string>
#include <utility>

namespace Tbx::Plugins::SDL
{
    static std::string ToLowerCopy(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(),
            [](unsigned char ch)
            {
                return static_cast<char>(std::tolower(ch));
            });
        return value;
    }

    SDL3AudioPlugin::SDL3AudioPlugin(Ref<EventBus> eventBus)
    {
        if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
        {
            TBX_TRACE_ERROR("SDL3Audio: Failed to initialize SDL audio subsystem: {}", SDL_GetError());
        }

        SDL_AudioSpec desired = {};
        desired.format = SDL_AUDIO_F32;
        desired.channels = 2;
        desired.freq = 48000;

        _device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &desired);
        if (_device == 0)
        {
            TBX_TRACE_ERROR("SDL3Audio: Failed to open SDL audio device: {}", SDL_GetError());
        }

        if (!SDL_GetAudioDeviceFormat(_device, &_deviceSpec, nullptr))
        {
            TBX_TRACE_ERROR("SDL3Audio: Failed to query audio device format: {}", SDL_GetError());
            SDL_CloseAudioDevice(_device);
        }

        if (!SDL_ResumeAudioDevice(_device))
        {
            TBX_TRACE_WARNING("SDL3Audio: Unable to resume audio device: {}", SDL_GetError());
        }
    }

    SDL3AudioPlugin::~SDL3AudioPlugin()
    {
        for (auto& [_, playback] : _playbackInstances)
        {
            DestroyPlayback(playback);
        }
        _playbackInstances.clear();

        if (_device != 0)
        {
            SDL_PauseAudioDevice(_device);
            SDL_CloseAudioDevice(_device);
            _device = 0;
        }

        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }

    void SDL3AudioPlugin::DestroyPlayback(PlaybackInstance& instance)
    {
        if (!instance.Stream)
        {
            return;
        }

        SDL_UnbindAudioStream(instance.Stream);
        SDL_ClearAudioStream(instance.Stream);
        SDL_DestroyAudioStream(instance.Stream);
        instance.Stream = nullptr;
    }

    void SDL3AudioPlugin::Play(const Audio& audio)
    {
        auto it = _playbackInstances.find(audio.Id);
        if (it != _playbackInstances.end())
        {
            auto& instance = it->second;
            ApplyStreamTuning(instance);

            if (instance.Stream && instance.Loop)
            {
                const int queued = SDL_GetAudioStreamQueued(instance.Stream);
                if (queued == 0)
                {
                    if (!QueueAudioData(instance.Stream, audio))
                    {
                        TBX_TRACE_ERROR("SDL3Audio: Failed to queue looped audio for asset {}.", audio.Id.ToString());
                        DestroyPlayback(instance);
                        _playbackInstances.erase(it);
                        return;
                    }
                }
                else if (queued < 0)
                {
                    TBX_TRACE_WARNING("SDL3Audio: Failed to query queued audio for asset {}: {}", audio.Id.ToString(), SDL_GetError());
                }
            }
            return;
        }

        const auto& format = audio.Format;
        if (format.SampleFormat == AudioSampleFormat::Unknown || audio.Data.empty())
        {
            TBX_TRACE_WARNING("SDL3Audio: Audio asset {} contains no playable data.", audio.Id.ToString());
            return;
        }

        SDL_AudioSpec sourceSpec = ConvertFormatToSpec(format);
        if (sourceSpec.format == 0)
        {
            TBX_TRACE_WARNING("SDL3Audio: Unsupported audio sample format for asset {}.", audio.Id.ToString());
            return;
        }

        SDL_AudioStream* stream = SDL_CreateAudioStream(&sourceSpec, &_deviceSpec);
        if (stream == nullptr)
        {
            TBX_TRACE_ERROR("SDL3Audio: Failed to create audio stream: {}", SDL_GetError());
            return;
        }

        if (!SDL_BindAudioStream(_device, stream))
        {
            TBX_TRACE_ERROR("SDL3Audio: Failed to bind audio stream: {}", SDL_GetError());
            SDL_DestroyAudioStream(stream);
            return;
        }

        PlaybackInstance instance = {};
        instance.Stream = stream;
        if (!QueueAudioData(instance.Stream, audio))
        {
            SDL_UnbindAudioStream(stream);
            SDL_DestroyAudioStream(stream);
            return;
        }
        ApplyStreamTuning(instance);

        _playbackInstances.emplace(audio.Id, instance);
    }

    void SDL3AudioPlugin::Stop(const Audio& audio)
    {
        auto it = _playbackInstances.find(audio.Id);
        if (it == _playbackInstances.end())
        {
            return;
        }

        DestroyPlayback(it->second);
        _playbackInstances.erase(it);
    }

    void SDL3AudioPlugin::SetPitch(const Audio& audio, float pitch)
    {
        auto it = _playbackInstances.find(audio.Id);
        if (it == _playbackInstances.end())
        {
            return;
        }

        it->second.Pitch = pitch;
        ApplyStreamTuning(it->second);
    }

    void SDL3AudioPlugin::SetPlaybackSpeed(const Audio& audio, float speed)
    {
        auto it = _playbackInstances.find(audio.Id);
        if (it == _playbackInstances.end())
        {
            return;
        }

        it->second.Speed = speed;
        ApplyStreamTuning(it->second);
    }

    void SDL3AudioPlugin::SetLooping(const Audio& audio, bool loop)
    {
        auto it = _playbackInstances.find(audio.Id);
        if (it == _playbackInstances.end())
        {
            return;
        }

        auto& instance = it->second;
        instance.Loop = loop;

        if (!instance.Stream || !instance.Loop)
        {
            return;
        }

        const int queued = SDL_GetAudioStreamQueued(instance.Stream);
        if (queued == 0)
        {
            if (!QueueAudioData(instance.Stream, audio))
            {
                TBX_TRACE_ERROR("SDL3Audio: Failed to queue looped audio for asset {}.", audio.Id.ToString());
                DestroyPlayback(instance);
                _playbackInstances.erase(it);
                return;
            }
        }
        else if (queued < 0)
        {
            TBX_TRACE_WARNING("SDL3Audio: Failed to query queued audio for asset {}: {}", audio.Id.ToString(), SDL_GetError());
        }
    }

    void SDL3AudioPlugin::SetVolume(const Audio& audio, float volume)
    {
        auto it = _playbackInstances.find(audio.Id);
        if (it == _playbackInstances.end())
        {
            return;
        }

        it->second.Volume = volume;
        ApplyStreamTuning(it->second);
    }

    bool SDL3AudioPlugin::CanLoad(const std::filesystem::path& filepath) const
    {
        return IsSupportedExtension(filepath);
    }

    Audio SDL3AudioPlugin::LoadAudio(const std::filesystem::path& filepath)
    {
        SDL_AudioSpec sourceSpec = {};
        Uint8* rawBuffer = nullptr;
        Uint32 rawLength = 0;

        if (filepath.extension() == ".wav" || filepath.extension() == ".wave")
        {
            if (!SDL_LoadWAV(filepath.string().c_str(), &sourceSpec, &rawBuffer, &rawLength))
            {
                TBX_TRACE_ERROR("SDL3Audio: Failed to load '{}': {}", filepath.string(), SDL_GetError());
                return Audio();
            }
        }
        else
        {
            TBX_ASSERT(false, "SDL3Audio: Unsupported audio file format.");
            return Audio();
        }

        SDL_AudioSpec targetSpec = sourceSpec;
        targetSpec.format = SDL_AUDIO_F32;

        Uint8* convertedBuffer = nullptr;
        int convertedLength = 0;
        bool converted = SDL_ConvertAudioSamples(&sourceSpec, rawBuffer, static_cast<int>(rawLength), &targetSpec, &convertedBuffer, &convertedLength);
        SDL_free(rawBuffer);

        if (!converted)
        {
            TBX_TRACE_ERROR("SDL3Audio: Failed to convert audio '{}': {}", filepath.string(), SDL_GetError());
            return Audio();
        }

        AudioFormat format = ConvertSpecToFormat(targetSpec);
        Audio::SampleData samples(convertedBuffer, convertedBuffer + convertedLength);
        SDL_free(convertedBuffer);

        return Audio(std::move(samples), format);
    }

    void SDL3AudioPlugin::ApplyStreamTuning(PlaybackInstance& instance) const
    {
        if (!instance.Stream)
        {
            return;
        }

        const float ratio = std::clamp(instance.Pitch * instance.Speed, 0.01f, 100.0f);
        if (!SDL_SetAudioStreamFrequencyRatio(instance.Stream, ratio))
        {
            TBX_TRACE_WARNING("SDL3Audio: Failed to adjust audio stream playback ratio: {}", SDL_GetError());
        }

        if (!SDL_SetAudioStreamGain(instance.Stream, instance.Volume))
        {
            TBX_TRACE_WARNING("SDL3Audio: Failed to adjust audio stream volume: {}", SDL_GetError());
        }
    }

    bool SDL3AudioPlugin::QueueAudioData(SDL_AudioStream* stream, const Audio& audio)
    {
        if (!stream)
        {
            return false;
        }

        if (audio.Data.empty())
        {
            return false;
        }

        const auto dataSize = audio.Data.size();
        if (dataSize > static_cast<size_t>(std::numeric_limits<int>::max()))
        {
            TBX_TRACE_ERROR("SDL3Audio: Audio asset {} is too large to queue for playback.", audio.Id.ToString());
            return false;
        }

        if (!SDL_PutAudioStreamData(stream, audio.Data.data(), static_cast<int>(dataSize)))
        {
            TBX_TRACE_ERROR("SDL3Audio: Failed to queue audio data: {}", SDL_GetError());
            return false;
        }

        if (!SDL_FlushAudioStream(stream))
        {
            TBX_TRACE_WARNING("SDL3Audio: Failed to flush audio stream: {}", SDL_GetError());
        }

        return true;
    }

    bool SDL3AudioPlugin::IsSupportedExtension(const std::filesystem::path& path)
    {
        const auto extension = ToLowerCopy(path.extension().string());
        return extension == ".wav" || extension == ".wave";
    }

    AudioFormat SDL3AudioPlugin::ConvertSpecToFormat(const SDL_AudioSpec& spec)
    {
        AudioSampleFormat sampleFormat = AudioSampleFormat::Unknown;
        switch (spec.format)
        {
        case SDL_AUDIO_U8:
            sampleFormat = AudioSampleFormat::UInt8;
            break;
        case SDL_AUDIO_S16:
            sampleFormat = AudioSampleFormat::Int16;
            break;
        case SDL_AUDIO_S32:
            sampleFormat = AudioSampleFormat::Int32;
            break;
        case SDL_AUDIO_F32:
            sampleFormat = AudioSampleFormat::Float32;
            break;
        default:
            sampleFormat = AudioSampleFormat::Unknown;
            break;
        }

        AudioFormat format = {};
        format.SampleFormat = sampleFormat;
        format.SampleRate = spec.freq;
        format.Channels = spec.channels;
        return format;
    }

    SDL_AudioSpec SDL3AudioPlugin::ConvertFormatToSpec(const AudioFormat& format)
    {
        SDL_AudioSpec spec = {};
        spec.freq = format.SampleRate;
        spec.channels = static_cast<Uint8>(std::max(0, format.Channels));

        switch (format.SampleFormat)
        {
        case AudioSampleFormat::UInt8:
            spec.format = SDL_AUDIO_U8;
            break;
        case AudioSampleFormat::Int16:
            spec.format = SDL_AUDIO_S16;
            break;
        case AudioSampleFormat::Int32:
            spec.format = SDL_AUDIO_S32;
            break;
        case AudioSampleFormat::Float32:
            spec.format = SDL_AUDIO_F32;
            break;
        default:
            TBX_ASSERT(false, "SDL3Audio: Unsupported audio sample format.");
            spec.format = SDL_AUDIO_UNKNOWN;
            break;
        }

        return spec;
    }
}
