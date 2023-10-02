# wx_gl_shadertoy_tutorial

A [ShaderToy](https://www.shadertoy.com/) clone... but on Desktop!

![App Window on Linux](/shadertoy.png)

This simple app automatically compiles your GLSL fragment shader code and displays the result on the screen. Built with wxWidgets, the app works on Windows, Mac, and Linux.

## How this works

We use modern CMake's `FetchContent` functionality to download `wxWidgets` and `GLEW` (the latter is needed to resolve OpenGL function addresses).

## Requirements

This works on Windows, Mac, and Linux. You'll need `cmake` and a C++ compiler (tested on `clang`, `gcc`, and MSVC).

Linux builds require the GTK3 library and headers installed in the system. You can install them on Ubuntu using:

```sh
sudo apt install libgtk-3-dev
```

OpenGL tutorials require OpenGL development package on Linux (Windows and Mac should work out of the box):

```sh
sudo apt install libglu1-mesa-dev
```

## Building

To build the project, use:

```bash
cmake -S. -Bbuild
cmake --build build -j
```

This will create a directory named `build` and create all build artifacts there. The main executable can be found directly in the `build/` folder.

---
Check out the blog for more! [www.justdevtutorials.com](https://www.justdevtutorials.com)
---

