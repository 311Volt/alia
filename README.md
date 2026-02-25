# alia

Welcome!

This is the **Advanced Layer for Interactive Applications**, or **ALIA** for short. The goal of this library is to deliver a set of C++23 templates for programming interactive applications (particularly video games) in a way that takes maximum advantage of programming techniques made possible by recent C++ standards.

# Status and plans

The project is in a very early stage. Do NOT use it yet.

### Planned backends

  - Graphics
    - DirectX 9
    - OpenGL
    - Software
    - later: modern APIs (D3D12, Vulkan, Metal)
  - OS 
    - Win32
    - Linux/X11
    - macOS
    - Android, iPhone OS?
  - Audio
    - DirectSound
    - ALSA
    - PipeWire

# Goals

 - Maximum portability and compatibility
 - "Simple by default, powerful when needed" as the most important design principle
 - Flexibility: can be configured to use any combination of compatible backends for each module


# Is it for me?

This library is for you if you're looking to:
 - program some games the old-fashioned way
 - make your own game engine
 - test out technical ideas outside of constraints of a game engine
  
ALIA only serves to abstract away the underlying platform and is not a game engine. You are responsible for the structure of your program.

# Examples

TODO
