# ALIA - Advanced Layer for Interactive Applications

## Overview

ALIA is a modern C++23 multimedia library providing low-level access to graphics, audio, and input subsystems while maximizing developer experience through zero-cost abstractions and modern language features.

### Design Philosophy

1. **99% Rule**: Everything can be highly configurable, but always provide a simple default for common use cases
2. **Hot-Swappable Backends**: No "restart required" messages—all backends can be reloaded at runtime
3. **Modern C++23**: Leverage concepts, ranges, constexpr, and other features for better APIs
4. **Separation of Concerns**: User-facing APIs are ergonomic; backend interfaces are implementation-friendly

---

## Dependencies

### Required
- **C++23 Compiler**: GCC 13+, Clang 17+, MSVC 19.37+
- **CMake 3.28+**: For build configuration

### Optional (Platform-Specific)
- **Windows**: Direct3D 9/11 SDK, DirectInput, XAudio2
- **Linux**: X11/Wayland dev libs, ALSA/PulseAudio, OpenGL
- **macOS**: Metal framework, CoreAudio

### Optional (Features)
- **libsndfile**: Extended audio format support
- **FreeType**: TrueType font rendering
- **stb_image**: Image loading (header-only, bundled)
- **FFmpeg/libav**: Video playback

---

## Module Organization

```
alia/
├── core/           # Fundamental types and utilities
├── os/             # Operating system abstractions
├── gfx/            # Graphics subsystem
├── io/             # Input devices
├── audio/          # Audio subsystem
├── pixel/          # Pixel formats and colorspaces
├── font/           # Font loading and rendering
├── image/          # Image loading
├── video/          # Video playback
└── detail/         # Backend interfaces (internal)
```

---

## File Structure

### Root Headers
```
include/alia/
├── alia.hpp                    # Master include
├── fwd.hpp                     # Forward declarations
├── version.hpp                 # Version macros
└── config.hpp                  # Compile-time configuration
```

### Core Module (`core/`)
```
include/alia/core/
├── core.hpp                    # Core module header
├── vec.hpp                     # Vec2<T>, Vec3<T>, Vec4<T>
├── rect.hpp                    # Rect<T> with full utility methods
├── mat.hpp                     # Mat3<T>, Mat4<T> matrices
├── color.hpp                   # Color type with format conversions
├── span.hpp                    # Extended span utilities
├── result.hpp                  # Result<T, E> for error handling
├── handle.hpp                  # RAII handle wrapper
├── time.hpp                    # Time utilities, durations
├── events.hpp                  # Event types and IDs
├── event_queue.hpp             # Event queue abstraction
└── vfs.hpp                     # Virtual filesystem (future)

src/core/
├── events.cpp                  # Event ID counter, registration
├── event_queue.cpp             # Event queue implementation
└── time.cpp                    # Platform time queries
```

### OS Module (`os/`)
```
include/alia/os/
├── os.hpp                      # OS module header
├── window.hpp                  # Window creation and management
├── display.hpp                 # Display modes, fullscreen
├── dialog.hpp                  # Native dialogs (file, message)
├── clipboard.hpp               # Clipboard access
└── monitor.hpp                 # Monitor enumeration

src/os/
├── window_win32.cpp            # Win32 window backend
├── window_x11.cpp              # X11 window backend
├── window_wayland.cpp          # Wayland window backend
├── display_win32.cpp           # Win32 display modes
├── display_x11.cpp             # X11 display modes
├── dialog_win32.cpp            # Win32 dialogs
├── dialog_x11.cpp              # X11/GTK dialogs
├── clipboard_win32.cpp         # Win32 clipboard
├── clipboard_x11.cpp           # X11 clipboard
└── monitor.cpp                 # Monitor enumeration
```

### Graphics Module (`gfx/`)
```
include/alia/gfx/
├── gfx.hpp                     # Graphics module header
├── bitmap.hpp                  # Bitmap/texture abstraction
├── surface.hpp                 # Render target surfaces
├── primitive.hpp               # Shape drawing, custom vertices
├── transform.hpp               # 2D/3D transforms
├── blend.hpp                   # Blend modes
├── vertex.hpp                  # Vertex declarations
├── backend.hpp                 # Backend selection/management
├── capabilities.hpp            # Feature flag queries
│
├── classic/                    # Software/legacy rendering
│   ├── classic.hpp             # Classic renderer header
│   ├── renderer.hpp            # Classic renderer interface
│   └── blitter.hpp             # Blit operations
│
├── accelerated/                # Hardware-accelerated 2D/3D
│   ├── accelerated.hpp         # Accelerated module header
│   ├── renderer.hpp            # Accelerated renderer
│   ├── shader.hpp              # Shader programs
│   ├── texture.hpp             # GPU textures
│   ├── framebuffer.hpp         # Framebuffer objects
│   ├── pipeline.hpp            # Render pipeline state
│   ├── buffer.hpp              # Vertex/Index buffers
│   └── uniform.hpp             # Uniform buffer objects
│
└── modern/                     # Vulkan/DX12 (future)
    └── modern.hpp              # Modern API placeholder

src/gfx/
├── bitmap.cpp                  # Bitmap common code
├── primitive.cpp               # Primitive drawing
├── transform.cpp               # Transform utilities
├── backend.cpp                 # Backend management
├── capabilities.cpp            # Capability queries
│
├── backends/
│   ├── software/
│   │   ├── sw_renderer.cpp     # Software renderer
│   │   └── sw_blitter.cpp      # Software blitting
│   │
│   ├── d3d9/
│   │   ├── d3d9_renderer.cpp   # D3D9 renderer
│   │   ├── d3d9_shader.cpp     # D3D9 shaders (HLSL)
│   │   ├── d3d9_texture.cpp    # D3D9 textures
│   │   └── d3d9_buffer.cpp     # D3D9 buffers
│   │
│   ├── d3d11/
│   │   ├── d3d11_renderer.cpp  # D3D11 renderer
│   │   ├── d3d11_shader.cpp    # D3D11 shaders
│   │   ├── d3d11_texture.cpp   # D3D11 textures
│   │   └── d3d11_buffer.cpp    # D3D11 buffers
│   │
│   └── opengl/
│       ├── gl_renderer.cpp     # OpenGL renderer
│       ├── gl_shader.cpp       # OpenGL shaders (GLSL)
│       ├── gl_texture.cpp      # OpenGL textures
│       └── gl_buffer.cpp       # OpenGL buffers
```

### Input Module (`io/`)
```
include/alia/io/
├── io.hpp                      # IO module header
├── keyboard.hpp                # Keyboard input
├── mouse.hpp                   # Mouse input
├── joystick.hpp                # Joystick/gamepad input
├── touch.hpp                   # Touch input
└── keycodes.hpp                # Platform-independent keycodes

src/io/
├── keyboard_win32.cpp          # Win32/DirectInput keyboard
├── keyboard_x11.cpp            # X11 keyboard
├── mouse_win32.cpp             # Win32/DirectInput mouse
├── mouse_x11.cpp               # X11 mouse
├── joystick_win32.cpp          # DirectInput/XInput joystick
├── joystick_linux.cpp          # evdev joystick
└── touch.cpp                   # Touch input
```

### Audio Module (`audio/`)
```
include/alia/audio/
├── audio.hpp                   # Audio module header
├── device.hpp                  # Audio device abstraction
├── sample.hpp                  # Audio sample data
├── stream.hpp                  # Audio streaming
├── voice.hpp                   # Audio voice/channel
├── mixer.hpp                   # Audio mixing
├── pipeline.hpp                # Audio flow graphs
├── recorder.hpp                # Audio recording
│
└── dsp/
    ├── dsp.hpp                 # DSP module header
    ├── filter.hpp              # LP, HP, parametric EQ
    ├── convolution.hpp         # Convolution reverb
    ├── fft.hpp                 # FFT/STFT utilities
    └── envelope.hpp            # ADSR envelopes

src/audio/
├── device.cpp                  # Common device code
├── sample.cpp                  # Sample loading/conversion
├── stream.cpp                  # Stream management
├── mixer.cpp                   # Software mixing
├── pipeline.cpp                # Audio graph processing
│
├── backends/
│   ├── xaudio2/
│   │   ├── xa2_device.cpp      # XAudio2 device
│   │   └── xa2_voice.cpp       # XAudio2 voices
│   │
│   ├── wasapi/
│   │   └── wasapi_device.cpp   # WASAPI device
│   │
│   ├── alsa/
│   │   └── alsa_device.cpp     # ALSA device
│   │
│   ├── pulseaudio/
│   │   └── pulse_device.cpp    # PulseAudio device
│   │
│   └── coreaudio/
│       └── ca_device.cpp       # CoreAudio device
│
└── dsp/
    ├── filter.cpp              # Filter implementations
    ├── convolution.cpp         # Convolution impl
    └── fft.cpp                 # FFT implementation
```

### Pixel Module (`pixel/`)
```
include/alia/pixel/
├── pixel.hpp                   # Pixel module header
├── format.hpp                  # Pixel format definitions
├── convert.hpp                 # Format conversion
└── colorspace.hpp              # Colorspace utilities

src/pixel/
├── convert.cpp                 # Conversion routines
└── colorspace.cpp              # Colorspace transforms
```

### Font Module (`font/`)
```
include/alia/font/
├── font.hpp                    # Font module header
├── ttf.hpp                     # TrueType fonts
├── bitmap_font.hpp             # Bitmap fonts
└── render.hpp                  # Text rendering

src/font/
├── ttf.cpp                     # FreeType integration
├── bitmap_font.cpp             # Bitmap font loading
└── render.cpp                  # Text rendering
```

### Image Module (`image/`)
```
include/alia/image/
├── image.hpp                   # Image module header
├── loader.hpp                  # Image loader interface
└── formats.hpp                 # Format-specific loaders

src/image/
├── loader.cpp                  # Loader registry
├── png.cpp                     # PNG loader
├── jpeg.cpp                    # JPEG loader
├── bmp.cpp                     # BMP loader
└── stb_impl.cpp                # stb_image implementation
```

### Video Module (`video/`)
```
include/alia/video/
├── video.hpp                   # Video module header
├── player.hpp                  # Video player
└── decoder.hpp                 # Decoder interface

src/video/
├── player.cpp                  # Video player logic
└── ffmpeg_decoder.cpp          # FFmpeg decoder
```

### Detail/Backend Interfaces (`detail/`)
```
include/alia/detail/
├── backend_interface.hpp       # Base backend interface
├── gfx_backend.hpp             # Graphics backend interface
├── audio_backend.hpp           # Audio backend interface
├── window_backend.hpp          # Window backend interface
├── resource_tracker.hpp        # Resource tracking for hot-swap
└── type_erasure.hpp            # Type erasure utilities
```

---

## Backend Architecture

### Backend Interface Requirements

Backend interfaces must:
1. Be implementable in a separate translation unit (no templates in interface)
2. Use type erasure (`void*`, type IDs) where necessary
3. Support resource enumeration for hot-swap
4. Provide capability flags

### Backend Registry

```cpp
namespace alia::detail {
    
    enum class BackendType : uint32_t {
        Software,
        D3D9,
        D3D11,
        OpenGL,
        Vulkan,
        Metal
    };
    
    struct BackendInfo {
        BackendType type;
        const char* name;
        uint32_t version;
        bool (*is_available)();
        void* (*create_instance)();
        void (*destroy_instance)(void*);
    };
    
    // Registry functions (implemented in backend.cpp)
    void register_backend(const BackendInfo& info);
    std::span<const BackendInfo> get_available_backends();
    BackendInfo* get_current_backend();
    
}
```

### Hot-Swap Mechanism

```cpp
namespace alia::gfx {
    
    // Resource tracking for hot-swap
    class ResourceTracker {
    public:
        // Register a resource for tracking
        template<typename T>
        ResourceHandle track(T* resource, ResourceData data);
        
        // Enumerate all tracked resources
        void enumerate(std::function<void(ResourceHandle, const ResourceData&)> callback);
        
        // Transfer resources to new backend
        void transfer_to(BackendInstance* new_backend);
    };
    
    // Backend switching
    bool switch_backend(BackendType new_backend);
    
}
```

### D3D9 Backend Specifics

**Window Integration:**
- D3D9 requires a window handle (HWND) at device creation
- Device is created with `D3DCREATE_HARDWARE_VERTEXPROCESSING`
- Fullscreen mode uses `D3DPRESENT_PARAMETERS` with windowed=FALSE

**Resource Management:**
- Textures: `IDirect3DTexture9` with D3DPOOL_MANAGED or D3DPOOL_DEFAULT
- Vertex Buffers: `IDirect3DVertexBuffer9`
- Index Buffers: `IDirect3DIndexBuffer9`
- Shaders: `ID3DXEffect` for combined vertex/pixel shaders

**State Management:**
- Transform matrices via `SetTransform()`
- Blend states via `SetRenderState()`
- Texture sampling via `SetSamplerState()`

**Hot-Swap Considerations:**
- D3DPOOL_MANAGED resources survive device reset
- D3DPOOL_DEFAULT resources must be recreated on reset/backend switch
- Keep CPU-side copies for non-managed resources

### OpenGL Backend Specifics

**Context Creation:**
- Use WGL on Windows, GLX on Linux
- Support OpenGL versions down to 1.1
- Throw exceptions when unsupported features are used (e.g., VBOs on 1.x)
- Provide backend feature flags for testing
- Enable debug context in debug builds

**Resource Management:**
- Textures: `glGenTextures`, `glTexImage2D`
- Vertex Buffers: `glGenBuffers`, `GL_ARRAY_BUFFER`
- Index Buffers: `glGenBuffers`, `GL_ELEMENT_ARRAY_BUFFER`
- Shaders: `glCreateShader`, `glCreateProgram`
- Framebuffers: `glGenFramebuffers`

**State Management:**
- Use VAOs for vertex format state
- Minimize state changes via state caching
- Track bound textures per texture unit

---

## Event System

### Event Type IDs

```cpp
namespace alia {
    
    using EventTypeID = uint32_t;
    
    // Reserved ranges:
    // 0x00000 - 0xFFFFF (0 - 1048575): Built-in events
    // 0x100000 - 0x1FFFFF: Auto-assigned user events (via get_user_event_id<T>())
    // 0x200000+: User-defined static event IDs (user responsibility to avoid conflicts)
    
    namespace detail {
        // Check if an event type ID is in the reserved range
        consteval bool is_reserved_event_id(EventTypeID id) {
            return id < 0x200000;
        }
        
        // Compile-time validation for user-defined event IDs
        template<EventTypeID ID>
        consteval void validate_user_event_id() {
            static_assert(!is_reserved_event_id(ID), 
                "User-defined event IDs must be >= 0x200000");
        }
    }
    
    // User event ID generation (auto-assigned from 0x100000 range)
    template<typename T>
    EventTypeID get_user_event_id() {
        // If T has a static TypeID member, use it (with compile-time validation)
        if constexpr (requires { T::TypeID; }) {
            detail::validate_user_event_id<T::TypeID>();
            return T::TypeID;
        } else {
            // Auto-assign from the 0x100000 range
            static EventTypeID id = detail::allocate_user_event_id();
            return id;
        }
    }
    
}
```

**Note:** Event type IDs are defined alongside their respective event structures (see Event Structures section and individual subsystem documentation).

### Event Structures

```cpp
namespace alia {
    
    // Event header (common metadata)
    struct EventHeader {
        EventTypeID type;
        double timestamp;
        void* source;
    };
    
    // Templated event wrapper - enables emit({aggregate init}) syntax
    template<typename TEventData>
    struct Event {
        using DataType = TEventData;
        
        EventHeader header;
        TEventData data;
        
        // Convenience accessors
        [[nodiscard]] EventTypeID type() const { return header.type; }
        [[nodiscard]] double timestamp() const { return header.timestamp; }
        [[nodiscard]] void* source() const { return header.source; }
    };
    
    // Type-erased event for storage/queuing
    class AnyEvent {
    public:
        [[nodiscard]] EventTypeID type() const;
        [[nodiscard]] double timestamp() const;
        [[nodiscard]] void* source() const;
        
        template<typename T>
        [[nodiscard]] const T& get() const;
        
        template<typename T>
        [[nodiscard]] const T* try_get() const;
    };
    
    // Event data structures (used with Event<T>)
    
    // Keyboard event data
    struct KeyEventData {
        static constexpr EventTypeID KeyDownID = 100;
        static constexpr EventTypeID KeyUpID = 101;
        
        int keycode;
        int scancode;
        uint32_t modifiers;
        bool repeat;
    };
    using KeyDownEvent = Event<KeyEventData>;
    using KeyUpEvent = Event<KeyEventData>;
    
    struct CharEventData {
        static constexpr EventTypeID TypeID = 102;
        
        char32_t codepoint;
        uint32_t modifiers;
    };
    using CharEvent = Event<CharEventData>;
    
    // Mouse event data
    struct MouseMoveEventData {
        static constexpr EventTypeID TypeID = 200;
        
        Vec2f position;
        Vec2f delta;
    };
    using MouseMoveEvent = Event<MouseMoveEventData>;
    
    struct MouseButtonEventData {
        static constexpr EventTypeID ButtonDownID = 201;
        static constexpr EventTypeID ButtonUpID = 202;
        
        Vec2f position;
        int button;
    };
    using MouseButtonDownEvent = Event<MouseButtonEventData>;
    using MouseButtonUpEvent = Event<MouseButtonEventData>;
    
    struct MouseWheelEventData {
        static constexpr EventTypeID TypeID = 203;
        
        Vec2f position;
        float delta_y;
        float delta_x;  // Horizontal scroll
    };
    using MouseWheelEvent = Event<MouseWheelEventData>;
    
    // Window event data
    struct WindowCloseEventData {
        static constexpr EventTypeID TypeID = 1;
        
        Window* window;
    };
    using WindowCloseEvent = Event<WindowCloseEventData>;
    
    struct WindowResizeEventData {
        static constexpr EventTypeID TypeID = 2;
        
        Window* window;
        Vec2i old_size;
        Vec2i new_size;
    };
    using WindowResizeEvent = Event<WindowResizeEventData>;
    
    struct WindowFocusEventData {
        static constexpr EventTypeID TypeID = 3;
        
        Window* window;
        bool focused;
    };
    using WindowFocusEvent = Event<WindowFocusEventData>;
    
}
```

---

## Capability/Feature Flags

```cpp
namespace alia::gfx {
    
    enum class Capability : uint32_t {
        // Texture capabilities
        TextureNPOT = 1 << 0,           // Non-power-of-two textures
        TextureFloat = 1 << 1,          // Float textures
        TextureDepth = 1 << 2,          // Depth textures
        TextureArray = 1 << 3,          // Texture arrays
        Texture3D = 1 << 4,             // 3D textures
        TextureCube = 1 << 5,           // Cubemap textures
        
        // Shader capabilities
        ShaderVertex = 1 << 8,          // Vertex shaders
        ShaderPixel = 1 << 9,           // Pixel/fragment shaders
        ShaderGeometry = 1 << 10,       // Geometry shaders
        ShaderCompute = 1 << 11,        // Compute shaders
        
        // Rendering capabilities
        Framebuffer = 1 << 16,          // Render to texture
        MultipleRenderTargets = 1 << 17, // MRT
        Instancing = 1 << 18,           // Hardware instancing
        IndirectDraw = 1 << 19,         // Indirect drawing
        
        // Blend capabilities
        BlendSeparate = 1 << 24,        // Separate RGB/A blending
        BlendDualSource = 1 << 25,      // Dual source blending
    };
    
    // Query capabilities
    bool has_capability(Capability cap);
    uint32_t get_capabilities();
    
    // Limits
    struct Limits {
        int max_texture_size;
        int max_texture_units;
        int max_render_targets;
        int max_vertex_attributes;
        int max_uniform_block_size;
    };
    
    const Limits& get_limits();
    
}
```

---

## Example Usage

### Basic Window and Rendering

```cpp
#include <alia/alia.hpp>

int main() {
    // Initialize with automatic backend selection
    alia::init();
    
    // Create a window (simple API) - accepts Vec2i or separate width/height
    auto window = alia::Window({800, 600}, "My Game");
    
    // Load a bitmap using free function (image format registry handled internally)
    auto sprite = alia::load_bitmap("player.png");
    
    // Main loop
    alia::EventQueue events;
    bool running = true;
    
    while (running) {
        // Process events
        while (auto event = events.poll()) {
            if (event.type() == alia::WindowCloseEventData::TypeID) {
                running = false;
            }
        }
        
        // Clear and draw
        alia::clear(alia::Color::CornflowerBlue);
        alia::draw(sprite, {100, 100});
        
        // Present
        window.flip();
    }
}
```

### Audio Playback

```cpp
#include <alia/audio/audio.hpp>

int main() {
    alia::audio::init();
    
    // Load and play a sound effect using free functions
    // (load_sample and load_stream are in audio_files.cpp, pulls in libsndfile)
    auto sfx = alia::audio::load_sample("explosion.wav");
    sfx.play();
    
    // Stream background music
    auto music = alia::audio::load_stream("bgm.ogg");
    music.set_looping(true);
    music.play();
    
    // Process audio events...
}
```

### Custom Vertex Format

```cpp
#include <alia/gfx/gfx.hpp>

// Vertex types must be standard layout
struct MyVertex {
    alia::Vec3f pos;
    alia::Vec2f uv;
    alia::Color color;
};
static_assert(std::is_standard_layout_v<MyVertex>);

// Register vertex format with ALIA using offsetof (no UB)
template<>
struct alia::VertexTraits<MyVertex> {
    static constexpr auto attributes = alia::vertex_attrs(
        alia::VertexAttribute{alia::VertexSemantic::Position, alia::VertexAttrType::Float3, offsetof(MyVertex, pos)},
        alia::VertexAttribute{alia::VertexSemantic::TexCoord, alia::VertexAttrType::Float2, offsetof(MyVertex, uv)},
        alia::VertexAttribute{alia::VertexSemantic::Color, alia::VertexAttrType::Float4, offsetof(MyVertex, color)}
    );
};

void render() {
    std::vector<MyVertex> vertices = {
        {{0, 0, 0}, {0, 0}, alia::Color::Red},
        {{1, 0, 0}, {1, 0}, alia::Color::Green},
        {{0, 1, 0}, {0, 1}, alia::Color::Blue},
    };
    
    alia::draw_triangles(vertices);
}
```

### Event Handling with Custom Events

```cpp
#include <alia/core/events.hpp>

// Define a custom event
struct PlayerDied {
    int player_id;
    alia::Vec2f position;
};

void game_logic() {
    alia::EventQueue queue;
    alia::UserEventSource my_source;
    queue.register_source(my_source);
    
    // Emit custom event
    my_source.emit(PlayerDied{.player_id = 1, .position = {100, 200}});
    
    // Handle events
    while (auto event = queue.poll()) {
        if (event->type == alia::get_user_event_id<PlayerDied>()) {
            auto& data = alia::get_event_data<PlayerDied>(*event);
            // Handle player death...
        }
    }
}
```

### Backend Hot-Swap

```cpp
#include <alia/gfx/backend.hpp>

void settings_menu() {
    auto backends = alia::gfx::get_available_backends();
    
    // Display available backends to user
    for (const auto& backend : backends) {
        if (ui_button(backend.name)) {
            // Switch backend - all resources automatically transferred
            alia::gfx::switch_backend(backend.type);
            // No restart required!
        }
    }
}
```

### Audio DSP Pipeline

```cpp
#include <alia/audio/pipeline.hpp>
#include <alia/audio/dsp/dsp.hpp>

void setup_audio() {
    using namespace alia::audio;
    
    // Create audio graph
    auto music = Stream::open("music.ogg");
    auto lowpass = dsp::LowPassFilter(2000.0f);  // 2kHz cutoff
    auto reverb = dsp::ConvolutionReverb::load("hall.wav");
    
    // Connect nodes
    Pipeline pipeline;
    pipeline.connect(music, lowpass);
    pipeline.connect(lowpass, reverb);
    pipeline.connect(reverb, get_default_device());
    
    music.play();
}
```

---

## Thread Safety

### Rules

1. **Single-threaded by default**: Main graphics/window operations must be on main thread
2. **Audio is thread-safe**: Audio mixing and streaming run on dedicated threads
3. **Event queues are thread-safe**: Events can be posted from any thread
4. **Resource creation**: Most resources should be created on main thread
5. **Backend switch**: Must be called from main thread, blocks until complete

### Recommended Architecture

```
Main Thread:
  - Window management
  - Event processing  
  - Graphics rendering
  - Backend switching

Audio Thread (internal):
  - Audio mixing
  - DSP processing
  - Device output

User Threads:
  - Event posting
  - Resource loading (async)
  - Game logic
```

---

## Error Handling

ALIA uses exceptions for unrecoverable errors and `std::expected<T, E>` for recoverable errors.

```cpp
namespace alia {
    
    // Exception hierarchy
    struct Exception : std::runtime_error {
        using std::runtime_error::runtime_error;
    };
    
    struct InitError : Exception { };
    struct ResourceError : Exception { };
    struct BackendError : Exception { };
    struct ShaderError : Exception { };
    struct AudioError : Exception { };
    
    // Use std::expected for recoverable errors (C++23)
    template<typename T, typename E = std::string>
    using Result = std::expected<T, E>;
    
    // Convenience alias for common error type
    template<typename T>
    using Expected = std::expected<T, std::string>;
    
}
```

---

## Build Configuration

### CMake Options

```cmake
option(ALIA_BUILD_SHARED "Build shared library" ON)
option(ALIA_BUILD_EXAMPLES "Build examples" ON)
option(ALIA_BUILD_TESTS "Build tests" OFF)

# Backend options
option(ALIA_ENABLE_D3D9 "Enable Direct3D 9 backend" ON)
option(ALIA_ENABLE_D3D11 "Enable Direct3D 11 backend" ON)
option(ALIA_ENABLE_OPENGL "Enable OpenGL backend" ON)
option(ALIA_ENABLE_VULKAN "Enable Vulkan backend" OFF)

# Feature options
option(ALIA_ENABLE_AUDIO "Enable audio subsystem" ON)
option(ALIA_ENABLE_VIDEO "Enable video playback" OFF)
option(ALIA_ENABLE_TTF "Enable TrueType fonts" ON)

# Audio backend options
option(ALIA_AUDIO_XAUDIO2 "Enable XAudio2" ON)
option(ALIA_AUDIO_WASAPI "Enable WASAPI" OFF)
option(ALIA_AUDIO_ALSA "Enable ALSA" ON)
option(ALIA_AUDIO_PULSEAUDIO "Enable PulseAudio" ON)
```

### Compile-Time Configuration

```cpp
// include/alia/config.hpp

#ifndef ALIA_CONFIG_HPP
#define ALIA_CONFIG_HPP

// Platform detection
#if defined(_WIN32)
    #define ALIA_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #define ALIA_PLATFORM_LINUX 1
#elif defined(__APPLE__)
    #define ALIA_PLATFORM_MACOS 1
#endif

// Backend availability (set by CMake)
#cmakedefine ALIA_HAS_D3D9
#cmakedefine ALIA_HAS_D3D11
#cmakedefine ALIA_HAS_OPENGL
#cmakedefine ALIA_HAS_VULKAN

// Feature availability
#cmakedefine ALIA_HAS_AUDIO
#cmakedefine ALIA_HAS_VIDEO
#cmakedefine ALIA_HAS_TTF

#endif // ALIA_CONFIG_HPP
```

---

## Migration from axxegro

### Key Differences

| axxegro | ALIA |
|---------|------|
| `al::Vec<T, N>` (variadic) | `Vec2<T>`, `Vec3<T>`, `Vec4<T>` (separate types) |
| Allegro backend | Native platform backends |
| `al::Display` | `alia::Window` + `alia::gfx::Surface` |
| `al::EventLoop` | `alia::EventQueue` + user main loop |
| `al::Bitmap` | `alia::Bitmap` (similar API, different impl) |
| `al::Format()` | `std::format()` |

### Preserved Patterns

- Rect utility methods (kept verbatim)
- Exception hierarchy macro
- Audio span-based API
- User event registration
- Vertex declaration traits

---

## Future Considerations

### Vulkan/DX
