# ALIA Audio Interfaces

This document defines the C++ concepts, types, and functions for the ALIA audio subsystem.

---

## Table of Contents

- [ALIA Audio Interfaces](#alia-audio-interfaces)
  - [Table of Contents](#table-of-contents)
  - [Design Principles](#design-principles)
  - [Core Types](#core-types)
    - [Audio Units](#audio-units)
    - [Channel Configuration](#channel-configuration)
    - [Fragment Types](#fragment-types)
  - [Concepts](#concepts)
    - [Audio Fragment Concepts](#audio-fragment-concepts)
    - [Audio Source Concepts](#audio-source-concepts)
  - [Audio Device](#audio-device)
    - [Device Class](#device-class)
  - [Sample](#sample)
    - [Sample Class (Loaded Audio Data)](#sample-class-loaded-audio-data)
  - [Audio Stream](#audio-stream)
    - [Stream Class (Streaming Audio)](#stream-class-streaming-audio)
  - [Audio Recorder](#audio-recorder)
    - [Recorder Class](#recorder-class)
  - [Mixer](#mixer)
    - [Mixer Class](#mixer-class)
  - [Voice](#voice)
    - [Voice Class (Individual Sound Instance)](#voice-class-individual-sound-instance)
  - [Audio Pipeline](#audio-pipeline)
    - [Pipeline (Audio Flow Graph)](#pipeline-audio-flow-graph)
  - [DSP Building Blocks](#dsp-building-blocks)
    - [Filters](#filters)
    - [Convolution](#convolution)
    - [FFT and STFT](#fft-and-stft)
    - [Envelope and Dynamics](#envelope-and-dynamics)
  - [Ring Buffer](#ring-buffer)
    - [Thread-Safe Ring Buffer](#thread-safe-ring-buffer)
  - [Utilities](#utilities)
    - [Channel Conversion](#channel-conversion)
  - [Backend Interface](#backend-interface)
    - [Audio Backend Interface](#audio-backend-interface)
  - [Example Usage](#example-usage)
    - [Simple Audio Playback](#simple-audio-playback)
    - [Real-Time Audio Processing](#real-time-audio-processing)
    - [Audio Recording with Processing](#audio-recording-with-processing)

---

## Design Principles

Following libsndfile terminology:
- **Sample**: A single scalar audio value (float, int16, etc.)
- **Fragment**: A vector of samples across all channels (one sample per channel)
- **Frame**: Sometimes used interchangeably with fragment

API Guidelines:
- All APIs use `std::span<TFragment>` to avoid forcing allocations
- Fragment types: scalars (mono), `Vec2<T>` (stereo), `Vec3<T>`, `Vec4<T>`, `std::array<T, N>`
- All audio processing is based on fragments, not interleaved samples

---

## Core Types

### Audio Units

```cpp
namespace alia::audio {

// Frequency (Hz)
class Hz {
public:
    constexpr Hz() = default;
    constexpr explicit Hz(double value) : value_(value) {}
    
    [[nodiscard]] constexpr double get() const { return value_; }
    [[nodiscard]] constexpr double get_hz() const { return value_; }
    [[nodiscard]] constexpr double get_khz() const { return value_ / 1000.0; }
    
    // Conversion to period
    [[nodiscard]] constexpr double period_seconds() const { return 1.0 / value_; }
    [[nodiscard]] constexpr double period_ms() const { return 1000.0 / value_; }
    
    // Arithmetic
    constexpr Hz operator+(Hz rhs) const { return Hz(value_ + rhs.value_); }
    constexpr Hz operator-(Hz rhs) const { return Hz(value_ - rhs.value_); }
    constexpr Hz operator*(double scalar) const { return Hz(value_ * scalar); }
    constexpr Hz operator/(double scalar) const { return Hz(value_ / scalar); }
    
    constexpr bool operator==(Hz rhs) const = default;
    constexpr auto operator<=>(Hz rhs) const = default;
    
private:
    double value_ = 0.0;
};

// User-defined literals
namespace literals {
    constexpr Hz operator""_Hz(long double val) { return Hz(static_cast<double>(val)); }
    constexpr Hz operator""_Hz(unsigned long long val) { return Hz(static_cast<double>(val)); }
    constexpr Hz operator""_kHz(long double val) { return Hz(static_cast<double>(val) * 1000.0); }
    constexpr Hz operator""_kHz(unsigned long long val) { return Hz(static_cast<double>(val) * 1000.0); }
}

// Decibels
class dB {
public:
    constexpr dB() = default;
    constexpr explicit dB(double value) : value_(value) {}
    
    [[nodiscard]] constexpr double get() const { return value_; }
    
    // Convert to linear gain
    [[nodiscard]] double to_linear() const { 
        return std::pow(10.0, value_ / 20.0); 
    }
    
    // Create from linear gain
    [[nodiscard]] static dB from_linear(double linear) {
        return dB(20.0 * std::log10(linear));
    }
    
    // Arithmetic
    constexpr dB operator+(dB rhs) const { return dB(value_ + rhs.value_); }
    constexpr dB operator-(dB rhs) const { return dB(value_ - rhs.value_); }
    
private:
    double value_ = 0.0;
};

namespace literals {
    constexpr dB operator""_dB(long double val) { return dB(static_cast<double>(val)); }
    constexpr dB operator""_dB(unsigned long long val) { return dB(static_cast<double>(val)); }
}

// Sample rate type
using SampleRate = Hz;

// Common sample rates
namespace sample_rates {
    inline constexpr SampleRate CD = Hz(44100);
    inline constexpr SampleRate DVD = Hz(48000);
    inline constexpr SampleRate HiRes = Hz(96000);
    inline constexpr SampleRate UltraHiRes = Hz(192000);
}

} // namespace alia::audio
```

### Channel Configuration

```cpp
namespace alia::audio {

// Channel configuration tags
struct Mono {};
struct Stereo {};
struct Surround51 {};
struct Surround71 {};

// Channel count trait
template<typename Config>
struct ChannelCount;

template<> struct ChannelCount<Mono> { static constexpr int value = 1; };
template<> struct ChannelCount<Stereo> { static constexpr int value = 2; };
template<> struct ChannelCount<Surround51> { static constexpr int value = 6; };
template<> struct ChannelCount<Surround71> { static constexpr int value = 8; };

template<typename Config>
inline constexpr int channel_count_v = ChannelCount<Config>::value;

// Audio depth (bit depth)
enum class SampleFormat {
    Int16,      // 16-bit signed integer
    Int24,      // 24-bit signed integer (packed)
    Int32,      // 32-bit signed integer
    Float32,    // 32-bit floating point
    Float64,    // 64-bit floating point
};

// Sample format traits
template<SampleFormat F>
struct SampleFormatTraits;

template<> struct SampleFormatTraits<SampleFormat::Int16> {
    using type = int16_t;
    static constexpr int bits = 16;
    static constexpr double max_value = 32767.0;
};

template<> struct SampleFormatTraits<SampleFormat::Int32> {
    using type = int32_t;
    static constexpr int bits = 32;
    static constexpr double max_value = 2147483647.0;
};

template<> struct SampleFormatTraits<SampleFormat::Float32> {
    using type = float;
    static constexpr int bits = 32;
    static constexpr double max_value = 1.0;
};

template<> struct SampleFormatTraits<SampleFormat::Float64> {
    using type = double;
    static constexpr int bits = 64;
    static constexpr double max_value = 1.0;
};

} // namespace alia::audio
```

### Fragment Types

```cpp
namespace alia::audio {

// Fragment type for N channels
template<typename SampleT, int NumChannels>
struct Fragment {
    std::array<SampleT, NumChannels> channels;
    
    SampleT& operator[](int i) { return channels[i]; }
    const SampleT& operator[](int i) const { return channels[i]; }
    
    // Arithmetic
    Fragment operator+(const Fragment& rhs) const {
        Fragment result;
        for (int i = 0; i < NumChannels; ++i)
            result.channels[i] = channels[i] + rhs.channels[i];
        return result;
    }
    
    Fragment operator*(SampleT scalar) const {
        Fragment result;
        for (int i = 0; i < NumChannels; ++i)
            result.channels[i] = channels[i] * scalar;
        return result;
    }
    
    // Mix (add)
    Fragment& operator+=(const Fragment& rhs) {
        for (int i = 0; i < NumChannels; ++i)
            channels[i] += rhs.channels[i];
        return *this;
    }
};

// Specialization: Mono (single sample)
template<typename SampleT>
using MonoFragment = SampleT;

// Specialization: Stereo (Vec2)
template<typename SampleT>
using StereoFragment = Vec2<SampleT>;

// Type alias helpers
using FragmentMono = float;
using FragmentStereo = Vec2<float>;
using FragmentSurround51 = Fragment<float, 6>;
using FragmentSurround71 = Fragment<float, 8>;

// Fragment type from channel config
template<typename SampleT, typename ChannelConfig>
struct FragmentType;

template<typename SampleT>
struct FragmentType<SampleT, Mono> { using type = SampleT; };

template<typename SampleT>
struct FragmentType<SampleT, Stereo> { using type = Vec2<SampleT>; };

template<typename SampleT, typename Config>
using fragment_t = typename FragmentType<SampleT, Config>::type;

} // namespace alia::audio
```

---

## Concepts

### Audio Fragment Concepts

```cpp
namespace alia::audio {

// Check if T is a valid sample type
template<typename T>
concept SampleType = 
    std::same_as<T, float> ||
    std::same_as<T, double> ||
    std::same_as<T, int16_t> ||
    std::same_as<T, int32_t>;

// Check if T is a valid mono fragment (just a sample)
template<typename T>
concept MonoFragmentType = SampleType<T>;

// Check if T is a valid stereo fragment
template<typename T>
concept StereoFragmentType = 
    requires(T frag) {
        { frag.x } -> SampleType;
        { frag.y } -> SampleType;
    } ||
    (requires(T frag) {
        { frag[0] } -> SampleType;
        { frag[1] } -> SampleType;
    } && std::tuple_size_v<T> == 2);

// Check if T is a valid multi-channel fragment
template<typename T>
concept MultiChannelFragmentType = 
    requires(T frag, int i) {
        { frag[i] } -> SampleType;
    };

// Any valid fragment type
template<typename T>
concept FragmentType = 
    MonoFragmentType<T> || 
    StereoFragmentType<T> || 
    MultiChannelFragmentType<T>;

// Get channel count from fragment type
template<typename T>
constexpr int fragment_channels() {
    if constexpr (MonoFragmentType<T>) {
        return 1;
    } else if constexpr (StereoFragmentType<T>) {
        return 2;
    } else if constexpr (requires { std::tuple_size_v<T>; }) {
        return std::tuple_size_v<T>;
    } else if constexpr (requires(T t) { t.channels.size(); }) {
        return std::tuple_size_v<decltype(std::declval<T>().channels)>;
    } else {
        return 1; // Fallback
    }
}

// Check if types are compatible fragments
template<typename T, typename U>
concept CompatibleFragments = 
    FragmentType<T> && FragmentType<U> &&
    (fragment_channels<T>() == fragment_channels<U>());

} // namespace alia::audio
```

### Audio Source Concepts

```cpp
namespace alia::audio {

// Something that produces audio fragments
template<typename T, typename FragT>
concept AudioSource = requires(T source, std::span<FragT> buffer) {
    { source.read(buffer) } -> std::convertible_to<size_t>;
    { source.sample_rate() } -> std::convertible_to<Hz>;
};

// Something that consumes audio fragments
template<typename T, typename FragT>
concept AudioSink = requires(T sink, std::span<const FragT> buffer) {
    { sink.write(buffer) } -> std::convertible_to<size_t>;
    { sink.sample_rate() } -> std::convertible_to<Hz>;
};

// Something that can be played
template<typename T>
concept Playable = requires(T obj) {
    obj.play();
    obj.stop();
    obj.pause();
    { obj.is_playing() } -> std::convertible_to<bool>;
};

// Something with adjustable gain
template<typename T>
concept HasGain = requires(T obj, float gain) {
    obj.set_gain(gain);
    { obj.get_gain() } -> std::convertible_to<float>;
};

// Something with adjustable pan (stereo position)
template<typename T>
concept HasPan = requires(T obj, float pan) {
    obj.set_pan(pan);  // -1.0 = left, 0.0 = center, 1.0 = right
    { obj.get_pan() } -> std::convertible_to<float>;
};

} // namespace alia::audio
```

---

## Audio Device

### Device Class

```cpp
namespace alia::audio {

// Device type
enum class DeviceType {
    Output,     // Playback device
    Input,      // Recording device
};

// Device info
struct DeviceInfo {
    std::string name;
    std::string id;
    DeviceType type;
    int max_channels;
    std::vector<SampleRate> supported_rates;
    bool is_default;
};

// Audio device
class Device {
public:
    Device() = default;
    Device(Device&&) noexcept;
    Device& operator=(Device&&) noexcept;
    ~Device();
    
    // Open default device
    [[nodiscard]] static Device open_default(DeviceType type, 
                                              SampleRate rate = sample_rates::CD,
                                              int channels = 2);
    
    // Open specific device
    [[nodiscard]] static Device open(const DeviceInfo& info,
                                      SampleRate rate,
                                      int channels);
    
    // Enumerate available devices
    [[nodiscard]] static std::vector<DeviceInfo> enumerate(DeviceType type);
    
    // Get default device info
    [[nodiscard]] static DeviceInfo get_default(DeviceType type);
    
    [[nodiscard]] bool valid() const;
    [[nodiscard]] explicit operator bool() const { return valid(); }
    
    // Properties
    [[nodiscard]] SampleRate sample_rate() const;
    [[nodiscard]] int channels() const;
    [[nodiscard]] DeviceType type() const;
    [[nodiscard]] const DeviceInfo& info() const;
    
    // Latency
    [[nodiscard]] double latency_seconds() const;
    [[nodiscard]] int latency_samples() const;
    
    // Backend handle
    [[nodiscard]] void* native_handle() const;
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Get the default output device (singleton for convenience)
Device& get_default_device();

} // namespace alia::audio
```

---

## Sample

### Sample Class (Loaded Audio Data)

```cpp
namespace alia::audio {

// Play mode
enum class PlayMode {
    Once,       // Play once and stop
    Loop,       // Loop infinitely
    BiDir,      // Ping-pong loop
};

// Sample (pre-loaded audio data in memory)
class Sample {
public:
    Sample() = default;
    Sample(Sample&&) noexcept;
    Sample& operator=(Sample&&) noexcept;
    ~Sample();
    
    // Load from file
    [[nodiscard]] static Sample load(std::string_view path);
    
    // Load from memory
    [[nodiscard]] static Sample load(std::span<const std::byte> data);
    
    // Create from raw audio data
    template<FragmentType FragT>
    [[nodiscard]] static Sample from_data(std::span<const FragT> fragments, SampleRate rate);
    
    [[nodiscard]] bool valid() const;
    [[nodiscard]] explicit operator bool() const { return valid(); }
    
    // Properties
    [[nodiscard]] SampleRate sample_rate() const;
    [[nodiscard]] int channels() const;
    [[nodiscard]] size_t fragment_count() const;
    [[nodiscard]] double duration_seconds() const;
    
    // Quick play (fire and forget, uses default mixer)
    void play(float gain = 1.0f, float speed = 1.0f, float pan = 0.0f) const;
    void play_looped(float gain = 1.0f, float speed = 1.0f) const;
    
    // Get raw data (for custom processing)
    template<FragmentType FragT>
    [[nodiscard]] std::span<const FragT> data() const;
    
    // Backend handle
    [[nodiscard]] void* native_handle() const;
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Reserve sample playback slots (call early for fire-and-forget play())
void reserve_samples(int count);

} // namespace alia::audio
```

---

## Audio Stream

### Stream Class (Streaming Audio)

```cpp
namespace alia::audio {

// Stream configuration
struct StreamConfig {
    int num_chunks = 4;          // Number of buffers
    int fragments_per_chunk = 2048;  // Fragments per buffer
};

// Audio stream (for streaming playback, e.g., music)
template<typename SampleT, typename ChannelConfig>
class AudioStream {
public:
    using fragment_type = fragment_t<SampleT, ChannelConfig>;
    
    AudioStream() = default;
    AudioStream(SampleRate rate, StreamConfig config = {});
    
    AudioStream(AudioStream&&) noexcept;
    AudioStream& operator=(AudioStream&&) noexcept;
    ~AudioStream();
    
    // Open from file (streaming, not fully loaded)
    [[nodiscard]] static AudioStream open(std::string_view path);
    [[nodiscard]] static AudioStream open(std::string_view path, StreamConfig config);
    
    [[nodiscard]] bool valid() const;
    [[nodiscard]] explicit operator bool() const { return valid(); }
    
    // Properties
    [[nodiscard]] SampleRate sample_rate() const;
    [[nodiscard]] int channels() const;
    [[nodiscard]] double duration_seconds() const;  // May be unknown for some streams
    [[nodiscard]] double position_seconds() const;
    
    // Playback control
    void play();
    void stop();
    void pause();
    void rewind();
    void seek(double seconds);
    
    [[nodiscard]] bool is_playing() const;
    [[nodiscard]] bool is_paused() const;
    
    // Playback settings
    void set_play_mode(PlayMode mode);
    void set_gain(float gain);
    void set_speed(float speed);  // 1.0 = normal
    void set_pan(float pan);      // -1.0 to 1.0
    
    [[nodiscard]] float get_gain() const;
    [[nodiscard]] float get_speed() const;
    [[nodiscard]] float get_pan() const;
    [[nodiscard]] PlayMode get_play_mode() const;
    
    // For custom streaming: chunk ready callback
    // Called when a chunk needs to be filled with audio data
    using ChunkCallback = std::function<void(std::span<fragment_type> buffer)>;
    void set_chunk_callback(ChunkCallback callback);
    
    // Create event handler for event-driven filling
    // Returns a handler that can be registered with EventDispatcher
    std::function<void(const Event&)> create_chunk_event_handler(ChunkCallback callback);
    
    // Event source for stream events
    EventSource& get_event_source();
    
    // Backend handle
    [[nodiscard]] void* native_handle() const;
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Common instantiations
using MonoStream = AudioStream<float, Mono>;
using StereoStream = AudioStream<float, Stereo>;
using BaseAudioStream = StereoStream;  // Default

// Helper to load an audio stream (auto-detects format)
BaseAudioStream load_audio_stream(std::string_view path);

} // namespace alia::audio
```

---

## Audio Recorder

### Recorder Class

```cpp
namespace alia::audio {

// Recorder configuration
struct RecorderConfig {
    int num_chunks = 16;
    int fragments_per_chunk = 1024;
};

// Audio recorder
template<typename SampleT, typename ChannelConfig>
class AudioRecorder {
public:
    using fragment_type = fragment_t<SampleT, ChannelConfig>;
    
    AudioRecorder() = default;
    AudioRecorder(SampleRate rate, RecorderConfig config = {});
    
    AudioRecorder(AudioRecorder&&) noexcept;
    AudioRecorder& operator=(AudioRecorder&&) noexcept;
    ~AudioRecorder();
    
    [[nodiscard]] bool valid() const;
    [[nodiscard]] explicit operator bool() const { return valid(); }
    
    // Properties
    [[nodiscard]] SampleRate sample_rate() const;
    [[nodiscard]] int channels() const;
    
    // Recording control
    void start();
    void stop();
    void pause();
    void resume();
    
    [[nodiscard]] bool is_recording() const;
    [[nodiscard]] bool is_paused() const;
    
    // Chunk ready callback
    using ChunkCallback = std::function<void(std::span<const fragment_type> buffer)>;
    void set_chunk_callback(ChunkCallback callback);
    
    // Create event handler for event-driven processing
    std::function<void(const Event&)> create_chunk_event_handler(ChunkCallback callback);
    
    // Event source for recorder events
    EventSource& get_event_source();
    
    // Backend handle
    [[nodiscard]] void* native_handle() const;
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Common instantiations
using MonoRecorder = AudioRecorder<float, Mono>;
using StereoRecorder = AudioRecorder<float, Stereo>;

} // namespace alia::audio
```

---

## Mixer

### Mixer Class

```cpp
namespace alia::audio {

// Mixer (combines multiple audio sources)
template<typename SampleT, typename ChannelConfig>
class Mixer {
public:
    using fragment_type = fragment_t<SampleT, ChannelConfig>;
    
    Mixer() = default;
    Mixer(SampleRate rate);
    
    Mixer(Mixer&&) noexcept;
    Mixer& operator=(Mixer&&) noexcept;
    ~Mixer();
    
    [[nodiscard]] bool valid() const;
    [[nodiscard]] explicit operator bool() const { return valid(); }
    
    // Properties
    [[nodiscard]] SampleRate sample_rate() const;
    [[nodiscard]] int channels() const;
    
    // Attach sources
    void attach(AudioStream<SampleT, ChannelConfig>& stream);
    void attach(Sample& sample);  // Creates internal sample instance
    void attach(Mixer& sub_mixer);
    
    // Detach sources
    void detach(AudioStream<SampleT, ChannelConfig>& stream);
    void detach(Sample& sample);
    void detach(Mixer& sub_mixer);
    
    // Master gain
    void set_gain(float gain);
    [[nodiscard]] float get_gain() const;
    
    // Post-process callback (for effects, metering, etc.)
    using PostProcessCallback = std::function<void(std::span<fragment_type> buffer)>;
    void set_postprocess_callback(PostProcessCallback callback);
    
    // Backend handle
    [[nodiscard]] void* native_handle() const;
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Common instantiations
using MonoMixer = Mixer<float, Mono>;
using StereoMixer = Mixer<float, Stereo>;
using UserMixer = StereoMixer;

// Default mixer (singleton, attached to default device)
StereoMixer& get_default_mixer();

} // namespace alia::audio
```

---

## Voice

### Voice Class (Individual Sound Instance)

```cpp
namespace alia::audio {

// Voice (represents a single playing sound)
class Voice {
public:
    Voice() = default;
    Voice(Voice&&) noexcept;
    Voice& operator=(Voice&&) noexcept;
    ~Voice();
    
    // Create attached to default device
    Voice(SampleRate rate);
    
    [[nodiscard]] bool valid() const;
    [[nodiscard]] explicit operator bool() const { return valid(); }
    
    // Properties
    [[nodiscard]] SampleRate sample_rate() const;
    [[nodiscard]] int channels() const;
    
    // Attach mixer or stream to voice
    void attach(StereoMixer& mixer);
    
    // Backend handle
    [[nodiscard]] void* native_handle() const;
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace alia::audio
```

---

## Audio Pipeline

### Pipeline (Audio Flow Graph)

```cpp
namespace alia::audio {

// Audio node interface
class IAudioNode {
public:
    virtual ~IAudioNode() = default;
    
    virtual void process(std::span<const float> input, std::span<float> output) = 0;
    virtual int input_channels() const = 0;
    virtual int output_channels() const = 0;
    virtual SampleRate sample_rate() const = 0;
};

// Pipeline connection
struct PipelineConnection {
    IAudioNode* source;
    int source_output;
    IAudioNode* dest;
    int dest_input;
};

// Audio pipeline (flow graph)
class Pipeline {
public:
    Pipeline() = default;
    Pipeline(SampleRate rate);
    
    Pipeline(Pipeline&&) noexcept;
    Pipeline& operator=(Pipeline&&) noexcept;
    ~Pipeline();
    
    [[nodiscard]] bool valid() const;
    
    // Add nodes
    template<typename NodeT, typename... Args>
    NodeT& add_node(Args&&... args);
    
    void remove_node(IAudioNode& node);
    
    // Connect nodes
    void connect(IAudioNode& source, IAudioNode& dest);
    void connect(IAudioNode& source, int output, IAudioNode& dest, int input);
    
    // Disconnect
    void disconnect(IAudioNode& source, IAudioNode& dest);
    void disconnect_all(IAudioNode& node);
    
    // Set output (final destination)
    void set_output(Device& device);
    void set_output(Mixer<float, Stereo>& mixer);
    
    // Processing
    void process();  // Process one buffer
    void start();    // Start continuous processing
    void stop();     // Stop processing
    
    [[nodiscard]] bool is_running() const;
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace alia::audio
```

---

## DSP Building Blocks

### Filters

```cpp
namespace alia::audio::dsp {

// Filter base class
class Filter : public IAudioNode {
public:
    virtual void reset() = 0;  // Clear filter state
};

// Biquad filter types
enum class BiquadType {
    LowPass,
    HighPass,
    BandPass,
    Notch,
    AllPass,
    LowShelf,
    HighShelf,
    Peaking,
};

// Biquad filter
class BiquadFilter : public Filter {
public:
    BiquadFilter(BiquadType type, Hz frequency, float q = 0.707f, float gain_db = 0.0f);
    
    void set_type(BiquadType type);
    void set_frequency(Hz freq);
    void set_q(float q);
    void set_gain(float gain_db);  // For shelving/peaking
    
    void recalculate();  // Recalculate coefficients
    void reset() override;
    
    void process(std::span<const float> input, std::span<float> output) override;
    int input_channels() const override { return 1; }
    int output_channels() const override { return 1; }
    SampleRate sample_rate() const override;
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Convenience filter classes
class LowPassFilter : public BiquadFilter {
public:
    LowPassFilter(Hz cutoff, float q = 0.707f);
    void set_cutoff(Hz freq) { set_frequency(freq); }
};

class HighPassFilter : public BiquadFilter {
public:
    HighPassFilter(Hz cutoff, float q = 0.707f);
    void set_cutoff(Hz freq) { set_frequency(freq); }
};

class BandPassFilter : public BiquadFilter {
public:
    BandPassFilter(Hz center, float q = 1.0f);
    void set_center(Hz freq) { set_frequency(freq); }
};

// Parametric EQ band
class ParametricEQ : public Filter {
public:
    struct Band {
        BiquadType type = BiquadType::Peaking;
        Hz frequency = Hz(1000);
        float q = 1.0f;
        float gain_db = 0.0f;
        bool enabled = true;
    };
    
    ParametricEQ(int num_bands = 4);
    
    void set_band(int index, const Band& band);
    Band& get_band(int index);
    const Band& get_band(int index) const;
    
    void reset() override;
    void process(std::span<const float> input, std::span<float> output) override;
    int input_channels() const override { return 1; }
    int output_channels() const override { return 1; }
    SampleRate sample_rate() const override;
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace alia::audio::dsp
```

### Convolution

```cpp
namespace alia::audio::dsp {

// Convolution reverb
class ConvolutionReverb : public Filter {
public:
    ConvolutionReverb();
    
    // Load impulse response
    [[nodiscard]] static ConvolutionReverb load(std::string_view ir_path);
    [[nodiscard]] static ConvolutionReverb from_sample(const Sample& ir);
    
    void set_wet_dry(float wet, float dry);  // Mix ratio
    void set_predelay(double ms);
    
    void reset() override;
    void process(std::span<const float> input, std::span<float> output) override;
    int input_channels() const override;
    int output_channels() const override;
    SampleRate sample_rate() const override;
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace alia::audio::dsp
```

### FFT and STFT

```cpp
namespace alia::audio::dsp {

// FFT size options
enum class FFTSize {
    FFT_256 = 256,
    FFT_512 = 512,
    FFT_1024 = 1024,
    FFT_2048 = 2048,
    FFT_4096 = 4096,
    FFT_8192 = 8192,
};

// Complex number for FFT output
using Complex = std::complex<float>;

inline float magnitude(const Complex& value) { return std::abs(value); }
inline float phase(const Complex& value) { return std::arg(value); }

// FFT processor
class FFT {
public:
    explicit FFT(FFTSize size);
    ~FFT();
    
    // Forward FFT: time domain -> frequency domain
    void forward(std::span<const float> input, std::span<Complex> output);
    
    // Inverse FFT: frequency domain -> time domain
    void inverse(std::span<const Complex> input, std::span<float> output);
    
    [[nodiscard]] int size() const;
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Window functions
enum class WindowType {
    Rectangular,
    Hann,
    Hamming,
    Blackman,
    BlackmanHarris,
    Kaiser,
};

// Generate window function
std::vector<float> make_window(WindowType type, int size, float param = 0.0f);

// Short-Time Fourier Transform
class STFT {
public:
    STFT(FFTSize fft_size, int hop_size, WindowType window = WindowType::Hann);
    ~STFT();
    
    // Process input samples, call callback for each frame
    using FrameCallback = std::function<void(std::span<Complex> frame)>;
    void process(std::span<const float> input, FrameCallback callback);
    
    // Overlap-add synthesis from modified frames
    using SynthesisCallback = std::function<void(std::span<const Complex> frame, 
                                                  std::span<float> output)>;
    void synthesize(std::span<float> output, SynthesisCallback callback);
    
    [[nodiscard]] int fft_size() const;
    [[nodiscard]] int hop_size() const;
    [[nodiscard]] int num_bins() const;  // fft_size / 2 + 1
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace alia::audio::dsp
```

### Envelope and Dynamics

```cpp
namespace alia::audio::dsp {

// ADSR envelope generator
class ADSREnvelope {
public:
    struct Parameters {
        double attack_ms = 10.0;
        double decay_ms = 100.0;
        double sustain = 0.7;      // 0.0 to 1.0
        double release_ms = 200.0;
    };
    
    ADSREnvelope(SampleRate rate, Parameters params = {});
    
    void set_parameters(const Parameters& params);
    void set_attack(double ms);
    void set_decay(double ms);
    void set_sustain(double level);
    void set_release(double ms);
    
    // Trigger note on/off
    void note_on();
    void note_off();
    
    // Generate envelope samples
    float process();  // Single sample
    void process(std::span<float> output);  // Buffer
    
    [[nodiscard]] bool is_active() const;  // Not in idle state
    
    void reset();
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Compressor/limiter
class Compressor : public Filter {
public:
    struct Parameters {
        float threshold_db = -20.0f;
        float ratio = 4.0f;          // e.g., 4:1
        double attack_ms = 10.0;
        double release_ms = 100.0;
        float knee_db = 0.0f;        // Soft knee width
        float makeup_gain_db = 0.0f;
    };
    
    Compressor(SampleRate rate, Parameters params = {});
    
    void set_parameters(const Parameters& params);
    void set_threshold(float db);
    void set_ratio(float ratio);
    void set_attack(double ms);
    void set_release(double ms);
    void set_knee(float db);
    void set_makeup_gain(float db);
    
    // Get current gain reduction (for metering)
    [[nodiscard]] float get_gain_reduction_db() const;
    
    void reset() override;
    void process(std::span<const float> input, std::span<float> output) override;
    int input_channels() const override { return 1; }
    int output_channels() const override { return 1; } 
    SampleRate sample_rate() const override;
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Hard/soft limiter
class Limiter : public Filter {
public:
    Limiter(SampleRate rate, float ceiling_db = -0.1f);
    
    void set_ceiling(float db);
    void set_release(double ms);
    
    [[nodiscard]] float get_gain_reduction_db() const;
    
    void reset() override;
    void process(std::span<const float> input, std::span<float> output) override;
    int input_channels() const override { return 1; }
    int output_channels() const override { return 1; }
    SampleRate sample_rate() const override;
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace alia::audio::dsp
```

---

## Ring Buffer

### Thread-Safe Ring Buffer

```cpp
namespace alia::audio {

// Lock-free ring buffer for audio data
template<typename T>
class RingBuffer {
public:
    explicit RingBuffer(size_t capacity);
    ~RingBuffer();
    
    // Non-copyable, movable
    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;
    RingBuffer(RingBuffer&&) noexcept;
    RingBuffer& operator=(RingBuffer&&) noexcept;
    
    // Properties
    [[nodiscard]] size_t capacity() const;
    [[nodiscard]] size_t size() const;       // Current number of items
    [[nodiscard]] size_t available() const;  // Space available for writing
    [[nodiscard]] bool empty() const;
    [[nodiscard]] bool full() const;
    
    // Write operations (producer)
    bool push(const T& item);
    bool push_data(std::span<const T> data);
    
    // Read operations (consumer)
    bool pop(T& item);
    bool pop_into(std::span<T> buffer);
    
    // Peek without removing
    bool peek(T& item) const;
    
    // Clear all data
    void clear();
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Common instantiations
using FloatRingBuffer = RingBuffer<float>;
using StereoRingBuffer = RingBuffer<Vec2<float>>;

} // namespace alia::audio
```

---

## Utilities

### Channel Conversion

```cpp
namespace alia::audio {

// Convert stereo to separate mono channels
template<typename T>
bool unzip_channels(std::span<const Vec2<T>> stereo,
                    std::span<T> left,
                    std::span<T> right);

// Combine separate mono channels to stereo
template<typename T>
bool zip_channels(std::span<const T> left,
                  std::span<const T> right,
                  std::span<Vec2<T>> stereo);

// Sample rate conversion
class SampleRateConverter {
public:
    enum class Quality {
        Fast,       // Linear interpolation
        Medium,     // Polynomial interpolation
        High,       // Sinc interpolation
    };
    
    SampleRateConverter(SampleRate from, SampleRate to, 
                        int channels = 2,
                        Quality quality = Quality::Medium);
    ~SampleRateConverter();
    
    // Process samples
    template<FragmentType FragT>
    size_t process(std::span<const FragT> input, std::span<FragT> output);
    
    // Calculate required output buffer size
    [[nodiscard]] size_t output_size_for(size_t input_size) const;
    
    void reset();
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Format conversion
template<typename FromT, typename ToT>
void convert_samples(std::span<const FromT> input, std::span<ToT> output);

// Peak detection
template<FragmentType FragT>
float calculate_peak(std::span<const FragT> buffer);

// RMS calculation
template<FragmentType FragT>
float calculate_rms(std::span<const FragT> buffer);

// Peak to dB
inline float peak_to_db(float peak) {
    return 20.0f * std::log10(std::max(peak, 1e-10f));
}

} // namespace alia::audio
```

---

## Backend Interface

### Audio Backend Interface

```cpp
namespace alia::detail {

// Audio backend interface
class IAudioBackend {
public:
    virtual ~IAudioBackend() = default;
    
    // Lifecycle
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual bool is_valid() const = 0;
    
    // Device enumeration
    virtual std::vector<audio::DeviceInfo> enumerate_devices(audio::DeviceType type) = 0;
    virtual audio::DeviceInfo get_default_device(audio::DeviceType type) = 0;
    
    // Device operations
    virtual void* open_device(const audio::DeviceInfo& info, 
                               double sample_rate, 
                               int channels) = 0;
    virtual void close_device(void* handle) = 0;
    
    // Stream operations
    virtual void* create_stream(void* device, int buffer_size, int num_buffers) = 0;
    virtual void destroy_stream(void* handle) = 0;
    virtual void start_stream(void* handle) = 0;
    virtual void stop_stream(void* handle) = 0;
    
    // Mixer operations
    virtual void* create_mixer(double sample_rate, int channels) = 0;
    virtual void destroy_mixer(void* handle) = 0;
    virtual void attach_to_mixer(void* mixer, void* source) = 0;
    virtual void detach_from_mixer(void* mixer, void* source) = 0;
    
    // Sample operations
    virtual void* load_sample(const char* path) = 0;
    virtual void* load_sample_memory(const void* data, size_t size) = 0;
    virtual void destroy_sample(void* handle) = 0;
    
    // Playback
    virtual void play_sample(void* sample, float gain, float speed, float pan) = 0;
    
    // Backend info
    virtual const char* get_name() const = 0;
};

// Backend registration
struct AudioBackendInfo {
    const char* name;
    uint32_t id;
    bool (*is_available)();
    IAudioBackend* (*create)();
    void (*destroy)(IAudioBackend*);
};

void register_audio_backend(const AudioBackendInfo& info);
std::span<const AudioBackendInfo> get_audio_backends();
IAudioBackend* get_current_audio_backend();
bool switch_audio_backend(uint32_t backend_id);

} // namespace alia::detail
```

---

## Example Usage

### Simple Audio Playback

```cpp
#include <alia/audio/audio.hpp>

int main() {
    using namespace alia::audio;
    using namespace alia::audio::literals;
    
    // Initialize audio
    alia::audio::init();
    
    // Load and play a sound effect
    auto explosion = Sample::load("explosion.wav");
    explosion.play();
    
    // Stream background music
    auto music = load_audio_stream("music.ogg");
    music.set_play_mode(PlayMode::Loop);
    music.set_gain(0.6f);
    music.play();
    
    // Main loop...
}
```

### Real-Time Audio Processing

```cpp
#include <alia/audio/audio.hpp>
#include <alia/audio/dsp/dsp.hpp>

void audio_thread_example() {
    using namespace alia::audio;
    using namespace alia::audio::dsp;
    
    // Create audio stream for custom output
    StereoStream stream(44100_Hz, {.num_chunks = 4, .fragments_per_chunk = 1024});
    
    // Create DSP chain
    LowPassFilter lpf(2000_Hz);
    
    // Oscillator state
    double phase = 0.0;
    const double freq = 440.0;
    const double sample_rate = 44100.0;
    
    // Set callback to generate audio
    stream.set_chunk_callback([&](std::span<Vec2<float>> buffer) {
        for (auto& frag : buffer) {
            // Generate sine wave
            float sample = std::sin(phase * 2.0 * M_PI) * 0.5f;
            phase += freq / sample_rate;
            if (phase >= 1.0) phase -= 1.0;
            
            // Stereo output
            frag = {sample, sample};
        }
    });
    
    stream.play();
}
```

### Audio Recording with Processing

```cpp
#include <alia/audio/audio.hpp>

void recording_example() {
    using namespace alia::audio;
    
    // Create recorder
    StereoRecorder recorder(44100_Hz);
    
    // Ring buffer for communication between threads
    RingBuffer<Vec2<float>> ring_buffer(44100 * 2);  // 2 seconds
    
    // Metering
    std::atomic<float> left_peak = 0.0f, right_peak = 0.0f;
    
    // Set callback to process recorded audio
    recorder.set_chunk_callback([&](std::span<const Vec2<float>> buffer) {
        // Calculate peaks
        std::vector<float> left(buffer.size()), right(buffer.size());
        unzip_channels(buffer, left, right);
        
        left_peak = calculate_peak(std::span{left});
        right_peak = calculate_peak(std::span{right});
        
        // Store in ring buffer for later use
        ring_buffer.push_data(buffer);
    });
    
    recorder.start();
    
    // ... main loop, display peaks, etc.
}
```
