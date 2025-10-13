#include "SDL3AudioPlugin.h"
#include "Tbx/Audio/Audio.h"
#include "Tbx/Debug/Tracers.h"
#include <SDL3/SDL_init.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

namespace Tbx::Plugins::SDL3Audio
{
    // Extract a positional component from a Tbx::Vector3. SDL only needs the horizontal
    // plane (X/Z) for panning plus the distance for attenuation, so the helper simply maps
    // the fields into indexed access.
    static float ExtractComponent(const Vector3& vector, size_t index)
    {
        switch (index)
        {
        case 0:
            return static_cast<float>(vector.X);
        case 1:
            return static_cast<float>(vector.Y);
        default:
            return static_cast<float>(vector.Z);
        }
    }

    // Calculate stereo gains for a spatialized listener. Distance attenuates the overall
    // volume while the horizontal angle determines a simple left/right pan.
    static StereoSpace CalculateSpatialGains(const Vector3& position)
    {
        const float x = ExtractComponent(position, 0);
        const float y = ExtractComponent(position, 1);
        const float z = ExtractComponent(position, 2);

        // Use an inverse distance rolloff so sounds closer than the reference distance
        // remain at full volume and gradually attenuate as they move away.
        const float distance = std::sqrt(x * x + y * y + z * z);
        constexpr float minDistance = 1.0f;
        constexpr float rolloff = 0.08f;
        const float attenuatedDistance = std::max(distance - minDistance, 0.0f);
        const float attenuation = 1.0f / (1.0f + rolloff * attenuatedDistance);

        // Determine pan by projecting onto the XZ plane and normalising.
        const float horizontal = std::sqrt(x * x + z * z);
        float pan = 0.0f;
        if (horizontal > std::numeric_limits<float>::epsilon())
        {
            pan = std::clamp(x / horizontal, -1.0f, 1.0f);
        }

        // Convert [-1, 1] pan into equal power stereo gains so panning does not change
        // perceived loudness.
        StereoSpace space = {};
        space.Left = attenuation * std::sqrt(std::max(0.0f, 0.5f * (1.0f - pan)));
        space.Right = attenuation * std::sqrt(std::max(0.0f, 0.5f * (1.0f + pan)));
        return space;
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
        SDL_PauseAudioDevice(_device);

        for (auto& [_, playback] : _playbackInstances)
        {
            DestroyPlayback(playback);
        }
        _playbackInstances.clear();

        SDL_CloseAudioDevice(_device);
        _device = 0;

        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        
        // Allow whichever plugin shuts down last to clean up SDL globally.
        if (SDL_WasInit(0) == 0)
        {
            SDL_Quit();
        }
    }

    void SDL3AudioPlugin::Play(const Audio& audio)
    {
        BeginPlayback(audio, nullptr);
    }

    void SDL3AudioPlugin::PlayFromPosition(const Audio& audio, Vector3 position)
    {
        BeginPlayback(audio, &position);
    }

    SpatialSettings SDL3AudioPlugin::ResolveSpatialSettings(const Audio& audio, const Vector3* position) const
    {
        SpatialSettings settings = {};
        settings.Requested = position != nullptr;
        if (!settings.Requested)
        {
            return settings;
        }

        // Spatial playback relies on float samples so we can mix them directly into stereo output.
        const bool formatSupportsSpatial = audio.Format.SampleFormat == AudioSampleFormat::Float32 && audio.Format.Channels > 0;
        if (!formatSupportsSpatial)
        {
            TBX_TRACE_WARNING("SDL3Audio: Spatial playback requested for asset {} but unsupported format was provided.", audio.Id.ToString());
            return settings;
        }

        // Panning requires at least two channels on the output device.
        if (_deviceSpec.channels < 2)
        {
            TBX_TRACE_WARNING("SDL3Audio: Spatial playback requested for asset {} but the audio device is not stereo.", audio.Id.ToString());
            return settings;
        }

        // Pre-calculate the gains so any subsequently queued buffers reuse the same spatial values.
        settings.Enabled = true;
        settings.Gain = CalculateSpatialGains(*position);
        return settings;
    }

    PlaybackReuseResult SDL3AudioPlugin::ReusePlaybackInstance(const Audio& audio, const SpatialSettings& settings)
    {
        auto it = _playbackInstances.find(audio.Id);
        if (it == _playbackInstances.end())
        {
            return PlaybackReuseResult::None;
        }

        auto& instance = it->second;
        if (instance.Stream && instance.Spatial != settings.Enabled)
        {
            // Rebuild the stream if the caller toggled spatial playback so we do not accidentally feed positional data into a mismatched stream layout.
            DestroyPlayback(instance);
            _playbackInstances.erase(it);
            return PlaybackReuseResult::None;
        }

        // Existing streams can continue playback once we update their spatial bookkeeping.
        instance.IsPlaying = true;
        instance.Spatial = settings.Enabled;
        instance.SpatialGain = settings.Enabled ? settings.Gain : StereoSpace{};

        if (!instance.Stream || !instance.Loop)
        {
            return PlaybackReuseResult::Reused;
        }

        const int queued = SDL_GetAudioStreamQueued(instance.Stream);
        if (queued < 0)
        {
            TBX_TRACE_WARNING("SDL3Audio: Failed to query queued audio for asset {}: {}", audio.Id.ToString(), SDL_GetError());
            return PlaybackReuseResult::Reused;
        }

        if (queued > 0)
        {
            return PlaybackReuseResult::Reused;
        }

        if (QueueAudioData(instance, audio))
        {
            return PlaybackReuseResult::Reused;
        }

        // Queueing failures stop playback entirely so we do not leave a corrupted instance behind.
        TBX_TRACE_ERROR("SDL3Audio: Failed to queue looped audio for asset {}.", audio.Id.ToString());
        DestroyPlayback(instance);
        _playbackInstances.erase(it);
        return PlaybackReuseResult::Abort;
    }

    bool SDL3AudioPlugin::PrepareSourceSpec(SDL_AudioSpec& sourceSpec, const Audio& audio, const SpatialSettings& settings) const
    {
        sourceSpec = ConvertFormatToSpec(audio.Format);
        if (sourceSpec.format == 0)
        {
            return false;
        }

        if (settings.Enabled)
        {
            // Force float stereo output from the asset so we can pan and attenuate before SDL converts to the device format.
            sourceSpec.format = SDL_AUDIO_F32;
            sourceSpec.channels = 2;
        }

        return true;
    }

    void SDL3AudioPlugin::BeginPlayback(const Audio& audio, const Vector3* position)
    {
        const SpatialSettings spatial = ResolveSpatialSettings(audio, position);
        switch (ReusePlaybackInstance(audio, spatial))
        {
        case PlaybackReuseResult::Reused:
        case PlaybackReuseResult::Abort:
            return;
        case PlaybackReuseResult::None:
            break;
        }

        const auto& format = audio.Format;
        if (format.SampleFormat == AudioSampleFormat::Unknown || audio.Data.empty())
        {
            TBX_TRACE_WARNING("SDL3Audio: Audio asset {} contains no playable data.", audio.Id.ToString());
            return;
        }

        SDL_AudioSpec sourceSpec = {};
        if (!PrepareSourceSpec(sourceSpec, audio, spatial))
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
        instance.Spatial = spatial.Enabled;
        instance.IsPlaying = true;
        instance.SpatialGain = spatial.Enabled ? spatial.Gain : StereoSpace{};

        if (!QueueAudioData(instance, audio))
        {
            SDL_UnbindAudioStream(stream);
            SDL_DestroyAudioStream(stream);
            return;
        }

        _playbackInstances.emplace(audio.Id, instance);
    }

    void SDL3AudioPlugin::Stop(const Audio& audio)
    {
        auto it = _playbackInstances.find(audio.Id);
        if (it == _playbackInstances.end())
        {
            return;
        }

        it->second.IsPlaying = false;
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
        ApplyStreamTuning(it->second, audio);
    }

    void SDL3AudioPlugin::SetPlaybackSpeed(const Audio& audio, float speed)
    {
        auto it = _playbackInstances.find(audio.Id);
        if (it == _playbackInstances.end())
        {
            return;
        }

        it->second.Speed = speed;
        ApplyStreamTuning(it->second, audio);
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
            if (!QueueAudioData(instance, audio))
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
        ApplyStreamTuning(it->second, audio);
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

    void SDL3AudioPlugin::ApplyStreamTuning(PlaybackInstance& instance, const Audio& audio)
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

    bool SDL3AudioPlugin::QueueAudioData(PlaybackInstance& instance, const Audio& audio)
    {
        if (!instance.Stream || audio.Data.empty())
        {
            return false;
        }

        const auto queueRaw = [&](const void* buffer, size_t size) -> bool
        {
            if (size > static_cast<size_t>(std::numeric_limits<int>::max()))
            {
                TBX_TRACE_ERROR("SDL3Audio: Audio asset {} is too large to queue for playback.", audio.Id.ToString());
                return false;
            }

            // SDL streams are fed with raw bytes and expect an explicit flush to make the
            // new data available to the device.
            if (!SDL_PutAudioStreamData(instance.Stream, buffer, static_cast<int>(size)))
            {
                TBX_TRACE_ERROR("SDL3Audio: Failed to queue audio data: {}", SDL_GetError());
                return false;
            }

            if (!SDL_FlushAudioStream(instance.Stream))
            {
                TBX_TRACE_WARNING("SDL3Audio: Failed to flush audio stream: {}", SDL_GetError());
            }

            return true;
        };

        const auto dataSize = audio.Data.size();
        if (!instance.Spatial)
        {
            return queueRaw(audio.Data.data(), dataSize);
        }

        if (audio.Format.SampleFormat != AudioSampleFormat::Float32)
        {
            TBX_TRACE_ERROR("SDL3Audio: Spatial playback requires float32 audio data for asset {}.", audio.Id.ToString());
            return false;
        }

        // Mix the source channels into a mono signal, then apply stereo gains so we can
        // approximate a 3D position with simple panning and distance attenuation.
        if (dataSize % sizeof(float) != 0)
        {
            TBX_TRACE_ERROR("SDL3Audio: Unexpected audio buffer size for asset {}.", audio.Id.ToString());
            return false;
        }

        const int channels = std::max(audio.Format.Channels, 1);
        const size_t sampleCount = dataSize / sizeof(float);
        if (sampleCount == 0 || sampleCount % static_cast<size_t>(channels) != 0)
        {
            TBX_TRACE_ERROR("SDL3Audio: Spatial playback could not interpret audio samples for asset {}.", audio.Id.ToString());
            return false;
        }

        const size_t frameCount = sampleCount / static_cast<size_t>(channels);
        std::vector<float> processed(frameCount * 2);
        const float leftGain = instance.SpatialGain.Left;
        const float rightGain = instance.SpatialGain.Right;
        const float invChannelCount = 1.0f / static_cast<float>(channels);
        const float* samples = reinterpret_cast<const float*>(audio.Data.data());

        for (size_t frame = 0; frame < frameCount; ++frame)
        {
            float monoSample = 0.0f;
            const size_t baseIndex = frame * static_cast<size_t>(channels);
            for (int channel = 0; channel < channels; ++channel)
            {
                // Average all channels to produce a single sample before applying
                // positional gain to each ear.
                monoSample += samples[baseIndex + static_cast<size_t>(channel)];
            }
            monoSample *= invChannelCount;
            processed[frame * 2] = monoSample * leftGain;
            processed[frame * 2 + 1] = monoSample * rightGain;
        }

        return queueRaw(processed.data(), processed.size() * sizeof(float));
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

    bool SDL3AudioPlugin::IsSupportedExtension(const std::filesystem::path& path)
    {
        const auto extension = path.extension().string();
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
