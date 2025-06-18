# alia

Welcome!

This is the **Advanced Layer for Interactive Applications**, or **ALIA** for short. The purpose of this library is to deliver a set of C++23 templates for programming interactive applications (particularly video games) in a way that takes maximum advantage of programming techniques made possible by recent C++ standards.

# Status and plans

The project is in a very early stage. Scope is limited and only SDL2 backends are implemented.

### Planned backends

 - ### Graphics + Audio + Platform
   - SDL 1.2, SDL 3.0
   - Allegro 4, Allegro 5
   - SFML

  - ### Graphics (distant future)
    - DirectX 9, 10, 11, 12
    - OpenGL
    - Vulkan



# Goals

 - Maximum portability and compatibility: can be backed by plain platform code (DirectX, OpenGL, etc) as well as any similar library (SDL, SFML, Allegro)
 - Flexibility: can be configured to use any combination of compatible backends for each module
 - API is modelled after SDL
 - Maximum adherence to the zero-overhead principle
 - Maximum adherence to the philosophy of "keep simple things simple"


# Is it for me?

This library is for you if you're looking to:
 - program some games the old-fashioned way
 - make your own game engine
 - test out technical ideas outside of constraints of a game engine
  
ALIA only serves to abstract away the underlying platform and is not a game engine. You are responsible for the structure of your program.

# Examples

TODO
