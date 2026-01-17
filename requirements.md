# alia design requirements scratchpad

## general goals

use the latest released c++ standard (c++23 as of now)

be the be-all end-all of game libraries - send SDL, SFML, Allegro all into obsolescence

if there are multiple approaches to a problem, give the user a choice (but still choose one approach and make it the convenient default)


## backend flexibility

### why

to support the absolute widest possible range of hardware and platforms

### what

provide multiple backends for audio, multiple backends for graphics, etc.

e.g. 
 - gfx -> software, d3d9, d3d11, ogl, vk, etc.
 - audio -> directsound, pulseaudio
 - platform -> win32, x11, mac


### how




## backend capabilities

### what

e.g.
 - platform backends: supports 

### how



## backend runtime/comptime dispatch choice

### why

necessary sacrifice to the gods of zero overhead principle

### what

### how

## graphics tiers


# hypothetical code snippets

```cpp
alia::context ctx("config.toml"); 
auto disp = ctx.create_display({800, 600}, alia::fullscreen);
auto renderer = ctx.create_accelerated_renderer(disp)
```
