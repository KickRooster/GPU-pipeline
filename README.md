# GPU Pipeline

A DirectX 12 renderer with Nanite-like virtualized geometry and PBR lighting.

## Features

- **Mesh Shaders**: Hardware-accelerated mesh processing pipeline
- **Nanite Geometry**: Continuous LOD with automatic level selection
- **PBR Rendering**: Image-based lighting with physically-based materials
- **Bindless Textures**: Efficient texture management using descriptor arrays

## Build Requirements

- Windows 10 (20H1 or later) / Windows 11
- Visual Studio 2019 or later
- GPU with Mesh Shader support (NVIDIA RTX series, AMD RDNA2+)

## Getting Started

Clone with submodules:
```bash
git clone --recurse-submodules git@github.com:KickRooster/GPU-pipeline.git
```

Build and run in Visual Studio.

## Third-party Libraries

- [imgui](https://github.com/ocornut/imgui)
- [assimp](https://github.com/assimp/assimp)
- [meshoptimizer](https://github.com/zeux/meshoptimizer)
- [ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo)
- [stb](https://github.com/nothings/stb)
