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

// Frequency (hz)
class hz {
public:
    constexpr hz() = default;
    constexpr explicit hz(double value) : value_(value) {}
    
    [[nodiscard]] constexpr double get() const { return value_; }
    [[nodiscard]] constexpr double get_hz() const { return value_; }
    [[nodiscard]] constexpr double get_khz() const { return value_ / 1000.0; }
    
    // Conversion to period
    [[nodiscard]] constexpr double period_seconds() const { return 1.0 / value_; }
    [[nodiscard]] constexpr double period_ms() const { return 1000.0 / value_; }
    
    // Arithmetic
    constexpr hz operator+(hz rhs) const { return hz(value_ + rhs.value_); }
    constexpr hz operator-(hz rhs) const { return hz(value_ - rhs.value_); }
    constexpr hz operator*(double scalar) const { return hz(value_ * scalar); }
    constexpr hz operator/(double scalar) const { return hz(value_ / scalar); }
    
    constexpr bool operator==(hz rhs) const = default;
    constexpr auto operator<=>(hz rhs) const = default;
    
private:
    double value_ = 0.0;
};

// User-defined literals
namespace literals {
    constexpr hz operator""_Hz(long double val) { return hz(static_cast<double>(val)); }
    constexpr hz operator""_Hz(unsigned long long val) { return hz(static_cast<double>(val)); }
    constexpr hz operator""_kHz(long double val) { return hz(static_cast<double>(val) * 1000.0); }
    constexpr hz operator""_kHz(unsigned long long val) { return hz(static_cast<double>(val) * 1000.0); }
}

// Decibels
class db {
public:
    constexpr db() = default;
    constexpr explicit db(double value) : value_(value) {}
    
    [[nodiscard]] constexpr double get() const { return value_; }
    
    // Convert to linear gain
    [[nodiscard]] double to_linear() const { 
        return std::pow(10.0, value_ / 20.0); 
    }
    
    // Create from linear gain
    [[nodiscard]] static db from_linear(double linear) {
        return db(20.0 * std::log10(linear));
    }
    
    // Arithmetic
    constexpr db operator+(db rhs) const { return db(value_ + rhs.value_); }
    constexpr db operator-(db rhs) const { return db(value_ - rhs.value_); }
    
private:
    double value_ = 0.0;
};

namespace literals {
    constexpr db operator""_dB(long double val) { return db(static_cast<double>(val)); }
    constexpr db operator""_dB(unsigned long long val) { return db(static_cast<double>(val)); }
}

// sample rate type
using sample_rate = hz;

// Common sample rates
namespace sample_rates {
    inline constexpr sample_rate cd = hz(44100);
    inline constexpr sample_rate dvd = hz(48000);
    inline constexpr sample_rate hi_res = hz(96000);
    inline constexpr sample_rate ultra_hi_res = hz(192000);
}

} // namespace alia::audio
```

### Channel Configuration

```cpp
namespace alia::audio {

// Channel configuration tags
struct mono {};
struct stereo {};
struct surround51 {};
struct surround71 {};

// Channel count trait
template<typename Config>
struct channel_count;

template<> struct channel_count<mono> { static constexpr int value = 1; };
template<> struct channel_count<stereo> { static constexpr int value = 2; };
template<> struct channel_count<surround51> { static constexpr int value = 6; };
template<> struct channel_count<surround71> { static constexpr int value = 8; };

template<typename Config>
inline constexpr int channel_count_v = channel_count<Config>::value;

// Audio depth (bit depth)
enum class sample_format {
    int16,      // 16-bit signed integer
    int24,      // 24-bit signed integer (packed)
    int32,      // 32-bit signed integer
    float32,    // 32-bit floating point
    float64,    // 64-bit floating point
};

// sample format traits
template<sample_format F>
struct sample_format_traits;

template<> struct sample_format_traits<sample_format::int16> {
    using type = int16_t;
    static constexpr int bits = 16;
    static constexpr double max_value = 32767.0;
};

template<> struct sample_format_traits<sample_format::int32> {
    using type = int32_t;
    static constexpr int bits = 32;
    static constexpr double max_value = 2147483647.0;
};

template<> struct sample_format_traits<sample_format::float32> {
    using type = float;
    static constexpr int bits = 32;
    static constexpr double max_value = 1.0;
};

template<> struct sample_format_traits<sample_format::float64> {
    using type = double;
    static constexpr int bits = 64;
    static constexpr double max_value = 1.0;
};

} // namespace alia::audio
```

### Fragment Types

```cpp
namespace alia::audio {

// fragment type for N channels
template<typename sample_t, int num_channels>
struct fragment {
    std::array<sample_t, num_channels> channels;
    
    sample_t& operator[](int i) { return channels[i]; }
    const sample_t& operator[](int i) const { return channels[i]; }
    
    // Arithmetic
    fragment operator+(const fragment& rhs) const {
        fragment result;
        for (int i = 0; i < num_channels; ++i)
            result.channels[i] = channels[i] + rhs.channels[i];
        return result;
    }
    
    fragment operator*(sample_t scalar) const {
        fragment result;
        for (int i = 0; i < num_channels; ++i)
            result.channels[i] = channels[i] * scalar;
        return result;
    }
    
    // Mix (add)
    fragment& operator+=(const fragment& rhs) {
        for (int i = 0; i < num_channels; ++i)
            channels[i] += rhs.channels[i];
        return *this;
    }
};

// Specialization: mono (single sample)
template<typename sample_t>
using mono_fragment = sample_t;

// Specialization: stereo (vec2)
template<typename sample_t>
using stereo_fragment = vec2<sample_t>;

// Type alias helpers
using fragment_mono = float;
using fragment_stereo = vec2<float>;
using fragment_surround51 = fragment<float, 6>;
using fragment_surround71 = fragment<float, 8>;

// fragment type from channel config
template<typename sample_t, typename channel_config>
struct fragment_type;

template<typename sample_t>
struct fragment_type<sample_t, mono> { using type = sample_t; };

template<typename sample_t>
struct fragment_type<sample_t, stereo> { using type = vec2<sample_t>; };

template<typename sample_t, typename Config>
using fragment_t = typename fragment_type<sample_t, Config>::type;

} // namespace alia::audio
```

---

## Concepts

### Audio Fragment Concepts

```cpp
namespace alia::audio {

// Check if T is a valid sample type
template<typename T>
concept sample_type = 
    std::same_as<T, float> ||
    std::same_as<T, double> ||
    std::same_as<T, int16_t> ||
    std::same_as<T, int32_t>;

// Check if T is a valid mono fragment (just a sample)
template<typename T>
concept mono_fragment_type = sample_type<T>;

// Check if T is a valid stereo fragment
template<typename T>
concept stereo_fragment_type = 
    requires(T frag) {
        { frag.x } -> sample_type;
        { frag.y } -> sample_type;
    } ||
    (requires(T frag) {
        { frag[0] } -> sample_type;
        { frag[1] } -> sample_type;
    } && std::tuple_size_v<T> == 2);

// Check if T is a valid multi-channel fragment
template<typename T>
concept multi_channel_fragment_type = 
    requires(T frag, int i) {
        { frag[i] } -> sample_type;
    };

// Any valid fragment type
template<typename T>
concept fragment_type = 
    mono_fragment_type<T> || 
    stereo_fragment_type<T> || 
    multi_channel_fragment_type<T>;

// Get channel count from fragment type
template<typename T>
constexpr int fragment_channels() {
    if constexpr (mono_fragment_type<T>) {
        return 1;
    } else if constexpr (stereo_fragment_type<T>) {
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
concept compatible_fragments = 
    fragment_type<T> && fragment_type<U> &&
    (fragment_channels<T>() == fragment_channels<U>());

} // namespace alia::audio
```

### Audio Source Concepts

```cpp
namespace alia::audio {

// Something that produces audio fragments
template<typename T, typename frag_t>
concept audio_source = requires(T source, std::span<frag_t> buffer) {
    { source.read(buffer) } -> std::convertible_to<size_t>;
    { source.sample_rate() } -> std::convertible_to<hz>;
};

// Something that consumes audio fragments
template<typename T, typename frag_t>
concept audio_sink = requires(T sink, std::span<const frag_t> buffer) {
    { sink.write(buffer) } -> std::convertible_to<size_t>;
    { sink.sample_rate() } -> std::convertible_to<hz>;
};

// Something that can be played
template<typename T>
concept playable = requires(T obj) {
    obj.play();
    obj.stop();
    obj.pause();
    { obj.is_playing() } -> std::convertible_to<bool>;
};

// Something with adjustable gain
template<typename T>
concept has_gain = requires(T obj, float gain) {
    obj.set_gain(gain);
    { obj.get_gain() } -> std::convertible_to<float>;
};

// Something with adjustable pan (stereo position)
template<typename T>
concept has_pan = requires(T obj, float pan) {
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

// device type
enum class device_type {
    Output,     // Playback device
    Input,      // Recording device
};

// device info
struct device_info {
    std::string name;
    std::string id;
    device_type type;
    int max_channels;
    std::vector<sample_rate> supported_rates;
    bool is_default;
};

// Audio device
class device {
public:
    device() = default;
    device(device&&) noexcept;
    device& operator=(device&&) noexcept;
    ~device();
    
    // Open default device
    [[nodiscard]] static device open_default(device_type type, 
                                              sample_rate rate = sample_rates::cd,
                                              int channels = 2);
    
    // Open specific device
    [[nodiscard]] static device open(const device_info& info,
                                      sample_rate rate,
                                      int channels);
    
    // Enumerate available devices
    [[nodiscard]] static std::vector<device_info> enumerate(device_type type);
    
    // Get default device info
    [[nodiscard]] static device_info get_default(device_type type);
    
    [[nodiscard]] bool valid() const;
    [[nodiscard]] explicit operator bool() const { return valid(); }
    
    // Properties
    [[nodiscard]] sample_rate sample_rate() const;
    [[nodiscard]] int channels() const;
    [[nodiscard]] device_type type() const;
    [[nodiscard]] const device_info& info() const;
    
    // Latency
    [[nodiscard]] double latency_seconds() const;
    [[nodiscard]] int latency_samples() const;
    
    // Backend handle
    [[nodiscard]] void* native_handle() const;
    
private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

// Get the default output device (singleton for convenience)
device& get_default_device();

} // namespace alia::audio
```

---

## Sample

### Sample Class (Loaded Audio Data)

```cpp
namespace alia::audio {

// Play mode
enum class play_mode {
    Once,       // Play once and stop
    Loop,       // Loop infinitely
    BiDir,      // Ping-pong loop
};

// sample (pre-loaded audio data in memory)
class sample {
public:
    sample() = default;
    sample(sample&&) noexcept;
    sample& operator=(sample&&) noexcept;
    ~sample();
    
    // Load from file
    [[nodiscard]] static sample load(std::string_view path);
    
    // Load from memory
    [[nodiscard]] static sample load(std::span<const std::byte> data);
    
    // Create from raw audio data
    template<fragment_type frag_t>
    [[nodiscard]] static sample from_data(std::span<const frag_t> fragments, sample_rate rate);
    
    [[nodiscard]] bool valid() const;
    [[nodiscard]] explicit operator bool() const { return valid(); }
    
    // Properties
    [[nodiscard]] sample_rate sample_rate() const;
    [[nodiscard]] int channels() const;
    [[nodiscard]] size_t fragment_count() const;
    [[nodiscard]] double duration_seconds() const;
    
    // Quick play (fire and forget, uses default mixer)
    void play(float gain = 1.0f, float speed = 1.0f, float pan = 0.0f) const;
    void play_looped(float gain = 1.0f, float speed = 1.0f) const;
    
    // Get raw data (for custom processing)
    template<fragment_type frag_t>
    [[nodiscard]] std::span<const frag_t> data() const;
    
    // Backend handle
    [[nodiscard]] void* native_handle() const;
    
private:
    struct impl;
    std::unique_ptr<impl> impl_;
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

// stream configuration
struct StreamConfig {
    int num_chunks = 4;          // Number of buffers
    int fragments_per_chunk = 2048;  // Fragments per buffer
};

// Audio stream (for streaming playback, e.g., music)
template<typename sample_t, typename channel_config>
class audio_stream {
public:
    using fragment_type = fragment_t<sample_t, channel_config>;
    
    audio_stream() = default;
    audio_stream(sample_rate rate, StreamConfig config = {});
    
    audio_stream(audio_stream&&) noexcept;
    audio_stream& operator=(audio_stream&&) noexcept;
    ~audio_stream();
    
    // Open from file (streaming, not fully loaded)
    [[nodiscard]] static audio_stream open(std::string_view path);
    [[nodiscard]] static audio_stream open(std::string_view path, StreamConfig config);
    
    [[nodiscard]] bool valid() const;
    [[nodiscard]] explicit operator bool() const { return valid(); }
    
    // Properties
    [[nodiscard]] sample_rate sample_rate() const;
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
    void set_play_mode(play_mode mode);
    void set_gain(float gain);
    void set_speed(float speed);  // 1.0 = normal
    void set_pan(float pan);      // -1.0 to 1.0
    
    [[nodiscard]] float get_gain() const;
    [[nodiscard]] float get_speed() const;
    [[nodiscard]] float get_pan() const;
    [[nodiscard]] play_mode get_play_mode() const;
    
    // For custom streaming: chunk ready callback
    // Called when a chunk needs to be filled with audio data
    using ChunkCallback = std::function<void(std::span<fragment_type> buffer)>;
    void set_chunk_callback(ChunkCallback callback);
    
    // Create event handler for event-driven filling
    // Returns a handler that can be registered with event_dispatcher
    std::function<void(const Event&)> create_chunk_event_handler(ChunkCallback callback);
    
    // Event source for stream events
    event_source& get_event_source();
    
    // Backend handle
    [[nodiscard]] void* native_handle() const;
    
private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

// Common instantiations
using mono_stream = audio_stream<float, mono>;
using stereo_stream = audio_stream<float, stereo>;
using base_audio_stream = stereo_stream;  // default

// Helper to load an audio stream (auto-detects format)
base_audio_stream load_audio_stream(std::string_view path);

} // namespace alia::audio
```

---

## Audio Recorder

### Recorder Class

```cpp
namespace alia::audio {

// Recorder configuration
struct recorder_config {
    int num_chunks = 16;
    int fragments_per_chunk = 1024;
};

// Audio recorder
template<typename sample_t, typename channel_config>
class audio_recorder {
public:
    using fragment_type = fragment_t<sample_t, channel_config>;
    
    audio_recorder() = default;
    audio_recorder(sample_rate rate, recorder_config config = {});
    
    audio_recorder(audio_recorder&&) noexcept;
    audio_recorder& operator=(audio_recorder&&) noexcept;
    ~audio_recorder();
    
    [[nodiscard]] bool valid() const;
    [[nodiscard]] explicit operator bool() const { return valid(); }
    
    // Properties
    [[nodiscard]] sample_rate sample_rate() const;
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
    event_source& get_event_source();
    
    // Backend handle
    [[nodiscard]] void* native_handle() const;
    
private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

// Common instantiations
using mono_recorder = audio_recorder<float, mono>;
using stereo_recorder = audio_recorder<float, stereo>;

} // namespace alia::audio
```

---

## Mixer

### Mixer Class

```cpp
namespace alia::audio {

// mixer (combines multiple audio sources)
template<typename sample_t, typename channel_config>
class mixer {
public:
    using fragment_type = fragment_t<sample_t, channel_config>;
    
    mixer() = default;
    mixer(sample_rate rate);
    
    mixer(mixer&&) noexcept;
    mixer& operator=(mixer&&) noexcept;
    ~mixer();
    
    [[nodiscard]] bool valid() const;
    [[nodiscard]] explicit operator bool() const { return valid(); }
    
    // Properties
    [[nodiscard]] sample_rate sample_rate() const;
    [[nodiscard]] int channels() const;
    
    // Attach sources
    void attach(audio_stream<sample_t, channel_config>& stream);
    void attach(sample& sample);  // Creates internal sample instance
    void attach(mixer& sub_mixer);
    
    // Detach sources
    void detach(audio_stream<sample_t, channel_config>& stream);
    void detach(sample& sample);
    void detach(mixer& sub_mixer);
    
    // Master gain
    void set_gain(float gain);
    [[nodiscard]] float get_gain() const;
    
    // Post-process callback (for effects, metering, etc.)
    using PostProcessCallback = std::function<void(std::span<fragment_type> buffer)>;
    void set_postprocess_callback(PostProcessCallback callback);
    
    // Backend handle
    [[nodiscard]] void* native_handle() const;
    
private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

// Common instantiations
using mono_mixer = mixer<float, mono>;
using stereo_mixer = mixer<float, stereo>;
using user_mixer = stereo_mixer;

// default mixer (singleton, attached to default device)
stereo_mixer& get_default_mixer();

} // namespace alia::audio
```

---

## Voice

### Voice Class (Individual Sound Instance)

```cpp
namespace alia::audio {

// voice (represents a single playing sound)
class voice {
public:
    voice() = default;
    voice(voice&&) noexcept;
    voice& operator=(voice&&) noexcept;
    ~voice();
    
    // Create attached to default device
    voice(sample_rate rate);
    
    [[nodiscard]] bool valid() const;
    [[nodiscard]] explicit operator bool() const { return valid(); }
    
    // Properties
    [[nodiscard]] sample_rate sample_rate() const;
    [[nodiscard]] int channels() const;
    
    // Attach mixer or stream to voice
    void attach(stereo_mixer& mixer);
    
    // Backend handle
    [[nodiscard]] void* native_handle() const;
    
private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

} // namespace alia::audio
```

---

## Audio Pipeline

### Pipeline (Audio Flow Graph)

```cpp
namespace alia::audio {

// Audio node interface
class i_audio_node {
public:
    virtual ~i_audio_node() = default;
    
    virtual void process(std::span<const float> input, std::span<float> output) = 0;
    virtual int input_channels() const = 0;
    virtual int output_channels() const = 0;
    virtual sample_rate sample_rate() const = 0;
};

// pipeline connection
struct pipeline_connection {
    i_audio_node* source;
    int source_output;
    i_audio_node* dest;
    int dest_input;
};

// Audio pipeline (flow graph)
class pipeline {
public:
    pipeline() = default;
    pipeline(sample_rate rate);
    
    pipeline(pipeline&&) noexcept;
    pipeline& operator=(pipeline&&) noexcept;
    ~pipeline();
    
    [[nodiscard]] bool valid() const;
    
    // add nodes
    template<typename node_t, typename... Args>
    node_t& add_node(Args&&... args);
    
    void remove_node(i_audio_node& node);
    
    // Connect nodes
    void connect(i_audio_node& source, i_audio_node& dest);
    void connect(i_audio_node& source, int output, i_audio_node& dest, int input);
    
    // Disconnect
    void disconnect(i_audio_node& source, i_audio_node& dest);
    void disconnect_all(i_audio_node& node);
    
    // Set output (final destination)
    void set_output(device& device);
    void set_output(mixer<float, stereo>& mixer);
    
    // Processing
    void process();  // Process one buffer
    void start();    // start continuous processing
    void stop();     // Stop processing
    
    [[nodiscard]] bool is_running() const;
    
private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

} // namespace alia::audio
```

---

## DSP Building Blocks

### Filters

```cpp
namespace alia::audio::dsp {

// filter base class
class filter : public i_audio_node {
public:
    virtual void reset() = 0;  // Clear filter state
};

// Biquad filter types
enum class biquad_type {
    low_pass,
    high_pass,
    band_pass,
    notch,
    all_pass,
    low_shelf,
    high_shelf,
    peaking,
};

// Biquad filter
class biquad_filter : public filter {
public:
    biquad_filter(biquad_type type, hz frequency, float q = 0.707f, float gain_db = 0.0f);
    
    void set_type(biquad_type type);
    void set_frequency(hz freq);
    void set_q(float q);
    void set_gain(float gain_db);  // For shelving/peaking
    
    void recalculate();  // Recalculate coefficients
    void reset() override;
    
    void process(std::span<const float> input, std::span<float> output) override;
    int input_channels() const override { return 1; }
    int output_channels() const override { return 1; }
    sample_rate sample_rate() const override;
    
private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

// Convenience filter classes
class low_pass_filter : public biquad_filter {
public:
    low_pass_filter(hz cutoff, float q = 0.707f);
    void set_cutoff(hz freq) { set_frequency(freq); }
};

class high_pass_filter : public biquad_filter {
public:
    high_pass_filter(hz cutoff, float q = 0.707f);
    void set_cutoff(hz freq) { set_frequency(freq); }
};

class band_pass_filter : public biquad_filter {
public:
    band_pass_filter(hz center, float q = 1.0f);
    void set_center(hz freq) { set_frequency(freq); }
};

// Parametric EQ band
class parametric_eq : public filter {
public:
    struct band {
        biquad_type type = biquad_type::peaking;
        hz frequency = hz(1000);
        float q = 1.0f;
        float gain_db = 0.0f;
        bool enabled = true;
    };
    
    parametric_eq(int num_bands = 4);
    
    void set_band(int index, const band& band);
    band& get_band(int index);
    const band& get_band(int index) const;
    
    void reset() override;
    void process(std::span<const float> input, std::span<float> output) override;
    int input_channels() const override { return 1; }
    int output_channels() const override { return 1; }
    sample_rate sample_rate() const override;
    
private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

} // namespace alia::audio::dsp
```

### Convolution

```cpp
namespace alia::audio::dsp {

// Convolution reverb
class convolution_reverb : public filter {
public:
    convolution_reverb();
    
    // Load impulse response
    [[nodiscard]] static convolution_reverb load(std::string_view ir_path);
    [[nodiscard]] static convolution_reverb from_sample(const sample& ir);
    
    void set_wet_dry(float wet, float dry);  // Mix ratio
    void set_predelay(double ms);
    
    void reset() override;
    void process(std::span<const float> input, std::span<float> output) override;
    int input_channels() const override;
    int output_channels() const override;
    sample_rate sample_rate() const override;
    
private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

} // namespace alia::audio::dsp
```

### FFT and STFT

```cpp
namespace alia::audio::dsp {

// fft size options
enum class fft_size {
    fft_256 = 256,
    fft_512 = 512,
    fft_1024 = 1024,
    fft_2048 = 2048,
    fft_4096 = 4096,
    fft_8192 = 8192,
};

// complex number for fft output
using complex = std::complex<float>;

inline float magnitude(const complex& value) { return std::abs(value); }
inline float phase(const complex& value) { return std::arg(value); }

// fft processor
class fft {
public:
    explicit fft(fft_size size);
    ~fft();
    
    // Forward fft: time domain -> frequency domain
    void forward(std::span<const float> input, std::span<complex> output);
    
    // Inverse fft: frequency domain -> time domain
    void inverse(std::span<const complex> input, std::span<float> output);
    
    [[nodiscard]] int size() const;
    
private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

// window functions
enum class window_type {
    rectangular,
    hann,
    hamming,
    blackman,
    blackman_harris,
    kaiser,
};

// Generate window function
std::vector<float> make_window(window_type type, int size, float param = 0.0f);

// Short-Time Fourier transform
class stft {
public:
    stft(fft_size fft_size, int hop_size, window_type window = window_type::hann);
    ~stft();
    
    // Process input samples, call callback for each frame
    using FrameCallback = std::function<void(std::span<complex> frame)>;
    void process(std::span<const float> input, FrameCallback callback);
    
    // Overlap-add synthesis from modified frames
    using SynthesisCallback = std::function<void(std::span<const complex> frame, 
                                                  std::span<float> output)>;
    void synthesize(std::span<float> output, SynthesisCallback callback);
    
    [[nodiscard]] int fft_size() const;
    [[nodiscard]] int hop_size() const;
    [[nodiscard]] int num_bins() const;  // fft_size / 2 + 1
    
private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

} // namespace alia::audio::dsp
```

### Envelope and Dynamics

```cpp
namespace alia::audio::dsp {

// ADSR envelope generator
class adsr_envelope {
public:
    struct Parameters {
        double attack_ms = 10.0;
        double decay_ms = 100.0;
        double sustain = 0.7;      // 0.0 to 1.0
        double release_ms = 200.0;
    };
    
    adsr_envelope(sample_rate rate, Parameters params = {});
    
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
    struct impl;
    std::unique_ptr<impl> impl_;
};

// compressor/limiter
class compressor : public filter {
public:
    struct Parameters {
        float threshold_db = -20.0f;
        float ratio = 4.0f;          // e.g., 4:1
        double attack_ms = 10.0;
        double release_ms = 100.0;
        float knee_db = 0.0f;        // Soft knee width
        float makeup_gain_db = 0.0f;
    };
    
    compressor(sample_rate rate, Parameters params = {});
    
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
    sample_rate sample_rate() const override;
    
private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

// Hard/soft limiter
class limiter : public filter {
public:
    limiter(sample_rate rate, float ceiling_db = -0.1f);
    
    void set_ceiling(float db);
    void set_release(double ms);
    
    [[nodiscard]] float get_gain_reduction_db() const;
    
    void reset() override;
    void process(std::span<const float> input, std::span<float> output) override;
    int input_channels() const override { return 1; }
    int output_channels() const override { return 1; }
    sample_rate sample_rate() const override;
    
private:
    struct impl;
    std::unique_ptr<impl> impl_;
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
class ring_buffer {
public:
    explicit ring_buffer(size_t capacity);
    ~ring_buffer();
    
    // Non-copyable, movable
    ring_buffer(const ring_buffer&) = delete;
    ring_buffer& operator=(const ring_buffer&) = delete;
    ring_buffer(ring_buffer&&) noexcept;
    ring_buffer& operator=(ring_buffer&&) noexcept;
    
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
    struct impl;
    std::unique_ptr<impl> impl_;
};

// Common instantiations
using float_ring_buffer = ring_buffer<float>;
using stereo_ring_buffer = ring_buffer<vec2<float>>;

} // namespace alia::audio
```

---

## Utilities

### Channel Conversion

```cpp
namespace alia::audio {

// Convert stereo to separate mono channels
template<typename T>
bool unzip_channels(std::span<const vec2<T>> stereo,
                    std::span<T> left,
                    std::span<T> right);

// Combine separate mono channels to stereo
template<typename T>
bool zip_channels(std::span<const T> left,
                  std::span<const T> right,
                  std::span<vec2<T>> stereo);

// sample rate conversion
class sample_rate_converter {
public:
    enum class quality {
        fast,       // Linear interpolation
        medium,     // Polynomial interpolation
        high,       // Sinc interpolation
    };
    
    sample_rate_converter(sample_rate from, sample_rate to, 
                        int channels = 2,
                        quality quality = quality::medium);
    ~sample_rate_converter();
    
    // Process samples
    template<fragment_type frag_t>
    size_t process(std::span<const frag_t> input, std::span<frag_t> output);
    
    // Calculate required output buffer size
    [[nodiscard]] size_t output_size_for(size_t input_size) const;
    
    void reset();
    
private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

// Format conversion
template<typename FromT, typename ToT>
void convert_samples(std::span<const FromT> input, std::span<ToT> output);

// Peak detection
template<fragment_type frag_t>
float calculate_peak(std::span<const frag_t> buffer);

// RMS calculation
template<fragment_type frag_t>
float calculate_rms(std::span<const frag_t> buffer);

// Peak to db
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
class i_audio_backend {
public:
    virtual ~i_audio_backend() = default;
    
    // Lifecycle
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual bool is_valid() const = 0;
    
    // device enumeration
    virtual std::vector<audio::device_info> enumerate_devices(audio::device_type type) = 0;
    virtual audio::device_info get_default_device(audio::device_type type) = 0;
    
    // device operations
    virtual void* open_device(const audio::device_info& info, 
                               double sample_rate, 
                               int channels) = 0;
    virtual void close_device(void* handle) = 0;
    
    // stream operations
    virtual void* create_stream(void* device, int buffer_size, int num_buffers) = 0;
    virtual void destroy_stream(void* handle) = 0;
    virtual void start_stream(void* handle) = 0;
    virtual void stop_stream(void* handle) = 0;
    
    // mixer operations
    virtual void* create_mixer(double sample_rate, int channels) = 0;
    virtual void destroy_mixer(void* handle) = 0;
    virtual void attach_to_mixer(void* mixer, void* source) = 0;
    virtual void detach_from_mixer(void* mixer, void* source) = 0;
    
    // sample operations
    virtual void* load_sample(const char* path) = 0;
    virtual void* load_sample_memory(const void* data, size_t size) = 0;
    virtual void destroy_sample(void* handle) = 0;
    
    // Playback
    virtual void play_sample(void* sample, float gain, float speed, float pan) = 0;
    
    // Backend info
    virtual const char* get_name() const = 0;
};

// Backend registration
struct audio_backend_info {
    const char* name;
    uint32_t id;
    bool (*is_available)();
    i_audio_backend* (*create)();
    void (*destroy)(i_audio_backend*);
};

void register_audio_backend(const audio_backend_info& info);
std::span<const audio_backend_info> get_audio_backends();
i_audio_backend* get_current_audio_backend();
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
    auto explosion = sample::load("explosion.wav");
    explosion.play();
    
    // stream background music
    auto music = load_audio_stream("music.ogg");
    music.set_play_mode(play_mode::Loop);
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
    stereo_stream stream(44100_Hz, {.num_chunks = 4, .fragments_per_chunk = 1024});
    
    // Create DSP chain
    low_pass_filter lpf(2000_Hz);
    
    // Oscillator state
    double phase = 0.0;
    const double freq = 440.0;
    const double sample_rate = 44100.0;
    
    // Set callback to generate audio
    stream.set_chunk_callback([&](std::span<vec2<float>> buffer) {
        for (auto& frag : buffer) {
            // Generate sine wave
            float sample = std::sin(phase * 2.0 * M_PI) * 0.5f;
            phase += freq / sample_rate;
            if (phase >= 1.0) phase -= 1.0;
            
            // stereo output
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
    stereo_recorder recorder(44100_Hz);
    
    // Ring buffer for communication between threads
    ring_buffer<vec2<float>> ring_buffer(44100 * 2);  // 2 seconds
    
    // Metering
    std::atomic<float> left_peak = 0.0f, right_peak = 0.0f;
    
    // Set callback to process recorded audio
    recorder.set_chunk_callback([&](std::span<const vec2<float>> buffer) {
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
