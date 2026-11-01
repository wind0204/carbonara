# Introduction
It is a WIP RPG game framework being worked on by an amateur hobbyist developer.
I worked on it for a while and rendered a rotating pyramid and a square tile under it with textures on them using Vulkan in 2025, and dropped the project.
And I've recently picked it up again.

## Goals
- Make a game framework that can be used to build a feature-rich 3D RPG
- Learn to write in C23 first, and probably adopt C2y in the future
- Target linux PC's first in the beginning, because my PC runs a Linux system
- The minimum CPU requirement is AVX2 and LZCNT

## Here's the roadmap(can be changed):
- Implement a basic job-stealing multi-threaded app (implemented)
- Implement basic 3D RPG features
- Start using box3d
- Implement a multiplayer game mode that is robust enough for action games
- Utilize CPU instructions that are available only on newer processors
- Port the Linux version either to a console, or to Mac and Windows 11 after having achieved a decent set of features!

## I'm currently using(can be changed):
- vulkan
- SDL3
- simde
- hwloc v2.15
- volk
- the standard C library

<br>

# Build Instructions
## Prepare SDL3
- clone the [git repo](https://github.com/libsdl-org/SDL) and place it in `extern/SDL`
- install the development headers of liburing (e.g. liburing-dev on Ubuntu)

## Prepare hwloc
- clone the [git repo](https://github.com/open-mpi/hwloc) and place it in `extern/hwloc`
- check out `v2.15` tag in the git repo

## Configure cmake builds for debugging and releasing
```sh
CC=clang-21 cmake -B build-Debug -DCMAKE_BUILD_TYPE=Debug -G Ninja
CC=clang-21 cmake -B build-RelWithDebInfo -DCMAKE_BUILD_TYPE=RelWithDebInfo -G Ninja
CC=clang-21 cmake -B build-ship_it -DCMAKE_BUILD_TYPE=Release -G Ninja
```

## Start building a cmake build
```sh
time cmake --build build-Debug
```

## Prepare SPIR-V code files in `spirv` directory (TODO: include it in the cmake configuration)
```sh
dxc -spirv -T vs_6_8 -Zpr -enable-16bit-types -fvk-use-scalar-layout -E vert_main src/hlsl/depth_only_prepass.hlsl -Fo spirv/depth_only_prepass.hlsl.vert.spv
dxc -spirv -T ps_6_8 -Zpr -enable-16bit-types -fvk-use-scalar-layout -E frag_main src/hlsl/depth_only_prepass.hlsl -Fo spirv/depth_only_prepass.hlsl.frag.spv
dxc -spirv -T vs_6_8 -Zpr -enable-16bit-types -fvk-use-scalar-layout -E vert_main src/hlsl/simple_test.hlsl -Fo spirv/simple_test.hlsl.vert.spv
dxc -spirv -T ps_6_8 -Zpr -enable-16bit-types -fvk-use-scalar-layout -E frag_main src/hlsl/simple_test.hlsl -Fo spirv/simple_test.hlsl.frag.spv
dxc -spirv -T vs_6_8 -Zpr -enable-16bit-types -fvk-use-scalar-layout -E vert_main src/hlsl/copy_to_swapchain_img.hlsl -Fo spirv/copy_to_swapchain_img.hlsl.vert.spv
dxc -spirv -T ps_6_8 -Zpr -enable-16bit-types -fvk-use-scalar-layout -E frag_main src/hlsl/copy_to_swapchain_img.hlsl -Fo spirv/copy_to_swapchain_img.hlsl.frag.spv
```

## Prepare 12-mip-level albedo maps in .dds files
- assets/textures/stone-01.dds
- assets/textures/sand-01.dds

## You can get the list of the compile commands and provide it to your LSP
```sh
ln -s build-Debug/compile_commands.json .
```

<br>

# How to run
## Run the binary when .dds files and .spv files are ready
```sh
build-Debug/Debug/carbonara
```
